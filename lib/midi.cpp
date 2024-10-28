// Copyright (C) 2024 by Brenton Bostick
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial
// portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "tbt-parser/rational.h"
#include "tbt-parser/tbt-parser-util.h"
#include "tbt-parser/tbt.h"

#include "common/assert.h"
#include "common/check.h"
#include "common/file.h"
#include "common/logging.h"

#include <algorithm> // for remove
#include <set>
#include <variant> // for get_if
#include <iterator>
#include <cinttypes>
#include <cmath> // for round, floor, fmod
#include <cstring> // for memcmp
#include <cstdio> // for snprintf


#include "last-found.inl"


#define TAG "midi"


#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-emplace"


const rational TBT_TICKS_PER_BEAT = 0xc0; // 192
const rational TBT_TICKS_PER_SPACE = (TBT_TICKS_PER_BEAT / 4); // 48

const rational MICROS_PER_SECOND = 1000000;
const rational MICROS_PER_MINUTE = (MICROS_PER_SECOND * 60);
const rational MICROS_PER_64TH = (MICROS_PER_SECOND / 64);


//
// meta events
//
const uint8_t M_TRACKNAME = 0x03;
const uint8_t M_LYRIC = 0x05;
const uint8_t M_ENDOFTRACK = 0x2f;
const uint8_t M_SETTEMPO = 0x51;
const uint8_t M_TIMESIGNATURE = 0x58;


//
// controller messages
//
const uint8_t C_BANKSELECT_MSB = 0x00;
const uint8_t C_MODULATION = 0x01;
const uint8_t C_DATAENTRY_MSB = 0x06;
const uint8_t C_VOLUME = 0x07;
const uint8_t C_PAN = 0x0a;
const uint8_t C_DATAENTRY_LSB = 0x26;
const uint8_t C_REVERB = 0x5b;
const uint8_t C_CHORUS = 0x5d;
const uint8_t C_RPNPARAM_LSB = 0x64;
const uint8_t C_RPNPARAM_MSB = 0x65;


//
// strings
//
const std::string S_MTHD = "MThd";
const std::string S_MTRK = "MTrk";


//
// resolve all of the Automatically Assign -1 values to actual channels
//
template <uint8_t VERSION, typename tbt_file_t>
void
computeChannelMap(
    const tbt_file_t &t,
    std::map<uint8_t, uint8_t> &channelMap) {

    channelMap.clear();

    //
    // Channel 9 is for drums and is not generally available
    //
    std::vector<uint8_t> availableChannels{ 0, 1, 2, 3, 4, 5, 6, 7, 8, /*9,*/ 10, 11, 12, 13, 14, 15 };

    //
    // first just treat any assigned channels as unavailable
    //
    if constexpr (0x6a <= VERSION) {
        
        for (uint8_t track = 0; track < t.header.trackCount; track++) {

            if (t.metadata.tracks[track].midiChannel == -1) {
                continue;
            }

            //
            // https://en.wikipedia.org/wiki/Erase%E2%80%93remove_idiom
            //
            availableChannels.erase(
                std::remove(
                    availableChannels.begin(),
                    availableChannels.end(),
                    t.metadata.tracks[track].midiChannel
                ),
                availableChannels.end()
            );

            channelMap[track] = static_cast<uint8_t>(t.metadata.tracks[track].midiChannel);
        }
    }

    //
    // availableChannels now holds generally available channels
    //

    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        if constexpr (0x6a <= VERSION) {
            if (t.metadata.tracks[track].midiChannel != -1) {
                continue;
            }
        }

        //
        // Take first available channel
        //

        ASSERT(!availableChannels.empty());
        
        auto firstAvailableChannel = availableChannels[0];
        availableChannels.erase(availableChannels.cbegin() + 0);

        channelMap[track] = firstAvailableChannel;
    }

    //
    // it's ok to have available channels left over
    //
//    if (!availableChannels.empty()) {
//        LOGE("availableChannels is not empty");
//        return TBT_ERR;
//    }
}


void
insertTempoMap_atActualSpace(
    uint16_t newTempo,
    rational actualSpace,
    std::map<uint16_t, std::map<rational, uint16_t> > &tempoMap) {

    auto flooredActualSpace = actualSpace.floor();

    auto flooredActualSpaceI = flooredActualSpace.to_uint16();

    auto spaceDiff = (actualSpace - flooredActualSpace);

    ASSERT(spaceDiff.is_nonnegative());

    if (spaceDiff.is_positive()) {
        LOGW("tempo change at non-integral space: %f", actualSpace.to_double());
    }

    const auto &tempoMapIt = tempoMap.find(flooredActualSpaceI);
    if (tempoMapIt != tempoMap.end()) {

        auto &m = tempoMapIt->second;

        const auto mIt = m.find(actualSpace);
        if (mIt != m.end()) {

            auto &tempo = mIt->second;

            if (tempo != newTempo) {
                LOGW("actualSpace %f has conflicting tempo changes: %d, %d", actualSpace.to_double(), tempo, newTempo);
            }

            tempo = newTempo;

        } else {

            m[actualSpace] = newTempo;
        }

    } else {

        std::map<rational, uint16_t> m;

        m[actualSpace] = newTempo;

        tempoMap[flooredActualSpaceI] = m;
    }
}


template <uint8_t VERSION, bool HASALTERNATETIMEREGIONS, typename tbt_file_t, uint8_t STRINGS_PER_TRACK>
void
computeTempoMap(
    const tbt_file_t &t,
    std::map<uint16_t, std::map<rational, uint16_t> > &tempoMap) {

    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        uint16_t trackSpaceCount;
        if constexpr (0x70 <= VERSION) {
            //
            // stored as 32-bit int, so must be cast
            //
            trackSpaceCount = static_cast<uint16_t>(t.metadata.tracks[track].spaceCount);
        } else if constexpr (VERSION == 0x6f) {
            trackSpaceCount = t.header.spaceCount;
        } else {
            trackSpaceCount = 4000;
        }

        const auto &maps = t.body.mapsList[track];

        rational actualSpace = 0;

        for (uint16_t space = 0; space < trackSpaceCount;) {

            if constexpr (VERSION == 0x72) {

                const auto &it = maps.trackEffectChangesMap.find(space);
                if (it != maps.trackEffectChangesMap.end()) {
                    
                    const auto &changes = it->second;

                    for (const auto &p : changes) {

                        auto effect = p.first;

                        if (effect == TE_TEMPO) {

                            auto newTempo = p.second;

                            insertTempoMap_atActualSpace(newTempo, actualSpace, tempoMap);

                            break;
                        }
                    }
                }

            } else {

                const auto &it = maps.notesMap.find(space);
                if (it != maps.notesMap.end()) {

                    const auto &vsqs = it->second;

                    const auto &trackEffect = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 0];

                    switch (trackEffect) {
                    case 'T': {

                        auto newTempo = static_cast<uint16_t>(vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3]);

                        insertTempoMap_atActualSpace(newTempo, actualSpace, tempoMap);

                        break;
                    }
                    case 't': {

                        auto newTempo = static_cast<uint16_t>(vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3] + 250);

                        insertTempoMap_atActualSpace(newTempo, actualSpace, tempoMap);

                        break;
                    }
                    case '\0':
                    case 'I':
                    case 'V':
                    case 'D':
                    case 'U':
                    case 'C':
                    case 'P':
                    case 'R': {
                        //
                        // skip
                        //
                        break;
                    }
                    default: {
                        ABORT("invalid trackEffect: %c (%d)", trackEffect, trackEffect);
                    }
                    }
                }
            }

            //
            // Compute actual space
            //
            {
                if constexpr (HASALTERNATETIMEREGIONS) {

                    const auto &alternateTimeRegionsIt = maps.alternateTimeRegionsMap.find(space);
                    if (alternateTimeRegionsIt != maps.alternateTimeRegionsMap.end()) {

                        const auto &alternateTimeRegion = alternateTimeRegionsIt->second;

                        auto atr = rational{ alternateTimeRegion[0], alternateTimeRegion[1] };

                        space++;

                        actualSpace += atr;

                    } else {

                        space++;

                        ++actualSpace;
                    }

                } else {

                    space++;

                    actualSpace = space;
                }
            }
        }

    } // for track
}


struct repeat_close_struct {
    uint16_t open;
    uint8_t repeats;
    size_t dataStart;
    size_t dataEnd;
    int jump;
};


struct repeat_open_struct { // NOLINT(*-pro-type-member-init)
    rational actualSpace;
    uint16_t space;
};


