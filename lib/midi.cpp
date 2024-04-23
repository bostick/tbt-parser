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

#include "tbt-parser/midi.h"

#include "tbt-parser/rational.h"
#include "tbt-parser/tbt-parser-util.h"

#include "common/assert.h"
#include "common/check.h"
#include "common/file.h"
#include "common/logging.h"

#include <algorithm> // for remove
#include <set>
#include <cinttypes>
#include <cmath> // for round, floor, fmod
#include <cstring> // for memcmp


#define TAG "midi"


const std::array<uint8_t, 8> STRING_MIDI_NOTE = {
    0x28, // MIDI note for open E string
    0x2d, // MIDI note for open A string
    0x32, // MIDI note for open D string
    0x37, // MIDI note for open G string
    0x3b, // MIDI note for open B string
    0x40, // MIDI note for open e string
    0x00,
    0x00,
};

const std::array<uint8_t, 6> STRING_MIDI_NOTE_LE6A = {
    0x40, // MIDI note for open e string
    0x3b, // MIDI note for open B string
    0x37, // MIDI note for open G string
    0x32, // MIDI note for open D string
    0x2d, // MIDI note for open A string
    0x28, // MIDI note for open E string
};

const uint8_t MUTED = 0x11; // 17
const uint8_t STOPPED = 0x12; // 18


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
                    t.metadata.tracks[track].midiChannel),
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
        
        uint8_t firstAvailableChannel = availableChannels[0];
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
    std::map<uint32_t, std::map<rational, uint16_t> > &tempoMap) {

    auto flooredActualSpace = actualSpace.floor();

    auto flooredActualSpaceI = flooredActualSpace.to_uint32();

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


template <uint8_t VERSION, bool HASALTERNATETIMEREGIONS, typename tbt_file_t, size_t STRINGS_PER_TRACK>
void
computeTempoMap(
    const tbt_file_t &t,
    std::map<uint32_t, std::map<rational, uint16_t> > &tempoMap) {

    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        uint32_t trackSpaceCount;
        if constexpr (0x70 <= VERSION) {
            trackSpaceCount = t.metadata.tracks[track].spaceCount;
        } else if constexpr (VERSION == 0x6f) {
            trackSpaceCount = t.header.spaceCount;
        } else {
            trackSpaceCount = 4000;
        }

        const auto &maps = t.body.mapsList[track];

        rational actualSpace = 0;

        for (uint32_t space = 0; space < trackSpaceCount;) {

            if constexpr (VERSION == 0x72) {

                const auto &it = maps.trackEffectChangesMap.find(space);
                if (it != maps.trackEffectChangesMap.end()) {
                    
                    const auto &changes = it->second;

                    for (const auto &p : changes) {

                        auto effect = p.first;

                        if (effect == TEMPO) {

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
    uint32_t open;
    int repeats;
};


struct repeat_open_struct {
    rational actualSpace;
    uint32_t space;
};


template <uint8_t VERSION, typename tbt_file_t>
void
computeRepeats(
    const tbt_file_t &t,
    uint32_t barsSpaceCount,
    std::vector<std::set<uint32_t> > &openSpaceSets,
    std::vector<std::map<uint32_t, repeat_close_struct> > &repeatCloseMaps) {

    //
    // Setup repeats
    //

    for (uint8_t track = 0; track < t.header.trackCount + 1; track++) { // track count, + 1 for tempo track

        repeatCloseMaps.push_back( {} );

        openSpaceSets.push_back( {} );
    }

    uint32_t lastOpenSpace = 0;

    bool currentlyOpen = false;
    bool savedClose = false;
    uint8_t savedRepeats = 0;

    for (uint32_t space = 0; space < barsSpaceCount;) {

        //
        // Setup repeats:
        // setup repeatCloseMap and openSpaceSet
        //
        if constexpr (0x70 <= VERSION) {

            const auto &barsMapIt = t.body.barsMap.find(space);
            if (barsMapIt != t.body.barsMap.end()) {

                auto bar = barsMapIt->second;

                if (savedClose) {

                    for (uint8_t track = 0; track < t.header.trackCount + 1; track++) { // track count, + 1 for tempo track

                        if (openSpaceSets[track].find(lastOpenSpace) == openSpaceSets[track].end()) {
                        
                            LOGW("there was no repeat open at %d", lastOpenSpace);
                            
                            openSpaceSets[track].insert(lastOpenSpace);
                        }

                        repeatCloseMaps[track][space] = { lastOpenSpace, savedRepeats };
                    }

                    savedClose = false;

                    lastOpenSpace = space;
                }

                if ((bar[0] & CLOSEREPEAT_MASK_GE70) == CLOSEREPEAT_MASK_GE70) {

                    //
                    // save for next bar line
                    //

                    savedClose = true;
                    savedRepeats = bar[1];

                    currentlyOpen = false;
                }

                if ((bar[0] & OPENREPEAT_MASK_GE70) == OPENREPEAT_MASK_GE70) {

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

            const auto &barsMapIt = t.body.barsMap.find(space);
            if (barsMapIt != t.body.barsMap.end()) {

                auto bar = barsMapIt->second;

                auto change = static_cast<tbt_bar_line>(bar[0] & 0b00001111);

                switch (change) {
                case CLOSE: {
                    
                    auto value = (bar[0] & 0b11110000) >> 4;

                    for (uint8_t track = 0; track < t.header.trackCount + 1; track++) { // track count, + 1 for tempo track
                        
                        if (openSpaceSets[track].find(lastOpenSpace) == openSpaceSets[track].end()) {
                        
                            LOGW("there was no repeat open at %d", lastOpenSpace);
                            
                            openSpaceSets[track].insert(lastOpenSpace);
                        }
                        
                        repeatCloseMaps[track][space + 1] = { lastOpenSpace, value };
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

                    ASSERT(false);
                    
                    break;
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

                repeatCloseMaps[track][barsSpaceCount] = { lastOpenSpace, savedRepeats };
            }

            savedClose = false;
        }
    }
}


template <uint8_t VERSION, bool HASALTERNATETIMEREGIONS, typename tbt_file_t, size_t STRINGS_PER_TRACK>
Status
TconvertToMidi(
    const tbt_file_t &t,
    midi_file &out) {

    uint32_t barsSpaceCount;
    if constexpr (0x70 <= VERSION) {
        barsSpaceCount = t.body.barsSpaceCount;
    } else if constexpr (VERSION == 0x6f) {
        barsSpaceCount = t.header.spaceCount;
    } else {
        barsSpaceCount = 4000;
    }

    //
    // compute tempo map
    //
    // flooredActualSpace -> ( actualSpace -> tempo )
    //
    // there can be more than one tempo change mapped to the same flooredActualSpace
    // so this needs to be a map of actualSpace -> tempo
    //
    std::map<uint32_t, std::map<rational, uint16_t> > tempoMap;

    computeTempoMap<VERSION, HASALTERNATETIMEREGIONS, tbt_file_t, STRINGS_PER_TRACK>(t, tempoMap);

    //
    // compute channel map
    //
    std::map<uint8_t, uint8_t> channelMap;

    computeChannelMap<VERSION>(t, channelMap);

    //
    // for each track:
    //   set of spaces that repeat opens occur
    //
    std::vector<std::set<uint32_t> > openSpaceSets;

    //
    // for each track, including tempo track:
    //   actual space of close -> repeat_close_struct
    //
    std::vector<std::map<uint32_t, repeat_close_struct> > repeatCloseMaps;

    computeRepeats<VERSION, tbt_file_t>(t, barsSpaceCount, openSpaceSets, repeatCloseMaps);


    out.header = midi_header{
        1, // format
        static_cast<uint16_t>(t.header.trackCount + 1), // track count, + 1 for tempo track
        TICKS_PER_BEAT.to_uint16() // division
    };


    std::vector<midi_track_event> tmp;

    uint32_t tickCount;

    //
    // Track 0
    //
    {
        //
        // Emit events for tempo track
        //
        
        rational tick = 0;
        
        rational lastEventTick = 0;
        
        tmp.clear();

        auto diff = (tick - lastEventTick).round();

        tmp.push_back(TrackNameEvent{
            diff.to_int32(), // delta time
            "tbt-parser MIDI - Track 0"
        });

        lastEventTick += diff;

        diff = (tick - lastEventTick).round();

        tmp.push_back(TimeSignatureEvent{
            diff.to_int32(), // delta time
            4, // numerator
            2, // denominator (as 2^d)
            24, // ticks per metronome click
            8, // notated 32-notes in MIDI quarter notes
        });

        lastEventTick += diff;

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
            // TabIt uses floor(), but using round() is more accurate
            //
            // auto microsPerBeat = (MICROS_PER_MINUTE / tempoBPM).round();
            auto microsPerBeat = (MICROS_PER_MINUTE / tempoBPM).floor();

            diff = (tick - lastEventTick).round();

            tmp.push_back(TempoChangeEvent{
                diff.to_int32(), // delta time
                microsPerBeat.to_uint32()
            });

            lastEventTick += diff;
        }

        auto &repeatCloseMap = repeatCloseMaps[0];

        for (uint32_t space = 0; space < barsSpaceCount + 1;) { // space count, + 1 for handling repeats at end

            //
            // handle any repeat closes first
            //

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

                    //
                    // jump to the repeat open
                    //

                    space = r.open;

                    r.repeats--;

                    continue;
                }
            }
            
            //
            // Emit tempo changes
            //

            const auto &tempoMapIt = tempoMap.find(space);
            if (tempoMapIt != tempoMap.end()) {

                //
                // map of tempo changes at this floored space
                //
                auto &m = tempoMapIt->second;

                for (const auto &mIt : m) {

                    auto actualSpace = mIt.first;
                    auto tempoBPM = mIt.second;

                    auto spaceDiff = (actualSpace - space);

                    ASSERT(spaceDiff.is_nonnegative());

                    //
                    // TabIt uses floor(), but using round() is more accurate
                    //
                    // auto microsPerBeat = (MICROS_PER_MINUTE / tempoBPM).round();
                    auto microsPerBeat = (MICROS_PER_MINUTE / tempoBPM).floor();

                    //
                    // increment by spaceDiff
                    //
                    tick += spaceDiff * TICKS_PER_SPACE;

                    diff = (tick - lastEventTick).round();

                    tmp.push_back(TempoChangeEvent{
                        diff.to_int32(), // delta time
                        microsPerBeat.to_uint32()
                    });

                    lastEventTick += diff;

                    //
                    // restore tick
                    //
                    tick -= spaceDiff * TICKS_PER_SPACE;
                }
            }
            
            //     every bar: lyric for current bars ?
            //     every space: lyric for current tempo
            //     tmp;

            // tmp.insert(tmp.end(), { 0x00, 0xFF, 0x05, 0x11 }); // lyric
            // tmp.insert(tmp.end(), { 0x73, 0x70, 0x61, 0x63, 0x65, 0x20, 0x30, 0x20, 0x74, 0x65, 0x6d, 0x70, 0x6f, 0x20, 0x31, 0x32, 0x30, });
        
            tick += TICKS_PER_SPACE;

            space++;

        } // for space

        //
        // now backup 1 space
        //

        tick -= TICKS_PER_SPACE;

        for (const auto &repeatCloseMapIt : repeatCloseMap) {
            const auto &r = repeatCloseMapIt.second;
            ASSERT(r.repeats == 0);
        }

        diff = (tick - lastEventTick).round();

        tmp.push_back(EndOfTrackEvent{
            diff.to_int32() // delta time
        });

        lastEventTick += diff;

        out.tracks.push_back(tmp);

        tickCount = tick.to_uint32();

    } // Track 0

    //
    // the actual tracks
    //
    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        uint8_t channel = channelMap[track];

        const auto &trackMetadata = t.metadata.tracks[track];

        uint8_t volume = trackMetadata.volume;

        uint8_t midiBank;
        if constexpr (0x6e <= VERSION) {
            midiBank = trackMetadata.midiBank;
        } else {
            midiBank = 0;
        }

        bool dontLetRing = ((trackMetadata.cleanGuitar & 0b10000000) == 0b10000000);
        uint8_t midiProgram = (trackMetadata.cleanGuitar & 0b01111111);

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
            pitchBend = trackMetadata.pitchBend; // -2400 to 2400
            
        } else {
            
            modulation = 0;
            pitchBend = 0;
        }
        //
        // 0b0000000000000000 to 0b0011111111111111 (0 to 16383)
        //
        // important that value of 0 goes to value of 0x2000 (8192)
        //
        pitchBend = static_cast<int16_t>(round(((static_cast<double>(pitchBend) + 2400.0) * 16383.0) / (2.0 * 2400.0)));

        rational tick = 0;
        
        rational actualSpace = 0;

        rational lastEventTick = 0;

        std::array<uint8_t, STRINGS_PER_TRACK> currentlyPlayingStrings{};

        tmp.clear();

        auto diff = (tick - lastEventTick).round();

        tmp.push_back(TrackNameEvent{
            diff.to_int32(), // delta time
            std::string("tbt-parser MIDI - Track ") + std::to_string(track + 1)
        });

        lastEventTick += diff;

        if (midiBank != 0) {

            //
            // Bank Select MSB and Bank Select LSB are special and do not really mean MSB/LSB
            // TabIt only sends MSB
            //
            uint8_t midiBankMSB = midiBank;

            diff = (tick - lastEventTick).round();

            tmp.push_back(BankSelectMSBEvent{
                diff.to_int32(), // delta time
                channel,
                midiBankMSB
            });

            lastEventTick += diff;
        }

        diff = (tick - lastEventTick).round();

        tmp.push_back(ProgramChangeEvent{
            diff.to_int32(), // delta time
            channel,
            midiProgram
        });

        lastEventTick += diff;

        diff = (tick - lastEventTick).round();

        tmp.push_back(PanEvent{
            diff.to_int32(), // delta time
            channel,
            pan
        });

        lastEventTick += diff;

        diff = (tick - lastEventTick).round();

        tmp.push_back(ReverbEvent{
            diff.to_int32(), // delta time
            channel,
            reverb
        });

        lastEventTick += diff;

        diff = (tick - lastEventTick).round();

        tmp.push_back(ChorusEvent{
            diff.to_int32(), // delta time
            channel,
            chorus
        });

        lastEventTick += diff;

        diff = (tick - lastEventTick).round();

        tmp.push_back(ModulationEvent{
            diff.to_int32(), // delta time
            channel,
            modulation
        });

        lastEventTick += diff;

        //
        // RPN Parameter MSB 0, RPN Parameter LSB 0 = RPN Parameter 0
        // RPN Parameter 0 is standardized for pitch bend range
        //

        diff = (tick - lastEventTick).round();

        tmp.push_back(RPNParameterMSBEvent{
            diff.to_int32(), // delta time
            channel,
            0
        });

        lastEventTick += diff;

        diff = (tick - lastEventTick).round();

        tmp.push_back(RPNParameterLSBEvent{
            diff.to_int32(), // delta time
            channel,
            0
        });

        lastEventTick += diff;

        diff = (tick - lastEventTick).round();

        tmp.push_back(DataEntryMSBEvent{
            diff.to_int32(), // delta time
            channel,
            24 // semi-tones
        });

        lastEventTick += diff;

        diff = (tick - lastEventTick).round();

        tmp.push_back(DataEntryLSBEvent{
            diff.to_int32(), // delta time
            channel,
            0 // cents
        });

        lastEventTick += diff;

        diff = (tick - lastEventTick).round();

        tmp.push_back(PitchBendEvent{
            diff.to_int32(), // delta time
            channel,
            pitchBend
        });

        lastEventTick += diff;

        uint32_t trackSpaceCount;
        if constexpr (0x70 <= VERSION) {
            trackSpaceCount = trackMetadata.spaceCount;
        } else if constexpr (VERSION == 0x6f) {
            trackSpaceCount = t.header.spaceCount;
        } else {
            trackSpaceCount = 4000;
        }

        //
        // flooredActualSpace -> repeat_open_struct
        //
        std::map<uint32_t, repeat_open_struct> repeatOpenMap;

        auto &openSpaceSet = openSpaceSets[track + 1];

        auto &repeatCloseMap = repeatCloseMaps[track + 1];

        const auto &maps = t.body.mapsList[track];

        for (uint32_t space = 0; space < trackSpaceCount + 1;) { // space count, + 1 for handling repeats at end

            auto flooredActualSpace = actualSpace.floor();

            auto flooredActualSpaceI = flooredActualSpace.to_uint32();

            {
                //
                // handle any repeat closes first
                //

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

                        auto spaceDiff = (actualSpace - flooredActualSpace);

                        ASSERT(spaceDiff.is_nonnegative());

                        if (spaceDiff.is_positive()) {

                            LOGW("repeat CLOSE at non-integral space: %f", actualSpace.to_double());

                            //
                            // overshot the repeat close
                            //
                            // now backup by the difference between flooredActualSpace and actualSpace
                            //
                            tick -= spaceDiff * TICKS_PER_SPACE;
                        }

                        //
                        // jump to the repeat open
                        //

                        ASSERT(repeatOpenMap.contains(r.open));

                        const auto &openStruct = repeatOpenMap[r.open];

                        space = openStruct.space;

                        actualSpace = openStruct.actualSpace;

                        spaceDiff = (actualSpace - r.open);

                        ASSERT(spaceDiff.is_nonnegative());

                        if (spaceDiff.is_positive()) {

                            LOGW("repeat OPEN at non-integral space: %f", actualSpace.to_double());

                            //
                            // undershot the repeat open
                            //
                            // now scoot up by the difference between actualSpace and r.open
                            //
                            tick += spaceDiff * TICKS_PER_SPACE;
                        }

                        r.repeats--;

                        continue;
                    }
                }
            }

            {
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
            }

            const auto &notesMapIt = maps.notesMap.find(space);

            //
            // Emit note offs
            //
            {
                if (notesMapIt != maps.notesMap.end()) {

                    const auto &onVsqs = notesMapIt->second;

                    std::array<uint8_t, STRINGS_PER_TRACK> offVsqs{};

                    //
                    // Compute note offs
                    //
                    if (dontLetRing) {

                        //
                        // There may be string effects, but no note events
                        // This will still be in the notesMap, but should not affect notes because of dontLetRing
                        // i.e., simply checking for something in notesMap is not sufficient
                        //
                        bool anyEvents = false;
                        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                            uint8_t event = onVsqs[string];

                            if (event != 0) {
                                anyEvents = true;
                                break;
                            }
                        }
                        
                        if (anyEvents) {
                            
                            offVsqs = currentlyPlayingStrings;

                            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                                uint8_t on = onVsqs[string];

                                if (on == 0) {

                                    currentlyPlayingStrings[string] = 0;

                                } else if (on == MUTED ||
                                        on == STOPPED) {

                                    currentlyPlayingStrings[string] = 0;

                                } else {

                                    ASSERT(on >= 0x80);

                                    currentlyPlayingStrings[string] = on;
                                }
                            }
                        }

                    } else {

                        for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                            uint8_t current = currentlyPlayingStrings[string];

                            if (current == 0) {

                                uint8_t on = onVsqs[string];

                                if (on == 0) {

                                    offVsqs[string] = 0;

                                } else if (on == MUTED ||
                                        on == STOPPED) {

                                    offVsqs[string] = 0;

                                } else {

                                    ASSERT(on >= 0x80);

                                    offVsqs[string] = 0;
                                    currentlyPlayingStrings[string] = on;
                                }

                            } else {

                                ASSERT(current >= 0x80);

                                uint8_t on = onVsqs[string];

                                if (on == 0) {

                                    offVsqs[string] = 0;

                                } else if (on == MUTED ||
                                        on == STOPPED) {

                                    offVsqs[string] = current;
                                    currentlyPlayingStrings[string] = 0;

                                } else {

                                    ASSERT(on >= 0x80);

                                    offVsqs[string] = current;
                                    currentlyPlayingStrings[string] = on;
                                }
                            }
                        }
                    }

                    //
                    // Emit note offs
                    //
                    for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                        uint8_t off = offVsqs[string];

                        if (off == 0) {
                            continue;
                        }

                        ASSERT(off >= 0x80);

                        uint8_t stringNote = off - 0x80;

                        stringNote += static_cast<uint8_t>(trackMetadata.tuning[string]);

                        if constexpr (0x6e <= VERSION) {
                            stringNote += static_cast<uint8_t>(trackMetadata.transposeHalfSteps);
                        }
                        
                        uint8_t midiNote;
                        if constexpr (0x6b <= VERSION) {
                            midiNote = stringNote + STRING_MIDI_NOTE[string];
                        } else {
                            midiNote = stringNote + STRING_MIDI_NOTE_LE6A[string];
                        }

                        diff = (tick - lastEventTick).round();

                        tmp.push_back(NoteOffEvent{
                            diff.to_int32(), // delta time
                            channel,
                            midiNote,
                            0
                        });

                        lastEventTick += diff;
                    }
                }
            }

            //
            // Emit track effects
            //
            {
                if constexpr (VERSION == 0x72) {

                    const auto &trackEffectChangesIt = maps.trackEffectChangesMap.find(space);
                    if (trackEffectChangesIt != maps.trackEffectChangesMap.end()) {

                        const auto &changes = trackEffectChangesIt->second;

                        for (const auto &p : changes) {

                            auto effect = p.first;
                            auto value = p.second;

                            switch(effect) {
                            case INSTRUMENT: {

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
                                    
                                    diff = (tick - lastEventTick).round();

                                    tmp.push_back(BankSelectMSBEvent{
                                        diff.to_int32(), // delta time
                                        channel,
                                        midiBankMSB
                                    });

                                    lastEventTick += diff;
                                }

                                diff = (tick - lastEventTick).round();

                                tmp.push_back(ProgramChangeEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    midiProgram
                                });

                                lastEventTick += diff;

                                break;
                            }
                            case VOLUME: {

                                auto newVolume = value;

                                volume = static_cast<uint8_t>(newVolume);

                                break;
                            }
                            case TEMPO:
                                //
                                // already handled
                                //
                                break;
                            case STROKE_DOWN:
                            case STROKE_UP:
                                //
                                // nothing to do
                                //
                                break;
                            case PAN: {
                                
                                auto newPan = value;

                                diff = (tick - lastEventTick).round();

                                tmp.push_back(PanEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    static_cast<uint8_t>(newPan)
                                });

                                lastEventTick += diff;

                                break;
                            }
                            case CHORUS: {
                                
                                auto newChorus = value;

                                diff = (tick - lastEventTick).round();

                                tmp.push_back(ChorusEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    static_cast<uint8_t>(newChorus)
                                });

                                lastEventTick += diff;

                                break;
                            }
                            case REVERB: {
                                
                                auto newReverb = value;

                                diff = (tick - lastEventTick).round();

                                tmp.push_back(ReverbEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    static_cast<uint8_t>(newReverb)
                                });

                                lastEventTick += diff;

                                break;
                            }
                            case MODULATION: {
                                
                                auto newModulation = value;

                                diff = (tick - lastEventTick).round();

                                tmp.push_back(ModulationEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    static_cast<uint8_t>(newModulation)
                                });

                                lastEventTick += diff;

                                break;
                            }
                            case PITCH_BEND: {
                                
                                int16_t newPitchBend = static_cast<int16_t>(value); // -2400 to 2400
                                //
                                // 0b0000000000000000 to 0b0011111111111111 (0 to 16383)
                                //
                                // important that value of 0 goes to value of 0x2000 (8192)
                                //
                                newPitchBend = static_cast<int16_t>(round(((static_cast<double>(newPitchBend) + 2400.0) * 16383.0) / (2.0 * 2400.0)));

                                diff = (tick - lastEventTick).round();

                                tmp.push_back(PitchBendEvent{
                                    diff.to_int32(), // delta time
                                    channel,
                                    newPitchBend
                                });

                                lastEventTick += diff;

                                break;
                            }
                            default:
                                ASSERT(false);
                                break;
                            }
                        }
                    }

                } else {

                    if (notesMapIt != maps.notesMap.end()) {

                        const auto &vsqs = notesMapIt->second;

                        auto trackEffect = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 0];

                        switch (trackEffect) {
                        case 0x00:
                            //
                            // skip
                            //
                            break;
                        case 'I': { // Instrument change

                            auto newInstrument = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            dontLetRing = ((newInstrument & 0b10000000) == 0b10000000);
                            midiProgram = (newInstrument & 0b01111111);

                            diff = (tick - lastEventTick).round();

                            tmp.push_back(ProgramChangeEvent{
                                diff.to_int32(), // delta time
                                channel,
                                midiProgram
                            });

                            lastEventTick += diff;

                            break;
                        }
                        case 'V': { // Volume change

                            auto newVolume = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            volume = newVolume;

                            break;
                        }
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

                            auto newChorus = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            diff = (tick - lastEventTick).round();

                            tmp.push_back(ChorusEvent{
                                diff.to_int32(), // delta time
                                channel,
                                newChorus
                            });

                            lastEventTick += diff;

                            break;
                        }
                        case 'P': { // Pan change

                            auto newPan = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            diff = (tick - lastEventTick).round();

                            tmp.push_back(PanEvent{
                                diff.to_int32(), // delta time
                                channel,
                                newPan
                            });

                            lastEventTick += diff;

                            break;
                        }
                        case 'R': { // Reverb change

                            auto newReverb = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            diff = (tick - lastEventTick).round();

                            tmp.push_back(ReverbEvent{
                                diff.to_int32(), // delta time
                                channel,
                                newReverb
                            });

                            lastEventTick += diff;

                            break;
                        }
                        default:
                            ASSERT(false);
                            break;
                        }
                    }
                }
            }

            //
            // Emit note ons
            //
            {
                if (notesMapIt != maps.notesMap.end()) {

                    const auto &onVsqs = notesMapIt->second;
                    
                    for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                        uint8_t on = onVsqs[string];

                        if (on == 0 ||
                            on == MUTED ||
                            on == STOPPED) {
                            continue;
                        }

                        ASSERT(on >= 0x80);

                        uint8_t stringNote = on - 0x80;
                        
                        stringNote += static_cast<uint8_t>(trackMetadata.tuning[string]);
                        
                        if constexpr (0x6e <= VERSION) {
                            stringNote += static_cast<uint8_t>(trackMetadata.transposeHalfSteps);
                        }

                        uint8_t midiNote;
                        if constexpr (0x6b <= VERSION) {
                            midiNote = stringNote + STRING_MIDI_NOTE[string];
                        } else {
                            midiNote = stringNote + STRING_MIDI_NOTE_LE6A[string];
                        }

                        diff = (tick - lastEventTick).round();

                        tmp.push_back(NoteOnEvent{
                            diff.to_int32(), // delta time
                            channel,
                            midiNote,
                            volume // velocity
                        });

                        lastEventTick += diff;
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

                        tick += (atr * TICKS_PER_SPACE);

                        space++;

                        actualSpace += atr;

                    } else {

                        tick += TICKS_PER_SPACE;

                        space++;

                        ++actualSpace;
                    }

                } else {

                    tick += TICKS_PER_SPACE;
                    
                    space++;

                    actualSpace = space;
                }
            }

        } // for space

        //
        // now backup 1 space
        //

        tick -= TICKS_PER_SPACE;
        --actualSpace;

        ASSERT(tick == tickCount);
        ASSERT(actualSpace == barsSpaceCount);
        for (const auto &repeatCloseMapIt : repeatCloseMap) {
            const auto &r = repeatCloseMapIt.second;
            ASSERT(r.repeats == 0);
        }
        ASSERT(openSpaceSet.empty());

        //
        // Emit any final note offs
        //
        {
            auto offVsqs = currentlyPlayingStrings;

            for (uint8_t string = 0; string < trackMetadata.stringCount; string++) {

                uint8_t off = offVsqs[string];

                if (off == 0) {
                    continue;
                }

                ASSERT(off >= 0x80);

                uint8_t stringNote = off - 0x80;

                stringNote += static_cast<uint8_t>(trackMetadata.tuning[string]);

                if constexpr (0x6e <= VERSION) {
                    stringNote += static_cast<uint8_t>(trackMetadata.transposeHalfSteps);
                }

                uint8_t midiNote;
                if constexpr (0x6b <= VERSION) {
                    midiNote = stringNote + STRING_MIDI_NOTE[string];
                } else {
                    midiNote = stringNote + STRING_MIDI_NOTE_LE6A[string];
                }

                diff = (tick - lastEventTick).round();

                tmp.push_back(NoteOffEvent{
                    diff.to_int32(), // delta time
                    channel,
                    midiNote,
                    0
                });

                lastEventTick += diff;
            }
        }

        diff = (tick - lastEventTick).round();

        tmp.push_back(EndOfTrackEvent{
            diff.to_int32() // delta time
        });

        lastEventTick += diff;

        out.tracks.push_back(tmp);

    } // for track
    
    return OK;
}