template <uint8_t VERSION, typename tbt_file_t>
void
computeRepeats(
    const tbt_file_t &t,
    uint16_t barLinesSpaceCount,
    std::vector<std::set<uint16_t> > &openSpaceSets,
    std::vector<std::map<uint16_t, repeat_close_struct> > &repeatCloseMaps) {

    //
    // Setup repeats
    //

    for (uint8_t track = 0; track < t.header.trackCount + 1; track++) { // track count, + 1 for tempo track

        repeatCloseMaps.push_back( {} );

        openSpaceSets.push_back( {} );
    }

    uint16_t lastOpenSpace = 0;

    bool currentlyOpen = false;
    bool savedClose = false;
    uint8_t savedRepeats = 0;

    for (uint16_t space = 0; space < barLinesSpaceCount;) {

        //
        // Setup repeats:
        // setup repeatCloseMap and openSpaceSet
        //
        if constexpr (0x70 <= VERSION) {

            const auto &barLinesMapIt = t.body.barLinesMap.find(space);
            if (barLinesMapIt != t.body.barLinesMap.end()) {

                //
                // typical bar line is at spaces: 0, 16, 32, etc.
                //
                auto barLine = barLinesMapIt->second;

                if (savedClose) {

                    for (uint8_t track = 0; track < t.header.trackCount + 1; track++) { // track count, + 1 for tempo track

                        if (openSpaceSets[track].find(lastOpenSpace) == openSpaceSets[track].end()) {
                        
                            LOGW("there was no repeat open at %d", lastOpenSpace);
                            
                            openSpaceSets[track].insert(lastOpenSpace);
                        }

                        repeatCloseMaps[track][space] = { lastOpenSpace, savedRepeats, 0, 0, 0 };
                    }

                    savedClose = false;

                    lastOpenSpace = space;
                }

                if ((barLine[0] & CLOSEREPEAT_MASK_GE70) == CLOSEREPEAT_MASK_GE70) {

                    //
                    // save for next bar line
                    //

                    savedClose = true;
                    savedRepeats = barLine[1];

                    currentlyOpen = false;
                }

                if ((barLine[0] & OPENREPEAT_MASK_GE70) == OPENREPEAT_MASK_GE70) {

                    if (currentlyOpen) {

                        LOGW("repeat open at space %d is ignored", lastOpenSpace);

                    } else {

                        currentlyOpen = true;
                    }

                    lastOpenSpace = space;

                    for (uint8_t track = 0; track < t.header.trackCount + 1; track++) { // track count, + 1 for tempo track
                        openSpaceSets[track].insert(lastOpenSpace);
                    }
                }
            }

        } else {

            const auto &barLinesMapIt = t.body.barLinesMap.find(space);
            if (barLinesMapIt != t.body.barLinesMap.end()) {

                //
                // typical CLOSE, SINGLE, DOUBLE is at spaces: 15, 31, etc.
                // typical OPEN is at spaces: 0, 16, 32, etc.
                //
                auto barLine = barLinesMapIt->second;

                auto change = static_cast<tbt_bar_line>(barLine[0] & 0b00001111);

                switch (change) {
                case CLOSE: {
                    
                    auto repeats = static_cast<uint8_t>((barLine[0] & 0b11110000) >> 4);

                    for (uint8_t track = 0; track < t.header.trackCount + 1; track++) { // track count, + 1 for tempo track
                        
                        if (openSpaceSets[track].find(lastOpenSpace) == openSpaceSets[track].end()) {
                        
                            LOGW("there was no repeat open at %d", lastOpenSpace);
                            
                            openSpaceSets[track].insert(lastOpenSpace);
                        }
                        
                        repeatCloseMaps[track][space + 1] = { lastOpenSpace, repeats, 0, 0, 0 };
                    }

                    lastOpenSpace = space + 1;

                    for (uint8_t track = 0; track < t.header.trackCount + 1; track++) { // track count, + 1 for tempo track
                        openSpaceSets[track].insert(lastOpenSpace);
                    }

                    currentlyOpen = false;

                    break;
                }
                case OPEN: {

                    if (currentlyOpen) {

                        LOGW("repeat open at space %d is ignored", lastOpenSpace);

                    } else {

                        currentlyOpen = true;
                    }

                    lastOpenSpace = space;

                    for (uint8_t track = 0; track < t.header.trackCount + 1; track++) { // track count, + 1 for tempo track
                        openSpaceSets[track].insert(lastOpenSpace);
                    }

                    break;
                }
                case SINGLE:
                case DOUBLE:
                    //
                    // nothing to do
                    //
                    break;
                default:
                    ABORT("invalid change: %d", change);
                }
            }
        }

        space++;

    } // for space

    if constexpr (0x70 <= VERSION) {

        //
        // and make sure to handle close repeat at very end of song
        //
        if (savedClose) {

            for (uint8_t track = 0; track < t.header.trackCount + 1; track++) { // track count, + 1 for tempo track

                if (openSpaceSets[track].find(lastOpenSpace) == openSpaceSets[track].end()) {

                    LOGW("there was no repeat open at %d", lastOpenSpace);

                    openSpaceSets[track].insert(lastOpenSpace);
                }

                repeatCloseMaps[track][barLinesSpaceCount] = { lastOpenSpace, savedRepeats, 0, 0, 0 };
            }

            savedClose = false;
        }
    }
}


template <uint8_t VERSION, typename tbt_file_t, uint8_t STRINGS_PER_TRACK>
void
computeMidiNoteOffsetArrays(
    const tbt_file_t &t,
    std::vector<std::array<uint8_t, STRINGS_PER_TRACK> > &midiNoteOffsetArrays) {

    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        const auto &trackMetadata = t.metadata.tracks[track];

        std::array<uint8_t, STRINGS_PER_TRACK> midiNoteOffsetArray; // NOLINT(*-pro-type-member-init)

        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

            auto offset = -0x80;

            offset += trackMetadata.tuning[string];

            if constexpr (0x6e <= VERSION) {
                offset += trackMetadata.transposeHalfSteps;
            }

            if constexpr (0x6b <= VERSION) {
                offset += OPEN_STRING_TO_MIDI_NOTE[string];
            } else {
                offset += OPEN_STRING_TO_MIDI_NOTE_LE6A[string];
            }

            midiNoteOffsetArray[string] = static_cast<uint8_t>(offset);
        }

        midiNoteOffsetArrays.push_back(midiNoteOffsetArray);
    }
}


bool operator==(const ProgramChangeEvent& lhs, const ProgramChangeEvent& rhs) {
    return lhs.deltaTime == rhs.deltaTime && lhs.channel == rhs.channel && lhs.midiProgram == rhs.midiProgram;
}

bool operator==(const PitchBendEvent& lhs, const PitchBendEvent& rhs) {
    return lhs.deltaTime == rhs.deltaTime && lhs.channel == rhs.channel && lhs.pitchBend == rhs.pitchBend;
}

bool operator==(const NoteOffEvent& lhs, const NoteOffEvent& rhs) {
    return lhs.deltaTime == rhs.deltaTime && lhs.channel == rhs.channel && lhs.midiNote == rhs.midiNote && lhs.velocity == rhs.velocity;
}

bool operator==(const NoteOnEvent& lhs, const NoteOnEvent& rhs) {
    return lhs.deltaTime == rhs.deltaTime && lhs.channel == rhs.channel && lhs.midiNote == rhs.midiNote && lhs.velocity == rhs.velocity;
}

bool operator==(const ControlChangeEvent& lhs, const ControlChangeEvent& rhs) {
    return lhs.deltaTime == rhs.deltaTime && lhs.channel == rhs.channel && lhs.controller == rhs.controller && lhs.value == rhs.value;
}

bool operator==(const MetaEvent& lhs, const MetaEvent& rhs) {
    return lhs.deltaTime == rhs.deltaTime && lhs.type == rhs.type && lhs.data == rhs.data;
}

bool operator==(const PolyphonicKeyPressureEvent& lhs, const PolyphonicKeyPressureEvent& rhs) {
    return lhs.deltaTime == rhs.deltaTime && lhs.channel == rhs.channel && lhs.midiNote == rhs.midiNote && lhs.pressure == rhs.pressure;
}

bool operator==(const ChannelPressureEvent& lhs, const ChannelPressureEvent& rhs) {
    return lhs.deltaTime == rhs.deltaTime && lhs.channel == rhs.channel && lhs.pressure == rhs.pressure;
}

bool operator==(const SysExEvent& lhs, const SysExEvent& rhs) {
    return lhs.deltaTime == rhs.deltaTime && lhs.data == rhs.data;
}


template <uint8_t VERSION, bool HASALTERNATETIMEREGIONS, uint8_t STRINGS_PER_TRACK, typename tbt_file_t>
Status
TconvertToMidi(
    const tbt_file_t &t,
    const midi_convert_opts &opts,
    midi_file &out) {

    uint16_t barLinesSpaceCount;
    if constexpr (0x70 <= VERSION) {
        barLinesSpaceCount = t.body.barLinesSpaceCount;
    } else if constexpr (VERSION == 0x6f) {
        barLinesSpaceCount = t.header.spaceCount;
    } else {
        barLinesSpaceCount = 4000;
    }

    //
    // compute tempo map
    //
    // flooredActualSpace -> ( actualSpace -> tempo )
    //
    // there can be more than one tempo change mapped to the same flooredActualSpace
    // so this needs to be a map of actualSpace -> tempo
    //
    // pre-computed
    //
    std::map<uint16_t, std::map<rational, uint16_t> > tempoMap;

    computeTempoMap<VERSION, HASALTERNATETIMEREGIONS, tbt_file_t, STRINGS_PER_TRACK>(t, tempoMap);

    //
    // compute channel map
    //
    // pre-computed
    //
    std::map<uint8_t, uint8_t> channelMap;

    computeChannelMap<VERSION>(t, channelMap);

    //
    // for each track:
    //   set of spaces that repeat opens occur
    //
    // pre-computed
    //
    std::vector<std::set<uint16_t> > openSpaceSets;

    //
    // for each track, including tempo track:
    //   actual space of close -> repeat_close_struct
    //
    // pre-computed
    //
    std::vector<std::map<uint16_t, repeat_close_struct> > repeatCloseMaps;

    computeRepeats<VERSION, tbt_file_t>(t, barLinesSpaceCount, openSpaceSets, repeatCloseMaps);

    //
    // for each track:
    //   string -> offset needed to obtain midi note
    //
    // pre-computed
    //
    std::vector<std::array<uint8_t, STRINGS_PER_TRACK> > midiNoteOffsetArrays;

    computeMidiNoteOffsetArrays<VERSION, tbt_file_t, STRINGS_PER_TRACK>(t, midiNoteOffsetArrays);


    out.header = {
        1, // format
        static_cast<uint16_t>(t.header.trackCount + 1), // track count, + 1 for tempo track
        TBT_TICKS_PER_BEAT.to_uint16() // division
    };


    std::vector<midi_track_event> tmp;

#ifndef NDEBUG
    uint32_t tickCount;
#endif // NDEBUG

    //
    // Track 0
    //
    // will be used for tempo changes exclusively
    //
    {
        //
        // Emit events for tempo track
        //
        
        rational tick = 0;
        
        rational roundedTick = 0;

        rational lastEventTick = 0;

        auto diff = (roundedTick - lastEventTick);

        auto str = std::string("tbt-parser MIDI - Track 0");

        std::vector<uint8_t> trackNameData{ str.cbegin(), str.cend() };

        tmp.push_back(MetaEvent{
            diff.to_int32(), // delta time
            M_TRACKNAME,
            trackNameData
        });

        lastEventTick = roundedTick;

        diff = (roundedTick - lastEventTick);

        std::vector<uint8_t> timeSignatureData {
            4, // numerator
            2, // denominator (as 2^d)
            24, // ticks per metronome click
            8 // notated 32-notes in MIDI quarter notes
        };

        tmp.push_back(MetaEvent{
            diff.to_int32(), // delta time
            M_TIMESIGNATURE,
            timeSignatureData
        });

        lastEventTick = roundedTick;

        //
        // Emit tempo
        //
        {
            uint16_t tempoBPM;
            if constexpr (0x6e <= VERSION) {
                tempoBPM = t.header.tempo2;
            } else {
                tempoBPM = t.header.tempo1;
            }

            //
            // convert BeatsPerMinute -> MicrosPerBeat
            //
            // TabIt uses floor(), but using round() is more accurate
            //
            // auto microsPerBeat = (MICROS_PER_MINUTE / tempoBPM).round();
            auto microsPerBeat = (MICROS_PER_MINUTE.to_uint32() / tempoBPM);

            diff = (roundedTick - lastEventTick);

            std::vector<uint8_t> tempoChangeData;

            toDigitsBEOnly3(microsPerBeat, tempoChangeData); // only last 3 bytes of microsPerBeatBytes

            tmp.push_back(MetaEvent{
                diff.to_int32(), // delta time
                M_SETTEMPO,
                tempoChangeData
            });

            lastEventTick = roundedTick;

            if (opts.emit_custom_lyric_events) {

                diff = (roundedTick - lastEventTick);

                auto lyricStr = std::string("space 0 tempo ") + std::to_string(tempoBPM);

                auto lyricData = std::vector<uint8_t>{lyricStr.cbegin(), lyricStr.cend()};

                tmp.push_back(MetaEvent{
                    diff.to_int32(), // delta time
                    M_LYRIC,
                    lyricData
                });

                lastEventTick = roundedTick;
            }
        }

        auto &repeatCloseMap = repeatCloseMaps[0];

        for (uint16_t space = 0; space < barLinesSpaceCount + 1;) { // space count, + 1 for handling repeats at end

            //
            // handle any repeat closes first
            //
            {
                const auto &repeatCloseMapIt = repeatCloseMap.find(space);
                if (repeatCloseMapIt != repeatCloseMap.end()) {

                    //
                    // there is a repeat close at this space
                    //

                    auto &r = repeatCloseMapIt->second;

                    //
                    // how many repeats are left?
                    //

                    if (r.repeats > 0) {

                        if (r.jump < 3) {
                            
                            //
                            // jump to the repeat open and continue processing
                            //

                            space = r.open;

                            if (r.jump == 0) {

                            } else if (r.jump == 1) {

                                r.dataStart = tmp.size();

                            } else {

                                ASSERT(r.jump == 2);

                                r.dataEnd = tmp.size();
                            }

                            r.repeats--;

                            r.jump++;

                            continue;
                        }

                        //
                        // make copies of the events instead of jumping to the repeat open
                        //
                        // have now reached a fix-point, so this is correct
                        //

                        using tmp_diff_t = std::iterator_traits<std::vector<midi_track_event>::iterator>::difference_type;

                        auto sectionSize = r.dataEnd - r.dataStart;
                        auto sectionStart = tmp.cbegin() + static_cast<tmp_diff_t>(r.dataStart);

#ifndef NDEBUG
                        //
                        // verify all events are correct
                        //

                        auto lastJumpStart = tmp.cend() - static_cast<tmp_diff_t>(sectionSize);

                        for (size_t i = 0; i < sectionSize; i++) {
                            midi_track_event a = *(lastJumpStart + static_cast<tmp_diff_t>(i));
                            midi_track_event b = *(sectionStart + static_cast<tmp_diff_t>(i));
                            ASSERT(a == b);
                        }
#endif // NDEBUG

                        auto sectionEnd = tmp.cbegin() + static_cast<tmp_diff_t>(r.dataEnd);

                        auto section = std::vector<midi_track_event>(sectionStart, sectionEnd);

                        tmp.reserve(tmp.size() + r.repeats * sectionSize);

                        for (size_t i = 0; i < r.repeats; i++) {
                            tmp.insert(tmp.end(), section.cbegin(), section.cend());
                        }

                        r.repeats = 0;
                    }
                }
            }
            
            //
            // Emit tempo changes
            //
            {
                const auto &tempoMapIt = tempoMap.find(space);
                if (tempoMapIt != tempoMap.end()) {

                    //
                    // map of tempo changes at this floored space
                    //
                    const auto &m = tempoMapIt->second;

                    for (const auto &mIt : m) {

                        auto actualSpace = mIt.first;
                        auto tempoBPM = mIt.second;

                        auto spaceDiff = (actualSpace - space);

                        ASSERT(spaceDiff.is_nonnegative());

                        //
                        // convert BeatsPerMinute -> MicrosPerBeat
                        //
                        // TabIt uses floor(), but using round() is more accurate
                        //
                        // auto microsPerBeat = (MICROS_PER_MINUTE / tempoBPM).round();
                        auto microsPerBeat = (MICROS_PER_MINUTE.to_uint32() / tempoBPM);

                        {
                            //
                            // save tick
                            //
                            auto oldTick = tick;

                            auto oldRoundedTick = roundedTick;

                            //
                            // increment by spaceDiff
                            //
                            tick += spaceDiff * TBT_TICKS_PER_SPACE;

                            roundedTick = tick.round();

                            diff = (roundedTick - lastEventTick);

                            std::vector<uint8_t> tempoChangeData;

                            toDigitsBEOnly3(microsPerBeat, tempoChangeData); // only last 3 bytes of microsPerBeatBytes

                            tmp.push_back(MetaEvent{
                                diff.to_int32(), // delta time
                                M_SETTEMPO,
                                tempoChangeData
                            });

                            lastEventTick = roundedTick;
                            
                            if (opts.emit_custom_lyric_events) {

                                diff = (roundedTick - lastEventTick);

                                auto lyricStr = std::string("space ") + std::to_string(actualSpace.floor().to_uint32()) + " tempo " + std::to_string(tempoBPM);

                                auto lyricData = std::vector<uint8_t>{lyricStr.cbegin(), lyricStr.cend()};

                                tmp.push_back(MetaEvent{
                                    diff.to_int32(), // delta time
                                    M_LYRIC,
                                    lyricData
                                });

                                lastEventTick = roundedTick;
                            }

                            //
                            // restore tick
                            //
                            tick = oldTick;

                            roundedTick = oldRoundedTick;
                        }
                    }
                }
            }

            tick += TBT_TICKS_PER_SPACE;

            roundedTick = tick.round();

            space++;

        } // for space

        //
        // now backup 1 space
        //

        tick -= TBT_TICKS_PER_SPACE;

        roundedTick = tick.round();

#ifndef NDEBUG
        for (const auto &repeatCloseMapIt : repeatCloseMap) {
            const auto &r = repeatCloseMapIt.second;
            ASSERT(r.repeats == 0);
        }
#endif // NDEBUG

        diff = (roundedTick - lastEventTick);

        std::vector<uint8_t> endOfTrackData;

        tmp.push_back(MetaEvent{
            diff.to_int32(), // delta time
            M_ENDOFTRACK,
            endOfTrackData
        });

        lastEventTick = roundedTick;

        out.tracks.push_back(tmp);

#ifndef NDEBUG
        tickCount = tick.to_uint32();
#endif // NDEBUG

    } // Track 0

    //
    // the actual tracks
    //
    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        const uint8_t channel = channelMap[track];

        const auto &midiNoteOffsetArray = midiNoteOffsetArrays[track];

        const auto &trackMetadata = t.metadata.tracks[track];

        uint8_t midiBank;
        if constexpr (0x6e <= VERSION) {
            midiBank = trackMetadata.midiBank;
        } else {
            midiBank = 0;
        }

        bool dontLetRing;
        uint8_t midiProgram;
        dontLetRing = ((trackMetadata.cleanGuitar & 0b10000000) == 0b10000000);
        midiProgram =  (trackMetadata.cleanGuitar & 0b01111111);

        uint8_t volume = trackMetadata.volume;

        uint8_t pan;
        if constexpr (0x6b <= VERSION) {
            pan = trackMetadata.pan;
        } else {
            pan = 0x40; // 64
        }
        
        uint8_t reverb;
        uint8_t chorus;
        if constexpr (0x6e <= VERSION) {
            
            reverb = trackMetadata.reverb;
            chorus = trackMetadata.chorus;
            
        } else {
            
            reverb = 0;
            chorus = 0;
        }

        uint8_t modulation;
        int16_t pitchBend;
        if constexpr (0x71 <= VERSION) {
            
            modulation = trackMetadata.modulation;

            //
            // convert from (-2400, 2400) to (0b0000000000000000 to 0b0011111111111111) i.e. (0 to 16383)
            //
            pitchBend = (((rational(trackMetadata.pitchBend) + 2400) * 0b0011111111111111) / (2 * 2400)).round().to_int16();
            
        } else {
            
            modulation = 0;
            pitchBend = 0b0010000000000000; // 8192
        }

        //
        // various ticks and spaces
        //
        rational tick = 0;

        rational previousRoundedTick = 0;

        rational roundedTick = 0;

        rational actualSpace = 0;

        rational flooredActualSpace = 0;

        uint16_t flooredActualSpaceI = 0;

        rational lastEventTick = 0;

        std::array<uint8_t, STRINGS_PER_TRACK> currentlyPlayingStrings{};


        //
        // start processing
        //


        tmp.clear();

        auto diff = (roundedTick - lastEventTick);

        auto str = std::string("tbt-parser MIDI - Track ") + std::to_string(track + 1);

        std::vector<uint8_t> data{str.cbegin(), str.cend()};

        tmp.push_back(MetaEvent{
            diff.to_int32(), // delta time
            M_TRACKNAME,
            data
        });

        lastEventTick = roundedTick;

        if (opts.emit_control_change_events) {

            if (midiBank != 0) {

                //
                // Bank Select MSB and Bank Select LSB are special and do not really mean MSB/LSB
                // TabIt only sends MSB
                //
                auto midiBankMSB = midiBank;

                diff = (roundedTick - lastEventTick);

                tmp.push_back(ControlChangeEvent{
                    diff.to_int32(), // delta time
                    channel,
                    C_BANKSELECT_MSB,
                    midiBankMSB
                });

                lastEventTick = roundedTick;
            }
        }

        if (opts.emit_program_change_events) {

            diff = (roundedTick - lastEventTick);

            tmp.push_back(ProgramChangeEvent{
                diff.to_int32(), // delta time
                channel,
                midiProgram
            });

            lastEventTick = roundedTick;
        }

        if (opts.emit_control_change_events) {

            diff = (roundedTick - lastEventTick);

            tmp.push_back(ControlChangeEvent{
                diff.to_int32(), // delta time
                channel,
                C_VOLUME,
                volume
            });

            lastEventTick = roundedTick;

            diff = (roundedTick - lastEventTick);

            tmp.push_back(ControlChangeEvent{
                diff.to_int32(), // delta time
                channel,
                C_PAN,
                pan
            });

            lastEventTick = roundedTick;

            diff = (roundedTick - lastEventTick);

            tmp.push_back(ControlChangeEvent{
                diff.to_int32(), // delta time
                channel,
                C_REVERB,
                reverb
            });

            lastEventTick = roundedTick;

            diff = (roundedTick - lastEventTick);

            tmp.push_back(ControlChangeEvent{
                diff.to_int32(), // delta time
                channel,
                C_CHORUS,
                chorus
            });

            lastEventTick = roundedTick;

            diff = (roundedTick - lastEventTick);

            tmp.push_back(ControlChangeEvent{
                diff.to_int32(), // delta time
                channel,
                C_MODULATION,
                modulation
            });

            lastEventTick = roundedTick;

            //
            // RPN Parameter MSB 0, RPN Parameter LSB 0 = RPN Parameter 0
            // RPN Parameter 0 is standardized for pitch bend range
            //

            diff = (roundedTick - lastEventTick);

            tmp.push_back(ControlChangeEvent{
                diff.to_int32(), // delta time
                channel,
                C_RPNPARAM_MSB,
                0
            });

            lastEventTick = roundedTick;

            diff = (roundedTick - lastEventTick);

            tmp.push_back(ControlChangeEvent{
                diff.to_int32(), // delta time
                channel,
                C_RPNPARAM_LSB,
                0
            });

            lastEventTick = roundedTick;

            diff = (roundedTick - lastEventTick);

            tmp.push_back(ControlChangeEvent{
                diff.to_int32(), // delta time
                channel,
                C_DATAENTRY_MSB,
                24 // semi-tones
            });

            lastEventTick = roundedTick;

            diff = (roundedTick - lastEventTick);

            tmp.push_back(ControlChangeEvent{
                diff.to_int32(), // delta time
                channel,
                C_DATAENTRY_LSB,
                0 // cents
            });

            lastEventTick = roundedTick;
        }

        if (opts.emit_pitch_bend_events) {

            diff = (roundedTick - lastEventTick);

            tmp.push_back(PitchBendEvent{
                diff.to_int32(), // delta time
                channel,
                pitchBend
            });

            lastEventTick = roundedTick;
        }

        uint16_t trackSpaceCount;
        if constexpr (0x70 <= VERSION) {
            //
            // stored as 32-bit int, so must be cast
            //
            trackSpaceCount = static_cast<uint16_t>(trackMetadata.spaceCount);
        } else if constexpr (VERSION == 0x6f) {
            trackSpaceCount = t.header.spaceCount;
        } else {
            trackSpaceCount = 4000;
        }

        //
        // flooredActualSpace -> repeat_open_struct
        //
        // computed when rendering track
        //
        std::map<uint16_t, repeat_open_struct> repeatOpenMap;

        auto &openSpaceSet = openSpaceSets[track + 1];

        auto &repeatCloseMap = repeatCloseMaps[track + 1];

        const auto &maps = t.body.mapsList[track];

        for (uint16_t space = 0; space < trackSpaceCount + 1;) { // space count, + 1 for handling repeats at end

            //
            // handle any repeat closes first
            //
            {
                const auto &repeatCloseMapIt = repeatCloseMap.find(flooredActualSpaceI);
                if (repeatCloseMapIt != repeatCloseMap.end()) {

                    //
                    // there is a repeat close at this space
                    //

                    auto &r = repeatCloseMapIt->second;

                    //
                    // how many repeats are left?
                    //

                    if (r.repeats > 0) {

                        if (r.jump < 3) {

                            //
                            // jump to the repeat open and continue processing
                            //

                            auto spaceDiff = (actualSpace - flooredActualSpace);

                            ASSERT(spaceDiff.is_nonnegative());

                            if (spaceDiff.is_positive()) {

                                LOGW("repeat CLOSE at non-integral space: %f", actualSpace.to_double());

                                //
                                // overshot the repeat close
                                //
                                // now backup by the difference between flooredActualSpace and actualSpace
                                //
                                tick -= spaceDiff * TBT_TICKS_PER_SPACE;

                                roundedTick = tick.round();
                            }

                            ASSERT(repeatOpenMap.contains(r.open));

                            const auto &openStruct = repeatOpenMap[r.open];

                            space = openStruct.space;

                            actualSpace = openStruct.actualSpace;

                            flooredActualSpace = actualSpace.floor();

                            flooredActualSpaceI = flooredActualSpace.to_uint16();

                            spaceDiff = (actualSpace - r.open);

                            ASSERT(spaceDiff.is_nonnegative());

                            if (spaceDiff.is_positive()) {

                                LOGW("repeat OPEN at non-integral space: %f", actualSpace.to_double());

                                //
                                // undershot the repeat open
                                //
                                // now scoot up by the difference between actualSpace and r.open
                                //
                                tick += spaceDiff * TBT_TICKS_PER_SPACE;

                                roundedTick = tick.round();
                            }

                            if (r.jump == 0) {


                            } else if (r.jump == 1) {

                                r.dataStart = tmp.size();

                            } else {

                                ASSERT(r.jump == 2);

                                r.dataEnd = tmp.size();
                            }

                            r.repeats--;

                            r.jump++;

                            continue;
                        }

                        //
                        // make copies of the events instead of jumping to the repeat open
                        //
                        // have now reached a fix-point, so this is correct
                        //

                        using tmp_diff_t = std::iterator_traits<std::vector<midi_track_event>::iterator>::difference_type;

                        auto sectionSize = r.dataEnd - r.dataStart;
                        auto sectionStart = tmp.cbegin() + static_cast<tmp_diff_t>(r.dataStart);

#ifndef NDEBUG
                        //
                        // verify all events are correct
                        //

                        auto lastJumpStart = tmp.cend() - static_cast<tmp_diff_t>(sectionSize);
                        
                        for (size_t i = 0; i < sectionSize; i++) {
                            midi_track_event a = *(lastJumpStart + static_cast<tmp_diff_t>(i));
                            midi_track_event b = *(sectionStart + static_cast<tmp_diff_t>(i));
                            ASSERT(a == b);
                        }
#endif // NDEBUG

                        auto sectionEnd = tmp.cbegin() + static_cast<tmp_diff_t>(r.dataEnd);

                        auto section = std::vector<midi_track_event>(sectionStart, sectionEnd);

                        tmp.reserve(tmp.size() + r.repeats * sectionSize);

                        for (size_t i = 0; i < r.repeats; i++) {
                            tmp.insert(tmp.end(), section.cbegin(), section.cend());
                        }

                        r.repeats = 0;
                    }
                }
            }

            //
            // if there is an open repeat at this actual space, then store the track space for later
            //

            if (openSpaceSet.find(flooredActualSpaceI) != openSpaceSet.end()) {

                ASSERT(!repeatOpenMap.contains(flooredActualSpaceI));

                repeatOpenMap[flooredActualSpaceI] = { actualSpace, space };

                //
                // after adding to repeatOpenMap, can be removed from openSpaceSet
                //
                openSpaceSet.erase(flooredActualSpaceI);
            }

            //
            // Emit Note Offs for any previous Muteds
            //
            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                uint8_t current = currentlyPlayingStrings[string];

                if (current != MUTED) {
                    continue;
                }

                currentlyPlayingStrings[string] = 0;

                auto off = 0x80; // open string

                auto midiNote = static_cast<uint8_t>(off + midiNoteOffsetArray[string]);

                //
                // assume tempo of 120 bpm
                //
                // keeping track of correct tempo is too costly
                //
                static const auto microsPerBeat = (MICROS_PER_MINUTE.to_uint32() / 120);

                //
                // convert MicrosPerBeat -> MicrosPerTick
                //
                static const auto microsPerTick = rational(microsPerBeat) / TBT_TICKS_PER_BEAT;

                //
                // a muted note lasts for 1/64 second, or until the next event
                //
                static const auto mutedTickDiff = (MICROS_PER_64TH / microsPerTick).round();

                auto mutedTick = previousRoundedTick + mutedTickDiff;

                if (roundedTick < mutedTick) {
                    mutedTick = roundedTick;
                }

                diff = (mutedTick - lastEventTick);

                tmp.push_back(NoteOffEvent{
                    diff.to_int32(), // delta time
                    channel,
                    midiNote,
                    0 // velocity
                });

                lastEventTick = mutedTick;
            }


            const auto &notesMapIt = maps.notesMap.find(space);

            //
            // Compute note offs and note ons, and emit note offs
            //
            if (notesMapIt != maps.notesMap.end()) {

                bool anyStringsTurningOff = false;

                const auto &onVsqs = notesMapIt->second;

                std::array<uint8_t, STRINGS_PER_TRACK> offVsqs{};

                //
                // Compute note offs and note ons
                //
                if (dontLetRing) {

                    //
                    // Don't Let Ring
                    //
                    // An event on any string stops all strings
                    //

                    //
                    // There may be string effects, but no note events
                    // This will still be in the notesMap, but should not affect notes because of dontLetRing
                    // i.e., simply checking for something in notesMap is not sufficient
                    //
                    // so check all the strings for note events
                    //
                    bool anyEvents = false;
                    for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                        auto event = onVsqs[string];

                        if (event != 0) {
                            anyEvents = true;
                            break;
                        }
                    }
                    
                    if (anyEvents) {
                        
                        offVsqs = currentlyPlayingStrings;

                        anyStringsTurningOff = true;

                        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                            auto on = onVsqs[string];

                            if (on == 0) {

                                currentlyPlayingStrings[string] = 0;

                            } else if (0x80 <= on || on == MUTED) {

                                currentlyPlayingStrings[string] = on;

                            } else {

                                ASSERT(on == STOPPED);

                                currentlyPlayingStrings[string] = 0;
                            }
                        }
                    }

                } else {

                    //
                    // Let Ring
                    //
                    // All strings are independent
                    //

                    for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                        auto on = onVsqs[string];

                        if (on == 0) {

                            //
                            // nothing to do
                            //
                            continue;
                        }

                        auto current = currentlyPlayingStrings[string];

                        if (current == 0) {

                            if (0x80 <= on || on == MUTED) {

                                currentlyPlayingStrings[string] = on;

                            } else {

                                ASSERT(on == STOPPED);

                                //
                                // current is already 0
                                //
                            }

                        } else {

                            offVsqs[string] = current;

                            anyStringsTurningOff = true;

                            if (0x80 <= on || on == MUTED) {

                                currentlyPlayingStrings[string] = on;

                            } else {

                                ASSERT(on == STOPPED);

                                currentlyPlayingStrings[string] = 0;
                            }
                        }
                    }
                }

                //
                // Emit note offs
                //
                if (anyStringsTurningOff) {

                    for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                        auto off = offVsqs[string];

                        if (off == 0) {
                            continue;
                        }

                        ASSERT(off >= 0x80);

                        auto midiNote = static_cast<uint8_t>(off + midiNoteOffsetArray[string]);

                        diff = (roundedTick - lastEventTick);

                        tmp.push_back(NoteOffEvent{
                            diff.to_int32(), // delta time
                            channel,
                            midiNote,
                            0 // velocity
                        });

                        lastEventTick = roundedTick;
                    }
                }
            }

            //
            // Emit track effects
            //
            if constexpr (VERSION == 0x72) {

                const auto &trackEffectChangesIt = maps.trackEffectChangesMap.find(space);
                if (trackEffectChangesIt != maps.trackEffectChangesMap.end()) {

                    const auto &changes = trackEffectChangesIt->second;

                    for (const auto &p : changes) {

                        auto effect = p.first;
                        auto value = p.second;

                        switch(effect) {
                        case TE_INSTRUMENT: {

                            if (opts.emit_control_change_events) {

                                auto newInstrument = value;

                                bool midiBankFlag;

                                midiBankFlag = ((newInstrument & 0b1000000000000000) == 0b1000000000000000);
                                midiBank     = ((newInstrument & 0b0111111100000000) >> 8);
                                dontLetRing  = ((newInstrument & 0b0000000010000000) == 0b0000000010000000);
                                midiProgram  =  (newInstrument & 0b0000000001111111);
                                
                                if (midiBankFlag) {

                                    //
                                    // Bank Select MSB and Bank Select LSB are special and do not really mean MSB/LSB
                                    // TabIt only sends MSB
                                    //
                                    uint8_t midiBankMSB = midiBank;
                                    
                                    diff = (roundedTick - lastEventTick);

                                    tmp.push_back(ControlChangeEvent{
                                        diff.to_int32(), // delta time
                                        channel,
                                        C_BANKSELECT_MSB,
                                        midiBankMSB
                                    });

                                    lastEventTick = roundedTick;
                                }
                            }

                            if (opts.emit_program_change_events) {

                                diff = (roundedTick - lastEventTick);

                                tmp.push_back(ProgramChangeEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    midiProgram
                                });

                                lastEventTick = roundedTick;
                            }

                            break;
                        }
                        case TE_VOLUME: {

                            if (opts.emit_control_change_events) {

                                auto newVolume = static_cast<uint8_t>(value);

                                diff = (roundedTick - lastEventTick);

                                tmp.push_back(ControlChangeEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    C_VOLUME,
                                    newVolume
                                });

                                lastEventTick = roundedTick;
                            }

                            break;
                        }
                        // NOLINTNEXTLINE(bugprone-branch-clone)
                        case TE_TEMPO:
                            //
                            // already handled
                            //
                            break;
                        case TE_STROKE_DOWN:
                        case TE_STROKE_UP:
                            //
                            // nothing to do
                            //
                            break;
                        case TE_PAN: {

                            if (opts.emit_control_change_events) {

                                auto newPan = static_cast<uint8_t>(value);

                                diff = (roundedTick - lastEventTick);

                                tmp.push_back(ControlChangeEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    C_PAN,
                                    newPan
                                });

                                lastEventTick = roundedTick;
                            }

                            break;
                        }
                        case TE_CHORUS: {

                            if (opts.emit_control_change_events) {

                                auto newChorus = static_cast<uint8_t>(value);

                                diff = (roundedTick - lastEventTick);

                                tmp.push_back(ControlChangeEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    C_CHORUS,
                                    newChorus
                                });

                                lastEventTick = roundedTick;
                            }

                            break;
                        }
                        case TE_REVERB: {

                            if (opts.emit_control_change_events) {

                                auto newReverb = static_cast<uint8_t>(value);

                                diff = (roundedTick - lastEventTick);

                                tmp.push_back(ControlChangeEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    C_REVERB,
                                    newReverb
                                });

                                lastEventTick = roundedTick;
                            }

                            break;
                        }
                        case TE_MODULATION: {

                            if (opts.emit_control_change_events) {

                                auto newModulation = static_cast<uint8_t>(value);

                                diff = (roundedTick - lastEventTick);

                                tmp.push_back(ControlChangeEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    C_MODULATION,
                                    newModulation
                                });

                                lastEventTick = roundedTick;
                            }

                            break;
                        }
                        case TE_PITCH_BEND: {

                            if (opts.emit_pitch_bend_events) {

                                //
                                // convert from (-2400, 2400) to (0b0000000000000000 to 0b0011111111111111) i.e. (0 to 16383)
                                //
                                auto newPitchBend = (((rational(static_cast<int16_t>(value)) + 2400) * 0b0011111111111111) / (2 * 2400)).round().to_int16();

                                diff = (roundedTick - lastEventTick);

                                tmp.push_back(PitchBendEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    newPitchBend
                                });

                                lastEventTick = roundedTick;
                            }

                            break;
                        }
                        default: {
                            ABORT("invalid effect: %d", effect);
                        }
                        }
                    }
                }

            } else {

                if (notesMapIt != maps.notesMap.end()) {

                    const auto &effectVsqs = notesMapIt->second;

                    auto trackEffect = effectVsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 0];

                    switch (trackEffect) {
                    case '\0':
                        //
                        // skip
                        //
                        break;
                    case 'I': { // Instrument change

                        if (opts.emit_program_change_events) {

                            auto newInstrument = effectVsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            dontLetRing = ((newInstrument & 0b10000000) == 0b10000000);
                            midiProgram =  (newInstrument & 0b01111111);

                            diff = (roundedTick - lastEventTick);

                            tmp.push_back(ProgramChangeEvent{
                                diff.to_int32(), // delta time
                                channel,
                                midiProgram
                            });

                            lastEventTick = roundedTick;
                        }

                        break;
                    }
                    case 'V': { // Volume change

                        if (opts.emit_control_change_events) {

                            auto newVolume = effectVsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            diff = (roundedTick - lastEventTick);

                            tmp.push_back(ControlChangeEvent{
                                diff.to_int32(), // delta time
                                channel,
                                C_VOLUME,
                                newVolume
                            });

                            lastEventTick = roundedTick;
                        }

                        break;
                    }
                    // NOLINTNEXTLINE(bugprone-branch-clone)
                    case 'T': // Tempo change
                    case 't': // Tempo change + 250
                        //
                        // already handled
                        //
                        break;
                    case 'D': // Stroke down
                    case 'U': // Stroke up
                        //
                        // nothing to do
                        //
                        break;
                    case 'C': { // Chorus change

                        if (opts.emit_control_change_events) {

                            auto newChorus = effectVsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            diff = (roundedTick - lastEventTick);

                            tmp.push_back(ControlChangeEvent{
                                diff.to_int32(), // delta time
                                channel,
                                C_CHORUS,
                                newChorus
                            });

                            lastEventTick = roundedTick;
                        }

                        break;
                    }
                    case 'P': { // Pan change

                        if (opts.emit_control_change_events) {

                            auto newPan = effectVsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            diff = (roundedTick - lastEventTick);

                            tmp.push_back(ControlChangeEvent{
                                diff.to_int32(), // delta time
                                channel,
                                C_PAN,
                                newPan
                            });

                            lastEventTick = roundedTick;
                        }

                        break;
                    }
                    case 'R': { // Reverb change

                        if (opts.emit_control_change_events) {

                            auto newReverb = effectVsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            diff = (roundedTick - lastEventTick);

                            tmp.push_back(ControlChangeEvent{
                                diff.to_int32(), // delta time
                                channel,
                                C_REVERB,
                                newReverb
                            });

                            lastEventTick = roundedTick;
                        }

                        break;
                    }
                    default: {
                        ABORT("invalid trackEffect: %c (%d)", trackEffect, trackEffect);
                    }
                    }
                }
            }

            //
            // Emit note ons
            //
            if (notesMapIt != maps.notesMap.end()) {

                const auto &onVsqs = notesMapIt->second;
                
                for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                    auto on = onVsqs[string];

                    if (on == 0 ||
                        on == STOPPED) {
                        continue;
                    }

                    if (on == MUTED) {
                        on = 0x80; // open string
                    }

                    ASSERT(on >= 0x80);

                    auto midiNote = static_cast<uint8_t>(on + midiNoteOffsetArray[string]);

                    diff = (roundedTick - lastEventTick);

                    //
                    // This is actually a departure from how TabIt exports to MIDI
                    // TabIt uses the track volume for the velocity of each note
                    // But it is better to separate note velocity and track volume
                    //
                    // Complaint about this on Reddit:
                    // https://old.reddit.com/r/tabit/comments/z6e9yo/community_version_of_tabit/j07cfhw/
                    //
                    tmp.push_back(NoteOnEvent{
                        diff.to_int32(), // delta time
                        channel,
                        midiNote,
                        0x40 // velocity
                    });

                    lastEventTick = roundedTick;
                }
            }

            //
            // Compute actual space
            //
            if constexpr (HASALTERNATETIMEREGIONS) {

                const auto &alternateTimeRegionsIt = maps.alternateTimeRegionsMap.find(space);
                if (alternateTimeRegionsIt != maps.alternateTimeRegionsMap.end()) {

                    const auto &alternateTimeRegion = alternateTimeRegionsIt->second;

                    auto atr = rational{ alternateTimeRegion[0], alternateTimeRegion[1] };

                    tick += (atr * TBT_TICKS_PER_SPACE);

                    previousRoundedTick = roundedTick;

                    roundedTick = tick.round();

                    space++;

                    actualSpace += atr;

                    flooredActualSpace = actualSpace.floor();

                    flooredActualSpaceI = flooredActualSpace.to_uint16();

                } else {

                    tick += TBT_TICKS_PER_SPACE;

                    previousRoundedTick = roundedTick;

                    roundedTick = tick.round();

                    space++;

                    ++actualSpace;

                    flooredActualSpace = actualSpace.floor();

                    flooredActualSpaceI = flooredActualSpace.to_uint16();
                }

            } else {

                tick += TBT_TICKS_PER_SPACE;

                previousRoundedTick = roundedTick;

                roundedTick = tick.round();

                space++;

                actualSpace = space;

                flooredActualSpace = space;

                flooredActualSpaceI = space;
            }

        } // for space

        //
        // now backup 1 space
        //

        tick -= TBT_TICKS_PER_SPACE;
        
        roundedTick = tick.round();
        
        --actualSpace;