Status
convertToMidi(
    const tbt_file &t,
    midi_file &out) {

    auto versionNumber = tbtFileVersionNumber(t);

    switch (versionNumber) {
    case 0x72: {

        auto t71 = std::get<tbt_file71>(t);
        
        if ((t71.header.featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            return TconvertToMidi<0x72, true, tbt_file71, 8>(t71, out);
        } else {
            return TconvertToMidi<0x72, false, tbt_file71, 8>(t71, out);
        }
    }
    case 0x71: {
        
        auto t71 = std::get<tbt_file71>(t);
        
        if ((t71.header.featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            return TconvertToMidi<0x71, true, tbt_file71, 8>(t71, out);
        } else {
            return TconvertToMidi<0x71, false, tbt_file71, 8>(t71, out);
        }
    }
    case 0x70: {
        
        auto t70 = std::get<tbt_file70>(t);
        
        if ((t70.header.featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            return TconvertToMidi<0x70, true, tbt_file70, 8>(t70, out);
        } else {
            return TconvertToMidi<0x70, false, tbt_file70, 8>(t70, out);
        }
    }
    case 0x6f: {
        
        auto t6f = std::get<tbt_file6f>(t);
        
        return TconvertToMidi<0x6f, false, tbt_file6f, 8>(t6f, out);
    }
    case 0x6e: {
        
        auto t6e = std::get<tbt_file6e>(t);
        
        return TconvertToMidi<0x6e, false, tbt_file6e, 8>(t6e, out);
    }
    case 0x6b: {
        
        auto t6b = std::get<tbt_file6b>(t);
        
        return TconvertToMidi<0x6b, false, tbt_file6b, 8>(t6b, out);
    }
    case 0x6a: {
        
        auto t6a = std::get<tbt_file6a>(t);
        
        return TconvertToMidi<0x6a, false, tbt_file6a, 6>(t6a, out);
    }
    case 0x69: {
        
        auto t68 = std::get<tbt_file68>(t);
        
        return TconvertToMidi<0x69, false, tbt_file68, 6>(t68, out);
    }
    case 0x68: {
        
        auto t68 = std::get<tbt_file68>(t);
        
        return TconvertToMidi<0x68, false, tbt_file68, 6>(t68, out);
    }
    case 0x67: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TconvertToMidi<0x67, false, tbt_file65, 6>(t65, out);
    }
    case 0x66: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TconvertToMidi<0x66, false, tbt_file65, 6>(t65, out);
    }
    case 0x65: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TconvertToMidi<0x65, false, tbt_file65, 6>(t65, out);
    }
    default:

        ASSERT(false);
        
        return ERR;
    }
}