#ifndef NDEBUG
        ASSERT(tick == tickCount);
        ASSERT(roundedTick == tickCount);
        ASSERT(actualSpace == barLinesSpaceCount);
        for (const auto &repeatCloseMapIt : repeatCloseMap) {
            const auto &r = repeatCloseMapIt.second;
            ASSERT(r.repeats == 0);
        }
        ASSERT(openSpaceSet.empty());
#endif // NDEBUG

        //
        // Emit any final note offs
        //
        {
            auto offVsqs = currentlyPlayingStrings;

            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                auto off = offVsqs[string];

                if (off == 0) {
                    continue;
                }

                ASSERT(off >= 0x80);

                auto midiNote = static_cast<uint8_t>(off + midiNoteOffsetArray[string]);

                diff = (roundedTick - lastEventTick);

                tmp.push_back(NoteOffEvent{
                    diff.to_int32(), // delta time
                    channel,
                    midiNote,
                    0 // velocity
                });

                lastEventTick = roundedTick;
            }
        }

        diff = (roundedTick - lastEventTick);

        std::vector<uint8_t> endOfTrackData;

        tmp.push_back(MetaEvent{
            diff.to_int32(), // delta time
            M_ENDOFTRACK,
            endOfTrackData
        });

        lastEventTick = roundedTick;

        out.tracks.push_back(tmp);

    } // for track
    
    return OK;
}


Status
convertToMidi(
    const tbt_file &t,
    const midi_convert_opts &opts,
    midi_file &out) {

    auto versionNumber = tbtFileVersionNumber(t);

    switch (versionNumber) {
    case 0x72: {

        auto t71 = std::get<tbt_file71>(t);
        
        if ((t71.header.featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            return TconvertToMidi<0x72, true, 8>(t71, opts, out);
        } else {
            return TconvertToMidi<0x72, false, 8>(t71, opts, out);
        }
    }
    case 0x71: {
        
        auto t71 = std::get<tbt_file71>(t);
        
        if ((t71.header.featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            return TconvertToMidi<0x71, true, 8>(t71, opts, out);
        } else {
            return TconvertToMidi<0x71, false, 8>(t71, opts, out);
        }
    }
    case 0x70: {
        
        auto t70 = std::get<tbt_file70>(t);
        
        if ((t70.header.featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            return TconvertToMidi<0x70, true, 8>(t70, opts, out);
        } else {
            return TconvertToMidi<0x70, false, 8>(t70, opts, out);
        }
    }
    case 0x6f: {
        
        auto t6f = std::get<tbt_file6f>(t);
        
        return TconvertToMidi<0x6f, false, 8>(t6f, opts, out);
    }
    case 0x6e: {
        
        auto t6e = std::get<tbt_file6e>(t);
        
        return TconvertToMidi<0x6e, false, 8>(t6e, opts, out);
    }
    case 0x6b: {
        
        auto t6b = std::get<tbt_file6b>(t);
        
        return TconvertToMidi<0x6b, false, 8>(t6b, opts, out);
    }
    case 0x6a: {
        
        auto t6a = std::get<tbt_file6a>(t);
        
        return TconvertToMidi<0x6a, false, 6>(t6a, opts, out);
    }
    case 0x69: {
        
        auto t68 = std::get<tbt_file68>(t);
        
        return TconvertToMidi<0x69, false, 6>(t68, opts, out);
    }
    case 0x68: {
        
        auto t68 = std::get<tbt_file68>(t);
        
        return TconvertToMidi<0x68, false, 6>(t68, opts, out);
    }
    case 0x67: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TconvertToMidi<0x67, false, 6>(t65, opts, out);
    }
    case 0x66: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TconvertToMidi<0x66, false, 6>(t65, opts, out);
    }
    case 0x65: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TconvertToMidi<0x65, false, 6>(t65, opts, out);
    }
    default: {
        ABORT("invalid versionNumber: 0x%02x", versionNumber);
    }
    }
}