struct EventExportVisitor {
    
    std::vector<uint8_t> &tmp;
    
    void operator()(const TimeSignatureEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            0xff, // meta event
            0x58, // time signature
            0x04, 
            e.numerator,
            e.denominator,
            e.ticksPerMetronomeClick,
            e.notated32notesInMIDIQuarterNotes
        });
    }

    void operator()(const TempoChangeEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            0xff, // meta event
            0x51, // tempo change
            //
            // FluidSynth hard-codes length of 3, so just do the same
            //
            0x03 // microsPerBeatBytes size VLQ
        });

        toDigitsBEOnly3(e.microsPerBeat, tmp); // only last 3 bytes of microsPerBeatBytes
    }

    void operator()(const EndOfTrackEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            0xff, // meta event
            0x2f, // end of track
            0x00
        });
    }

    void operator()(const ProgramChangeEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xc0 | e.channel), // program change
            e.midiProgram
        });
    }

    void operator()(const PanEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xb0 | e.channel), // control event
            0x0a, // pan
            e.pan
        });
    }

    void operator()(const ReverbEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xb0 | e.channel), // control event
            0x5b, // reverb
            e.reverb
        });
    }

    void operator()(const ChorusEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xb0 | e.channel), // control event
            0x5d, // chorus
            e.chorus
        });
    }

    void operator()(const ModulationEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xb0 | e.channel), // control event
            0x01, // modulation
            e.modulation
        });
    }

    void operator()(const RPNParameterMSBEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xb0 | e.channel), // control event
            0x65, // RPN Parameter MSB
            e.rpnParameterMSB
        });
    }

    void operator()(const RPNParameterLSBEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xb0 | e.channel), // control event
            0x64, // RPN Parameter LSB
            e.rpnParameterLSB
        });
    }

    void operator()(const DataEntryMSBEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xb0 | e.channel), // control event
            0x06, // Data Entry MSB
            e.dataEntryMSB
        });
    }

    void operator()(const DataEntryLSBEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xb0 | e.channel), // control event
            0x26, // Data Entry LSB
            e.dataEntryLSB
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

    void operator()(const NullEvent &e) {
        (void)e;
    }

    void operator()(const TrackNameEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            0xff, // meta event
            0x03, // Sequence/Track Name
        });

        toVLQ(static_cast<uint32_t>(e.name.size()), tmp); // len

        tmp.insert(tmp.end(), e.name.data(), e.name.data() + e.name.size());
    }

    void operator()(const BankSelectMSBEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xb0 | e.channel), // control event
            0x00, // Bank Select MSB
            e.bankSelectMSB
        });
    }

    void operator()(const BankSelectLSBEvent &e) {

        toVLQ(static_cast<uint32_t>(e.deltaTime), tmp); // delta time

        tmp.insert(tmp.end(), {
            static_cast<uint8_t>(0xb0 | e.channel), // control event
            0x20, /// Bank Select LSB
            e.bankSelectLSB
        });
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
        out.insert(out.end(), { 'M', 'T', 'h', 'd' }); // type

        toDigitsBE(static_cast<uint32_t>(6), out); // length

        toDigitsBE(static_cast<uint16_t>(m.header.format), out); // format

        toDigitsBE(static_cast<uint16_t>(m.header.trackCount), out); // track count

        toDigitsBE(static_cast<uint16_t>(m.header.division), out); // division
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

        out.insert(out.end(), { 'M', 'T', 'r', 'k' }); // type

        toDigitsBE(static_cast<uint32_t>(tmp.size()), out); // length

        out.insert(out.end(), tmp.cbegin(), tmp.cend());
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



struct chunk {
    std::array<uint8_t, 4> type;
    std::vector<uint8_t> data;
};


Status
parseChunk(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    chunk &out) {

    CHECK(it + 4 <= end, "out of data");

    out.type = { *it++, *it++, *it++, *it++ };

    CHECK(it + 4 <= end, "out of data");

    uint32_t len = parseBE4(*it++, *it++, *it++, *it++);
    
    CHECK(it + len <= end, "out of data");
    
    out.data = std::vector<uint8_t>(it, it + len);
    it += len;

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

    CHECK(memcmp(c.type.data(), "MThd", 4) == 0, "expected MThd type");

    auto it2 = c.data.cbegin();

    auto end2 = c.data.cend();

    CHECK(it2 + 2 <= end2, "out of data");

    out.header.format = parseBE2(*it2++, *it2++);

    CHECK(it2 + 2 <= end2, "out of data");

    out.header.trackCount = parseBE2(*it2++, *it2++);

    CHECK(it2 + 2 <= end2, "out of data");

    out.header.division = parseBE2(*it2++, *it2++);

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

        //
        // Polyphonic Key Pressure (Aftertouch)
        //
        
        CHECK(it + 1 <= end, "out of data");

        it += 1;

        out = NullEvent{};

        return OK;
    }
    case 0xb0: {

        //
        // control change
        //

        auto channel = lo;

        auto controller = b;

        CHECK(it + 1 <= end, "out of data");

        b = *it++;

        switch (controller) {
        case 0x00:
            out = BankSelectMSBEvent{deltaTime, channel, b};
            return OK;
        case 0x01:
            out = ModulationEvent{deltaTime, channel, b};
            return OK;
        case 0x06:
            out = DataEntryMSBEvent{deltaTime, channel, b};
            return OK;
        case 0x0a:
            out = PanEvent{deltaTime, channel, b};
            return OK;
        case 0x20:
            out = BankSelectLSBEvent{deltaTime, channel, b};
            return OK;
        case 0x26:
            out = DataEntryLSBEvent{deltaTime, channel, b};
            return OK;
        case 0x5b:
            out = ReverbEvent{deltaTime, channel, b};
            return OK;
        case 0x5d:
            out = ChorusEvent{deltaTime, channel, b};
            return OK;
        case 0x64:
            out = RPNParameterLSBEvent{deltaTime, channel, b};
            return OK;
        case 0x65:
            out = RPNParameterMSBEvent{deltaTime, channel, b};
            return OK;
        default:
            out = NullEvent{deltaTime};
            return OK;
        }
    }
    case 0xc0: {

        auto channel = lo;

        auto midiProgram = static_cast<uint8_t>(b & 0b01111111);

        out = ProgramChangeEvent{deltaTime, channel, midiProgram};

        return OK;
    }
    case 0xd0: {

        //
        // Channel Pressure (After-touch)
        //

        out = NullEvent{};

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

            //
            // SysEx
            //

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

            out = NullEvent{};

            return OK;

        } else if (lo == 0x0f) {

            //
            // meta event
            //

            uint32_t len;

            ret = parseVLQ(it, end, len);

            if (ret != OK) {
                return ret;
            }

            CHECK(it + len <= end, "out of data");

            switch (b) {
            case 0x2f: {

                CHECK(len == 0, "len != 0");

                out = EndOfTrackEvent{deltaTime};

                return OK;
            }
            case 0x51: {

                CHECK(len == 3, "len != 3");

                auto microsPerBeat = parseBE4(0, *it++, *it++, *it++);

                out = TempoChangeEvent{deltaTime, microsPerBeat};

                return OK;
            }
            case 0x58: {

                CHECK(len == 4, "len != 4");

                auto numerator = *it++;

                auto denominator = *it++;

                auto ticksPerMetronomeClick = *it++;

                auto notated32notesInMIDIQuarterNotes = *it++;

                out = TimeSignatureEvent{deltaTime, numerator, denominator, ticksPerMetronomeClick, notated32notesInMIDIQuarterNotes};

                return OK;
            }
            case 0x00: // Sequence Number
            case 0x01: // Text
            case 0x02: { // Copyright
                
                it += len;

                out = NullEvent{deltaTime};

                return OK;
            }
            case 0x03: { // Sequence/Track Name
                
                std::string str(it, it + len);
                it += len;

                out = TrackNameEvent{deltaTime, str};

                return OK;
            }
            case 0x04: // Instrument Name
            case 0x05: // Lyric
            case 0x06: // Marker
            case 0x07: // Cue Point
            case 0x08: // Program Name
            case 0x09: // Device Name
            case 0x20: // MIDI Channel
            case 0x21: // MIDI Port
            case 0x54: // SMPTE Offset
            case 0x59: // Key Signature
            case 0x7f: { // Sequencer Specific Meta-Event

                it += len;

                out = NullEvent{deltaTime};

                return OK;
            }
            default: {

                LOGE("unrecognized meta event byte: %d (0x%02x)", b, b);

                return ERR;
            }
            }

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

    CHECK(memcmp(c.type.data(), "MTrk", 4) == 0, "expected MTrk type");

    std::vector<midi_track_event> track;

    auto it2 = c.data.cbegin();

    auto end2 = c.data.cend();

    while (true) {

        midi_track_event e;

        ret = parseTrackEvent(it2, end2, running, e);

        if (ret != OK) {
            return ret;
        }

        if (!std::holds_alternative<NullEvent>(e)) {
            track.push_back(e);
        }

        if (std::holds_alternative<EndOfTrackEvent>(e)) {

            if (it2 != end2) {
                LOGW("bytes after EndOfTrack: %zu", (end2 - it2));
            }

            break;
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

    auto len = end - it;

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
    
    //
    // tick -> microsPerTick
    //
    std::map<rational, rational> &tempoMap;

    rational runningTick;

    EventFileTimesTempoMapVisitor(std::map<rational, rational> &tempoMapIn) :
        tempoMap(tempoMapIn),
        runningTick(0)
        {}

    void operator()(const TimeSignatureEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const TempoChangeEvent &e) {

        runningTick += e.deltaTime;

        auto newMicrosPerTick = rational(e.microsPerBeat) / TICKS_PER_BEAT;

        const auto &tempoMapIt = tempoMap.find(runningTick);
        if (tempoMapIt != tempoMap.end()) {

            auto microsPerTick = tempoMapIt->second;

            if (microsPerTick != newMicrosPerTick) {

                //
                // convert MicrosPerTick -> BeatsPerMinute
                //

                auto aBPM = (MICROS_PER_MINUTE / (microsPerTick * TICKS_PER_BEAT));

                auto bBPM = (MICROS_PER_MINUTE / (newMicrosPerTick * TICKS_PER_BEAT));

                LOGW("tick %f has conflicting tempo changes: %d, %d", runningTick.to_double(), aBPM.to_uint16(), bBPM.to_uint16());
            }
        }

        tempoMap[runningTick] = newMicrosPerTick;
    }

    void operator()(const EndOfTrackEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const ProgramChangeEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const PanEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const ReverbEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const ChorusEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const ModulationEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const RPNParameterMSBEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const RPNParameterLSBEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const DataEntryMSBEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const DataEntryLSBEvent &e) {
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

    void operator()(const NullEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const TrackNameEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const BankSelectMSBEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const BankSelectLSBEvent &e) {
        runningTick += e.deltaTime;
    }
};


struct EventFileTimesLastTicksVisitor {
    
    rational runningTick;

    //
    // important to start < 0, because 0 is a valid tick
    //
    rational lastNoteOnTick = -1;
    rational lastNoteOffTick = -1;
    rational lastEndOfTrackTick = -1;
    rational lastTempoChangeTick = -1;
    rational lastMicrosPerTick = 0;

    void operator()(const TimeSignatureEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const TempoChangeEvent &e) {
        
        runningTick += e.deltaTime;

        if (runningTick > lastTempoChangeTick) {
            lastTempoChangeTick = runningTick;
            lastMicrosPerTick = rational(e.microsPerBeat) / TICKS_PER_BEAT;
        }
    }

    void operator()(const EndOfTrackEvent &e) {
       
        runningTick += e.deltaTime;

        if (runningTick > lastEndOfTrackTick) {
            lastEndOfTrackTick = runningTick;
        }
    }

    void operator()(const ProgramChangeEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const PanEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const ReverbEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const ChorusEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const ModulationEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const RPNParameterMSBEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const RPNParameterLSBEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const DataEntryMSBEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const DataEntryLSBEvent &e) {
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

    void operator()(const NullEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const TrackNameEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const BankSelectMSBEvent &e) {
        runningTick += e.deltaTime;
    }

    void operator()(const BankSelectLSBEvent &e) {
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

    EventFileTimesTempoMapVisitor eventFileTimesTempoMapVisitor{ tempoMap };

    for (const auto &track : m.tracks) {

        eventFileTimesTempoMapVisitor.runningTick = 0;

        for (const auto &e : track) {
            std::visit(eventFileTimesTempoMapVisitor, e);
        }
    }


    EventFileTimesLastTicksVisitor v{};

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
        auto it = tempoMap.lower_bound(v.lastNoteOnTick);
        if (it != tempoMap.end()) {
            tempoMap[v.lastNoteOnTick] = it->second;
        } else {
            tempoMap[v.lastNoteOnTick] = v.lastMicrosPerTick;
        }
    }

    if (v.lastNoteOffTick != -1) {
        auto it = tempoMap.lower_bound(v.lastNoteOffTick);
        if (it != tempoMap.end()) {
            tempoMap[v.lastNoteOffTick] = it->second;
        } else {
            tempoMap[v.lastNoteOffTick] = v.lastMicrosPerTick;
        }
    }

    if (v.lastEndOfTrackTick != -1) {
        auto it = tempoMap.lower_bound(v.lastEndOfTrackTick);
        if (it != tempoMap.end()) {
            tempoMap[v.lastEndOfTrackTick] = it->second;
        } else {
            tempoMap[v.lastEndOfTrackTick] = v.lastMicrosPerTick;
        }
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


struct EventInfoVisitor {
    
    void operator()(const TimeSignatureEvent &e) {
        (void)e;
    }

    void operator()(const TempoChangeEvent &e) {
        (void)e;
    }

    void operator()(const EndOfTrackEvent &e) {
        (void)e;
    }

    void operator()(const ProgramChangeEvent &e) {
        (void)e;
    }

    void operator()(const PanEvent &e) {
        (void)e;
    }

    void operator()(const ReverbEvent &e) {
        (void)e;
    }

    void operator()(const ChorusEvent &e) {
        (void)e;
    }

    void operator()(const ModulationEvent &e) {
        (void)e;
    }

    void operator()(const RPNParameterMSBEvent &e) {
        (void)e;
    }

    void operator()(const RPNParameterLSBEvent &e) {
        (void)e;
    }

    void operator()(const DataEntryMSBEvent &e) {
        (void)e;
    }

    void operator()(const DataEntryLSBEvent &e) {
        (void)e;
    }

    void operator()(const PitchBendEvent &e) {
        (void)e;
    }

    void operator()(const NoteOffEvent &e) {
        (void)e;
    }

    void operator()(const NoteOnEvent &e) {
        (void)e;
    }

    void operator()(const NullEvent &e) {
        (void)e;
    }

    void operator()(const TrackNameEvent &e) {
        LOGI("Track Name: %s", e.name.c_str());
    }

    void operator()(const BankSelectMSBEvent &e) {
        (void)e;
    }

    void operator()(const BankSelectLSBEvent &e) {
        (void)e;
    }
};

void
midiFileInfo(const midi_file &m) {

    LOGI("tracks:");
    LOGI("Track Count: %d", m.header.trackCount);

    EventInfoVisitor eventInfoVisitor{};

    for (const auto &track : m.tracks) {

        for (const auto &e : track) {
            std::visit(eventInfoVisitor, e);
        }
    }

    auto times = midiFileTimes(m);

    LOGI("times:                  h:mm:sssss");
    if (times.lastNoteOnMicros != -1) {
        double lastNoteOnSec = times.lastNoteOnMicros / 1e6;
        double lastNoteOnMin = lastNoteOnSec / 60.0;
        double lastNoteOnHr = lastNoteOnMin / 60.0;
        LOGI("   last Note On (wall): %.0f:%02.0f:%05.2f", floor(lastNoteOnHr), fmod(lastNoteOnMin, 60.0), fmod(lastNoteOnSec, 60.0));
    } else {
        LOGI("   last Note On (wall): (none)");
    }
    if (times.lastNoteOffMicros != -1) {
        double lastNoteOffSec = times.lastNoteOffMicros / 1e6;
        double lastNoteOffMin = lastNoteOffSec / 60.0;
        double lastNoteOffHr = lastNoteOffMin / 60.0;
        LOGI("  last Note Off (wall): %.0f:%02.0f:%05.2f", floor(lastNoteOffHr), fmod(lastNoteOffMin, 60.0), fmod(lastNoteOffSec, 60.0));
    } else {
        LOGI("  last Note Off (wall): (none)");
    }
    if (times.lastEndOfTrackMicros != -1) {
        double lastEndOfTrackSec = times.lastEndOfTrackMicros / 1e6;
        double lastEndOfTrackMin = lastEndOfTrackSec / 60.0;
        double lastEndOfTrackHr = lastEndOfTrackMin / 60.0;
        LOGI("   End Of Track (wall): %.0f:%02.0f:%05.2f", floor(lastEndOfTrackHr), fmod(lastEndOfTrackMin, 60.0), fmod(lastEndOfTrackSec, 60.0));
    } else {
        LOGI("   End Of Track (wall): (none)");
    }

    LOGI(" last Note On (micros): %.17f", times.lastNoteOnMicros);
    LOGI("last Note Off (micros): %.17f", times.lastNoteOffMicros);
    LOGI(" End Of Track (micros): %.17f", times.lastEndOfTrackMicros);

    LOGI("  last Note On (ticks): %d", times.lastNoteOnTick);
    LOGI(" last Note Off (ticks): %d", times.lastNoteOffTick);
    LOGI("  End Of Track (ticks): %d", times.lastEndOfTrackTick);
}