struct EventExportVisitor {
    
    std::vector<uint8_t> &tmp;

    void operator()(const ProgramChangeEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xc0 | e.channel), // program change
            e.midiProgram
        });
    }

    void operator()(const PitchBendEvent &e) {

        uint8_t pitchBendLSB = (e.pitchBend & 0b01111111);
        uint8_t pitchBendMSB = ((e.pitchBend >> 7) & 0b01111111);

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xe0 | e.channel), // pitch bend
            pitchBendLSB,
            pitchBendMSB
        });
    }

    void operator()(const NoteOffEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0x80 | e.channel), // note off
            e.midiNote,
            e.velocity
        });
    }

    void operator()(const NoteOnEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0x90 | e.channel), // note on
            e.midiNote,
            e.velocity
        });
    }

    void operator()(const ControlChangeEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xb0 | e.channel),
            e.controller,
            e.value
        });
    }

    void operator()(const MetaEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            0xff, // Meta
            e.type
        });

        toVLQ(static_cast<uint32_t>(e.data.size()), tmp); // len

        tmp.insert(tmp.end(), e.data.cbegin(), e.data.cend());
    }

    void operator()(const PolyphonicKeyPressureEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xa0 | e.channel),
            e.midiNote,
            e.pressure
        });
    }

    void operator()(const ChannelPressureEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xd0 | e.channel),
            e.pressure
        });
    }

    void operator()(const SysExEvent &e) {
        (void)e;
    }
};


Status
exportMidiBytes(
    const midi_file &m,
    std::vector<uint8_t> &out) {

    out.clear();

    //
    // header
    //
    {
        out.insert(out.end(), S_MTHD.cbegin(), S_MTHD.cend()); // type

        toDigitsBE(static_cast<uint32_t>(2 + 2 + 2), out); // length

        toDigitsBE(m.header.format, out); // format

        toDigitsBE(m.header.trackCount, out); // track count

        toDigitsBE(m.header.division, out); // division
    }

    //
    // tracks
    //
    for (const auto &track : m.tracks) {

        std::vector<uint8_t> tmp;

        EventExportVisitor eventExportVisitor{ tmp };

        for (const auto &event : track) {
            std::visit(eventExportVisitor, event);
        }

        out.insert(out.end(), S_MTRK.cbegin(), S_MTRK.cend()); // type

        toDigitsBE(static_cast<uint32_t>(tmp.size()), out); // length

        out.insert(out.end(), tmp.cbegin(), tmp.cend()); // data
    }

    return OK;
}


Status
exportMidiFile(
    const midi_file &m,
    const char *path) {

    std::vector<uint8_t> data;

    Status ret;

    ret = exportMidiBytes(m, data);

    if (ret != OK) {
        return ret;
    }

    ret = saveFile(path, data);

    if (ret != OK) {
        return ret;
    }

    return OK;
}


Status
parseMidiFile(
    const char *path,
    midi_file &out) {
    
    Status ret;

    std::vector<uint8_t> buf;

    ret = openFile(path, buf);

    if (ret != OK) {
        return ret;
    }

    auto buf_it = buf.cbegin();

    auto buf_end = buf.cend();

    return parseMidiBytes(buf_it, buf_end, out);
}



struct chunk { // NOLINT(*-pro-type-member-init)
    std::array<uint8_t, 4> type;
    std::vector<uint8_t> data;
};


Status
parseChunk(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    chunk &out) {

    CHECK(it + 4 + 4 <= end, "out of data");

    std::memcpy(out.type.data(), &*it, 4);

    it += 4;

    uint32_t len = parseBE4(it);
    
    if (len == 0) {
        LOGW("chunk length is 0");
    }

    auto begin = it;
    
    it += len;

    CHECK(it <= end, "out of data");

    out.data = std::vector<uint8_t>(begin, it);

    return OK;
}


Status
parseHeader(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    midi_file &out) {

    chunk c;

    Status ret = parseChunk(it, end, c);

    if (ret != OK) {
        return ret;
    }

    CHECK(std::memcmp(c.type.data(), S_MTHD.c_str(), 4) == 0, "expected MThd type");

    auto it2 = c.data.cbegin();

    auto end2 = c.data.cend();

    CHECK(it2 + 2 + 2 + 2 <= end2, "out of data");

    out.header.format = parseBE2(it2);

    out.header.trackCount = parseBE2(it2);

    out.header.division = parseBE2(it2);

    if (out.header.format == 0) {

        if (out.header.trackCount != 1) {
            LOGW("format 0 but trackCount != 1: %d", out.header.trackCount);
        }
    }

    if (it2 != end2) {
        LOGW("bytes after header: %zu", (end2 - it2));
    }

    return OK;
}


Status
parseTrackEvent(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    uint8_t &running,
    midi_track_event &out) {

    uint32_t UdeltaTime;

    Status ret = parseVLQ(it, end, UdeltaTime);

    if (ret != OK) {
        return ret;
    }

    auto deltaTime = static_cast<int32_t>(UdeltaTime);

    CHECK(it + 1 <= end, "out of data");

    auto b = *it++;

    uint8_t hi;
    uint8_t lo;

    if ((b & 0b10000000) == 0b00000000) {

        //
        // use running status
        //

        CHECK((running & 0b10000000) == 0b10000000, "running status is not set");

        hi = (running & 0b11110000);
        lo = (running & 0b00001111);

        //
        // b is already set
        //

    } else if (b == 0xff) {

        //
        // ignore running status
        //

        hi = (b & 0b11110000);
        lo = (b & 0b00001111);

        CHECK(it + 1 <= end, "out of data");

        b = *it++;

    } else if ((b & 0b11110000) == 0b11110000) {

        //
        // cancel running status
        //

        running = 0xff;

        hi = (b & 0b11110000);
        lo = (b & 0b00001111);

        CHECK(it + 1 <= end, "out of data");

        b = *it++;

    } else {

        //
        // set running status
        //

        running = b;

        hi = (b & 0b11110000);
        lo = (b & 0b00001111);

        CHECK(it + 1 <= end, "out of data");

        b = *it++;
    }

    switch (hi) {
    case 0x80: {

        auto channel = lo;

        uint8_t midiNote = (b & 0b01111111);

        CHECK(it + 1 <= end, "out of data");

        b = *it++;

        uint8_t velocity = (b & 0b01111111);

        out = NoteOffEvent{deltaTime, channel, midiNote, velocity};

        return OK;
    }
    case 0x90: {

        auto channel = lo;

        uint8_t midiNote = (b & 0b01111111);
        
        CHECK(it + 1 <= end, "out of data");

        b = *it++;

        uint8_t velocity = (b & 0b01111111);

        out = NoteOnEvent{deltaTime, channel, midiNote, velocity};

        return OK;
    }
    case 0xa0: {

        auto channel = lo;

        uint8_t midiNote = (b & 0b01111111);
        
        CHECK(it + 1 <= end, "out of data");

        b = *it++;

        uint8_t pressure = (b & 0b01111111);

        out = PolyphonicKeyPressureEvent{deltaTime, channel, midiNote, pressure};

        return OK;
    }
    case 0xb0: {

        auto channel = lo;

        auto controller = b;

        CHECK(it + 1 <= end, "out of data");

        b = *it++;

        uint8_t value = (b & 0b01111111);

        out = ControlChangeEvent{deltaTime, channel, controller, value};

        return OK;
    }
    case 0xc0: {

        auto channel = lo;

        uint8_t midiProgram = (b & 0b01111111);

        out = ProgramChangeEvent{deltaTime, channel, midiProgram};

        return OK;
    }
    case 0xd0: {

        auto channel = lo;

        uint8_t pressure = (b & 0b01111111);

        out = ChannelPressureEvent{deltaTime, channel, pressure};

        return OK;
    }
    case 0xe0: {

        auto channel = lo;

        uint8_t pitchBendLSB = (b & 0b01111111);
        
        CHECK(it + 1 <= end, "out of data");

        b = *it++;

        uint8_t pitchBendMSB = (b & 0b01111111);

        auto pitchBend = static_cast<int16_t>((pitchBendMSB << 7) | pitchBendLSB);

        out = PitchBendEvent{deltaTime, channel, pitchBend};

        return OK;
    }
    case 0xf0: {

        if (lo == 0x00) {

            std::vector<uint8_t> tmp;

            tmp.push_back(b);

            while (true) {

                if (b == 0xf7) {
                    break;
                }

                CHECK(it + 1 <= end, "out of data");

                b = *it++;

                tmp.push_back(b);
            }

            auto it2 = tmp.cbegin();
            
            auto end2 = tmp.cend();

            uint32_t len;

            ret = parseVLQ(it2, end2, len);

            if (ret != OK) {
                return ret;
            }

            if (it2 + len != end2) {

                LOGW("SysEx event len is not correct");

                return ERR;
            }

            out = SysExEvent{deltaTime, tmp};

            return OK;

        } else if (lo == 0x0f) {

            auto type = b;

            uint32_t len;

            ret = parseVLQ(it, end, len);

            if (ret != OK) {
                return ret;
            }

            auto begin = it;

            it += len;

            CHECK(it <= end, "out of data");

            std::vector<uint8_t> data(begin, it);

            out = MetaEvent{deltaTime, type, data};

            return OK;

        } else {

            LOGE("unrecognized event byte: %d (0x%02x)", (hi | lo), (hi | lo));

            return ERR;
        }

    }
    default: {

        LOGE("unrecognized event byte: %d (0x%02x)", (hi | lo), (hi | lo));

        return ERR;
    }
    }
}


Status
parseTrack(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    midi_file &out) {

    uint8_t running = 0xff;

    chunk c;

    Status ret = parseChunk(it, end, c);

    if (ret != OK) {
        return ret;
    }

    CHECK(std::memcmp(c.type.data(), S_MTRK.c_str(), 4) == 0, "expected MTrk type");

    std::vector<midi_track_event> track;

    auto it2 = c.data.cbegin();

    auto end2 = c.data.cend();

    while (true) {

        midi_track_event e;

        ret = parseTrackEvent(it2, end2, running, e);

        if (ret != OK) {
            return ret;
        }

        track.push_back(e);

        if (auto metaEvent = std::get_if<MetaEvent>(&e)) {

            if (metaEvent->type == M_ENDOFTRACK) {

                //
                // FluidSynth ignores bytes after EndOfTrack
                //
                // so just warn here
                //
                if (it2 != end2) {
                    LOGW("bytes after EndOfTrack: %zu", (end2 - it2));
                }

                break;
            }
        }
    }

    out.tracks.push_back(track);

    return OK;
}


Status
parseMidiBytes(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    midi_file &out) {

    auto len = (end - it);

    CHECK(len != 0, "empty file");

    Status ret = parseHeader(it, end, out);

    if (ret != OK) {
        return ret;
    }

    for (int i = 0; i < out.header.trackCount; i++) {

        ret = parseTrack(it, end, out);

        if (ret != OK) {
            return ret;
        }
    }

    if (it != end) {
        LOGW("bytes after all tracks: %zu", (end - it));
    }

    return OK;
}


struct EventFileTimesTempoMapVisitor {
    
    const uint16_t division;

    //
    // tick -> microsPerTick
    //
    std::map<rational, rational> &tempoMap;

    uint16_t track;

    rational runningTick;

    void operator()(const ProgramChangeEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const PitchBendEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const NoteOffEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const NoteOnEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const ControlChangeEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const MetaEvent &e) {

        if (e.type == M_SETTEMPO) {

            runningTick += e.deltaTime;

            auto it = e.data.cbegin();

            auto microsPerBeat = parseBE3(it);

            //
            // convert MicrosPerBeat -> MicrosPerTick
            //
            auto newMicrosPerTick = rational(microsPerBeat) / division;

            const auto &tempoMapIt = tempoMap.find(runningTick);
            if (tempoMapIt != tempoMap.end()) {

                auto microsPerTick = tempoMapIt->second;

                if (microsPerTick != newMicrosPerTick) {

                    //
                    // convert MicrosPerTick -> BeatsPerMinute
                    //

                    auto aBPM = (MICROS_PER_MINUTE / (microsPerTick * division));

                    auto bBPM = (MICROS_PER_MINUTE / (newMicrosPerTick * division));

                    LOGW("track: %d tick %f has conflicting tempo changes: %f, %f", track, runningTick.to_double(), aBPM.to_double(), bBPM.to_double());
                }
            }

            tempoMap[runningTick] = newMicrosPerTick;

        } else {

            runningTick += e.deltaTime;
        }
    }

    void operator()(const PolyphonicKeyPressureEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const ChannelPressureEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const SysExEvent &e) {
        runningTick += e.deltaTime;
    }
};


struct EventFileTimesLastTicksVisitor {
    
    const uint16_t division;

    rational runningTick;

    //
    // important to start < 0, because 0 is a valid tick
    //
    rational lastNoteOnTick = -1;
    rational lastNoteOffTick = -1;
    rational lastEndOfTrackTick = -1;
    rational lastTempoChangeTick = -1;
    rational lastMicrosPerTick = 0;

    void operator()(const ProgramChangeEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const PitchBendEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const NoteOffEvent &e) {
        
        runningTick += e.deltaTime;

        if (runningTick > lastNoteOffTick) {
            lastNoteOffTick = runningTick;
        }
    }

    void operator()(const NoteOnEvent &e) {
        
        runningTick += e.deltaTime;

        if (runningTick > lastNoteOnTick) {
            lastNoteOnTick = runningTick;
        }
    }

    void operator()(const ControlChangeEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const MetaEvent &e) {

        switch (e.type) {
        case M_SETTEMPO: {

            runningTick += e.deltaTime;

            if (runningTick > lastTempoChangeTick) {

                lastTempoChangeTick = runningTick;

                auto it = e.data.cbegin();

                auto microsPerBeat = parseBE3(it);

                //
                // convert MicrosPerBeat -> MicrosPerTick
                //
                lastMicrosPerTick = rational(microsPerBeat) / division;
            }

            break;
        }
        case M_ENDOFTRACK: {

            runningTick += e.deltaTime;

            if (runningTick > lastEndOfTrackTick) {
                lastEndOfTrackTick = runningTick;
            }

            break;
        }
        default: {

            runningTick += e.deltaTime;
            
            break;
        }
        }
    }

    void operator()(const PolyphonicKeyPressureEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const ChannelPressureEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const SysExEvent &e) {
        runningTick += e.deltaTime;
    }
};


midi_file_times
midiFileTimes(const midi_file &m) {

    //
    // first build tempo map
    //

    //
    // tick -> microsPerTick
    //
    std::map<rational, rational> tempoMap;

    EventFileTimesTempoMapVisitor eventFileTimesTempoMapVisitor{ m.header.division, tempoMap, 0, 0 };

    for (uint16_t track = 0; track < m.tracks.size(); track++) {

        const auto &t = m.tracks[track];

        eventFileTimesTempoMapVisitor.track = track;
        eventFileTimesTempoMapVisitor.runningTick = 0;

        for (const auto &e : t) {
            std::visit(eventFileTimesTempoMapVisitor, e);
        }
    }


    EventFileTimesLastTicksVisitor v{ m.header.division, 0 };

    for (const auto &track : m.tracks) {

        v.runningTick = 0;

        for (const auto &e : track) {
            std::visit(v, e);
        }
    }


    //
    // insert actual ticks for these events
    //

    if (v.lastNoteOnTick != -1) {
        auto it = last_found(tempoMap, v.lastNoteOnTick);
        tempoMap[v.lastNoteOnTick] = it->second;
    }

    if (v.lastNoteOffTick != -1) {
        auto it = last_found(tempoMap, v.lastNoteOffTick);
        tempoMap[v.lastNoteOffTick] = it->second;
    }

    if (v.lastEndOfTrackTick != -1) {
        auto it = last_found(tempoMap, v.lastEndOfTrackTick);
        tempoMap[v.lastEndOfTrackTick] = it->second;
    }

    //
    // compute actual times for events
    //

    rational runningMicros = 0;
    rational lastNoteOnMicros = -1;
    rational lastNoteOffMicros = -1;
    rational lastEndOfTrackMicros = -1;

    rational lastTick = 0;
    rational lastMicrosPerTick = 0;
    for (const auto &x : tempoMap) {

        auto tick = x.first;
        auto microsPerTick = x.second;

        runningMicros += ((tick - lastTick) * lastMicrosPerTick);

        if (tick == v.lastNoteOnTick) {
            lastNoteOnMicros = runningMicros;
        }
        if (tick == v.lastNoteOffTick) {
            lastNoteOffMicros = runningMicros;
        }
        if (tick == v.lastEndOfTrackTick) {
            lastEndOfTrackMicros = runningMicros;
        }

        lastTick = tick;
        lastMicrosPerTick = microsPerTick;
    }

    midi_file_times times = {
        lastNoteOnMicros.to_double(), lastNoteOffMicros.to_double(), lastEndOfTrackMicros.to_double(),
        v.lastNoteOnTick.to_int32(), v.lastNoteOffTick.to_int32(), v.lastEndOfTrackTick.to_int32()
    };

    return times;
}


std::string
midiFileInfo(const midi_file &m) {

    std::string acc;
    
    char buf[100];

    std::snprintf(buf, sizeof(buf), "header:\n");
    acc += buf;

    std::snprintf(buf, sizeof(buf), "Format: %d\n", m.header.format);
    acc += buf;

    std::snprintf(buf, sizeof(buf), "Track Count: %d\n", m.header.trackCount);
    acc += buf;

    std::snprintf(buf, sizeof(buf), "Division: %d\n", m.header.division);
    acc += buf;


    std::snprintf(buf, sizeof(buf), "events:\n");
    acc += buf;

    for (const auto &track : m.tracks) {

        for (const auto &e : track) {
            std::visit([&](auto&& e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, MetaEvent>) {
                    switch (e.type) {
                    case M_TRACKNAME: {

                        std::string str{ e.data.cbegin(), e.data.cend() };

                        std::snprintf(buf, sizeof(buf), "Track Name: %s\n", str.c_str());
                        acc += buf;

                        break;
                    }
                    case M_TIMESIGNATURE: {

                        auto numerator = e.data[0];
                        auto denominator = e.data[1];
                        auto ticksPerBeat = e.data[2];
                        auto notated32notesPerBeat = e.data[3];

                        std::snprintf(buf, sizeof(buf), "Time Signature Numerator: %d\n", numerator);
                        acc += buf;

                        std::snprintf(buf, sizeof(buf), "Time Signature Denominator: %d\n", denominator);
                        acc += buf;

                        std::snprintf(buf, sizeof(buf), "Time Signature Ticks Per Beat: %d\n", ticksPerBeat);
                        acc += buf;

                        std::snprintf(buf, sizeof(buf), "Time Signature 32nd notes Per Beat: %d\n", notated32notesPerBeat);
                        acc += buf;

                        break;
                    }
                    }
                }
            }, e);
        }
    }

    auto times = midiFileTimes(m);

    std::snprintf(buf, sizeof(buf), "times:                      h:mm:sssss\n");
    acc += buf;

    if (times.lastNoteOnMicros != -1) {

        double lastNoteOnSec = times.lastNoteOnMicros / 1e6;
        double lastNoteOnMin = lastNoteOnSec / 60.0;
        double lastNoteOnHr = lastNoteOnMin / 60.0;

        std::snprintf(buf, sizeof(buf), "       last Note On (wall): %.0f:%02.0f:%05.2f\n", std::floor(lastNoteOnHr), std::floor(std::fmod(lastNoteOnMin, 60.0)), std::fmod(lastNoteOnSec, 60.0));
        acc += buf;

    } else {

        std::snprintf(buf, sizeof(buf), "       last Note On (wall): (none)\n");
        acc += buf;
    }

    if (times.lastNoteOffMicros != -1) {

        double lastNoteOffSec = times.lastNoteOffMicros / 1e6;
        double lastNoteOffMin = lastNoteOffSec / 60.0;
        double lastNoteOffHr = lastNoteOffMin / 60.0;
        std::snprintf(buf, sizeof(buf), "      last Note Off (wall): %.0f:%02.0f:%05.2f\n", std::floor(lastNoteOffHr), std::floor(std::fmod(lastNoteOffMin, 60.0)), std::fmod(lastNoteOffSec, 60.0));
        acc += buf;

    } else {

        std::snprintf(buf, sizeof(buf), "      last Note Off (wall): (none)\n");
        acc += buf;
    }

    if (times.lastEndOfTrackMicros != -1) {
        
        double lastEndOfTrackSec = times.lastEndOfTrackMicros / 1e6;
        double lastEndOfTrackMin = lastEndOfTrackSec / 60.0;
        double lastEndOfTrackHr = lastEndOfTrackMin / 60.0;
        std::snprintf(buf, sizeof(buf), "  last End Of Track (wall): %.0f:%02.0f:%05.2f\n", std::floor(lastEndOfTrackHr), std::floor(std::fmod(lastEndOfTrackMin, 60.0)), std::fmod(lastEndOfTrackSec, 60.0));
        acc += buf;

    } else {
        std::snprintf(buf, sizeof(buf), "  last End Of Track (wall): (none)\n");
        acc += buf;
    }

    std::snprintf(buf, sizeof(buf), "     last Note On (micros): %.17f\n", times.lastNoteOnMicros);
    acc += buf;

    std::snprintf(buf, sizeof(buf), "    last Note Off (micros): %.17f\n", times.lastNoteOffMicros);
    acc += buf;

    std::snprintf(buf, sizeof(buf), "last End Of Track (micros): %.17f\n", times.lastEndOfTrackMicros);
    acc += buf;


    std::snprintf(buf, sizeof(buf), "      last Note On (ticks): %d\n", times.lastNoteOnTick);
    acc += buf;

    std::snprintf(buf, sizeof(buf), "     last Note Off (ticks): %d\n", times.lastNoteOffTick);
    acc += buf;

    std::snprintf(buf, sizeof(buf), " last End Of Track (ticks): %d\n", times.lastEndOfTrackTick);
    acc += buf;


    return acc;
}


#pragma clang diagnostic pop








