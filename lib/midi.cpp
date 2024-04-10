// Copyright (C) 2023 by Brenton Bostick
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

#include "tbt-parser/tbt-parser-util.h"

#include "common/assert.h"
#include "common/file.h"
#include "common/logging.h"

#include <cinttypes>
#include <cmath> // for round
#include <algorithm> // for remove


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

const double MICROS_PER_MINUTE = 60 * 1e6;


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
insertTempoMap_atTick72(
    const std::vector<tbt_track_effect_change> &changes,
    uint32_t tick,
    std::map<uint32_t, uint16_t> &tempoMap) {

    for (const auto &change : changes) {

        if (change.effect != TEMPO) {
            continue;
        }

        uint16_t newTempo = change.value;

        const auto &tempoMapIt = tempoMap.find(tick);
        if (tempoMapIt != tempoMap.end()) {
            if (tempoMapIt->second != newTempo) {
                LOGW("tick %d has conflicting tempo changes: %d, %d", tick, tempoMapIt->second, newTempo);
            }
        }

        tempoMap[tick] = newTempo;
    }
}


template <size_t STRINGS_PER_TRACK>
void
insertTempoMap_atTick(
    const std::array<uint8_t, STRINGS_PER_TRACK + STRINGS_PER_TRACK + 4> &vsqs,
    uint32_t tick,
    std::map<uint32_t, uint16_t> &tempoMap) {

    auto trackEffect = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 0];

    switch (trackEffect) {
        case 'T': {

            uint16_t newTempo = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

            const auto &tempoMapIt = tempoMap.find(tick);
            if (tempoMapIt != tempoMap.end()) {
                if (tempoMapIt->second != newTempo) {
                    LOGW("tick %d has conflicting tempo changes: %d, %d", tick, tempoMapIt->second, newTempo);
                }
            }

            tempoMap[tick] = newTempo;

            break;
        }
        case 't': {

            uint16_t newTempo = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3] + 250;

            const auto &tempoMapIt = tempoMap.find(tick);
            if (tempoMapIt != tempoMap.end()) {
                if (tempoMapIt->second != newTempo) {
                    LOGW("tick %d has conflicting tempo changes: %d, %d", tick, tempoMapIt->second, newTempo);
                }
            }

            tempoMap[tick] = newTempo;

            break;
        }
        default:
            break;
    }
}


template <uint8_t VERSION, typename tbt_file_t, size_t STRINGS_PER_TRACK>
void
computeTempoMap(
    const tbt_file_t &t,
    std::map<uint32_t, uint16_t> &tempoMap) {

    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        double tick_d = 0.0;
        uint32_t tick = 0;

        uint32_t trackSpaceCount;
        if constexpr (0x70 <= VERSION) {
            trackSpaceCount = t.metadata.tracks[track].spaceCount;
        } else if constexpr (VERSION == 0x6f) {
            trackSpaceCount = t.header.spaceCount;
        } else {
            trackSpaceCount = 4000;
        }

        for (uint32_t space = 0; space < trackSpaceCount; space++) {

            if constexpr (VERSION == 0x72) {

                const auto &trackEffectChangesMap = t.body.trackEffectChangesMapList[track];

                const auto &it = trackEffectChangesMap.find(space);
                if (it != trackEffectChangesMap.end()) {
                    
                    const auto &changes = it->second;

                    insertTempoMap_atTick72(changes, tick, tempoMap);
                }

            } else {

                const auto &notesMap = t.body.notesMapList[track];

                const auto &it = notesMap.find(space);
                if (it != notesMap.end()) {

                    const auto &vsqs = it->second;

                    insertTempoMap_atTick<STRINGS_PER_TRACK>(vsqs, tick, tempoMap);
                }
            }

            //
            // Compute tick
            //
            {
                //
                // The denominator of the Alternate Time Region for this space. For example, for triplets, this is 2.
                //
                double denominator = 1.0;

                //
                // The numerator of the Alternate Time Region for this space. For example, for triplets, this is 3.
                //
                double numerator = 1.0;

                if constexpr (0x70 <= VERSION) {

                    auto hasAlternateTimeRegions = ((t.header.featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK);

                    if (hasAlternateTimeRegions) {

                        const auto &alternateTimeRegionsStruct = t.body.alternateTimeRegionsMapList[track];

                        const auto &alternateTimeRegionsIt = alternateTimeRegionsStruct.find(space);
                        if (alternateTimeRegionsIt != alternateTimeRegionsStruct.end()) {

                            const auto &alternateTimeRegion = alternateTimeRegionsIt->second;

                            denominator = static_cast<double>(alternateTimeRegion[0]);

                            numerator = static_cast<double>(alternateTimeRegion[1]);
                        }
                    }
                }

                double tickInc = denominator * TICKS_PER_SPACE_D / numerator;

                tick_d += tickInc;

                tick = static_cast<uint32_t>(round(tick_d));
            }
        }
    }
}


//
// adapted from:
// https://www.rosettacode.org/wiki/Variable-length_quantity#C++
//
std::vector<uint8_t> toVLQ(uint32_t value) {

    uint8_t i;
    for (i = 9; i > 0; i--) {
        if (value & 127ULL << i * 7) {
            break;
        }
    }

    std::vector<uint8_t> out;
    out.reserve(i);
    
    for (uint8_t j = 0; j <= i; j++) {
        out.push_back(((value >> ((i - j) * 7)) & 127) | 128);
    }

    out[i] ^= 128;

    return out;
}


template <uint8_t VERSION, typename tbt_file_t, size_t STRINGS_PER_TRACK>
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
    // tick -> tempoBPM
    //
    std::map<uint32_t, uint16_t> tempoMap;

    computeTempoMap<VERSION, tbt_file_t, STRINGS_PER_TRACK>(t, tempoMap);

    //
    // compute channel map
    //
    std::map<uint8_t, uint8_t> channelMap;

    computeChannelMap<VERSION>(t, channelMap);


    out.header = midi_header{
        1, // format
        static_cast<uint16_t>(t.header.trackCount + 1), // track count, + 1 for tempo track
        TICKS_PER_BEAT // division
    };


    std::vector<midi_track_event> tmp;

    //
    // Track 0
    //
    {
        double tick_d = 0.0;
        uint32_t tick = 0;
        uint32_t lastEventTick = 0;

        tmp.clear();

        tmp.push_back(TimeSignatureEvent{
            0, // delta time
            4, // numerator
            2, // denominator (as 2^d)
            24, // ticks per metronome click
            8, // notated 32-notes in MIDI quarter notes
        });

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
            // Use floating point here for better accuracy
            //
            auto microsPerBeat = static_cast<uint32_t>(round(MICROS_PER_MINUTE / static_cast<double>(tempoBPM)));

            tmp.push_back(TempoChangeEvent{
                0, // delta time
                microsPerBeat
            });

            lastEventTick = tick;
        }

        for (uint32_t space = 0; space < barsSpaceCount; space++) {

            //
            // Emit tempo changes
            //
            const auto &tempoMapIt = tempoMap.find(tick);
            if (tempoMapIt != tempoMap.end()) {

                uint16_t tempoBPM = tempoMapIt->second;

                //
                // Use floating point here for better accuracy
                //
                auto microsPerBeat = static_cast<uint32_t>(round(MICROS_PER_MINUTE / static_cast<double>(tempoBPM)));

                uint32_t diff = tick - lastEventTick;

                tmp.push_back(TempoChangeEvent{
                    diff, // delta time
                    microsPerBeat
                });

                lastEventTick = tick;
            }

            //     every bar: lyric for current bars ?
            //     every space: lyric for current tempo
            //     tmp;

            // tmp.insert(tmp.end(), { 0x00, 0xFF, 0x05, 0x11 }); // lyric
            // tmp.insert(tmp.end(), { 0x73, 0x70, 0x61, 0x63, 0x65, 0x20, 0x30, 0x20, 0x74, 0x65, 0x6d, 0x70, 0x6f, 0x20, 0x31, 0x32, 0x30, });
        
            tick_d = space * TICKS_PER_SPACE_D;

            tick = static_cast<uint32_t>(round(tick_d));
        }

        tmp.push_back(EndOfTrackEvent{
            0 // delta time
        });

        out.tracks.push_back(tmp);
    }

    //
    // the actual tracks
    //
    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        const auto &notesMap = t.body.notesMapList[track];

        uint8_t channel = channelMap[track];
        uint8_t volume = t.metadata.tracks[track].volume;
        bool dontLetRing = ((t.metadata.tracks[track].cleanGuitar & 0b10000000) == 0b10000000);
        uint8_t midiProgram = (t.metadata.tracks[track].cleanGuitar & 0b01111111);

        uint8_t pan;
        if constexpr (0x6b <= VERSION) {
            pan = t.metadata.tracks[track].pan;
        } else {
            pan = 0x40; // 64
        }
        
        uint8_t reverb;
        uint8_t chorus;
        if constexpr (0x6e <= VERSION) {
            
            reverb = t.metadata.tracks[track].reverb;
            chorus = t.metadata.tracks[track].chorus;
            
        } else {
            
            reverb = 0;
            chorus = 0;
        }

        uint8_t modulation;
        int16_t pitchBend;
        if constexpr (0x71 <= VERSION) {
            
            modulation = t.metadata.tracks[track].modulation;
            pitchBend = t.metadata.tracks[track].pitchBend; // -2400 to 2400
            
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

        double tick_d = 0.0;
        uint32_t tick = 0;
        uint32_t lastEventTick = 0;
        std::array<uint8_t, STRINGS_PER_TRACK> currentlyPlayingStrings{};

        tmp.clear();

        tmp.push_back(ProgramChangeEvent{
            0, // delta time
            channel,
            midiProgram
        });

        tmp.push_back(PanEvent{
            0, // delta time
            channel,
            pan
        });

        tmp.push_back(ReverbEvent{
            0, // delta time
            channel,
            reverb
        });

        tmp.push_back(ChorusEvent{
            0, // delta time
            channel,
            chorus
        });

        tmp.push_back(ModulationEvent{
            0, // delta time
            channel,
            modulation
        });

        //
        // RPN Parameter MSB 0, RPN Parameter LSB 0 = RPN Parameter 0
        // RPN Parameter 0 is standardized for pitch bend range
        //

        tmp.push_back(RPNParameterMSBEvent{
            0, // delta time
            channel,
            0
        });

        tmp.push_back(RPNParameterLSBEvent{
            0, // delta time
            channel,
            0
        });

        tmp.push_back(DataEntryMSBEvent{
            0, // delta time
            channel,
            24 // semi-tones
        });

        tmp.push_back(DataEntryLSBEvent{
            0, // delta time
            channel,
            0 // cents
        });

        tmp.push_back(PitchBendEvent{
            0, // delta time
            channel,
            pitchBend
        });

        uint32_t trackSpaceCount;
        if constexpr (0x70 <= VERSION) {
            trackSpaceCount = t.metadata.tracks[track].spaceCount;
        } else if constexpr (VERSION == 0x6f) {
            trackSpaceCount = t.header.spaceCount;
        } else {
            trackSpaceCount = 4000;
        }

        for (uint32_t space = 0; space < trackSpaceCount; space++) {
            
            const auto &notesMapIt = notesMap.find(space);

            //
            // Emit note offs
            //
            {
                if (notesMapIt != notesMap.end()) {

                    const auto &onVsqs = notesMapIt->second;

                    uint8_t stringCount = t.metadata.tracks[track].stringCount;

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
                        for (uint8_t string = 0; string < stringCount; string++) {

                            uint8_t event = onVsqs[string];

                            if (event != 0) {
                                anyEvents = true;
                                break;
                            }
                        }
                        
                        if (anyEvents) {
                            
                            offVsqs = currentlyPlayingStrings;

                            for (uint8_t string = 0; string < stringCount; string++) {

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

                        for (uint8_t string = 0; string < stringCount; string++) {

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
                    for (uint8_t string = 0; string < stringCount; string++) {

                        uint8_t off = offVsqs[string];

                        if (off == 0) {
                            continue;
                        }

                        ASSERT(off >= 0x80);

                        uint8_t stringNote = off - 0x80;

                        stringNote += static_cast<uint8_t>(t.metadata.tracks[track].tuning[string]);

                        if constexpr (0x6e <= VERSION) {
                            stringNote += static_cast<uint8_t>(t.metadata.tracks[track].transposeHalfSteps);
                        }
                        
                        uint8_t midiNote;
                        if constexpr (0x6b <= VERSION) {
                            midiNote = stringNote + STRING_MIDI_NOTE[string];
                        } else {
                            midiNote = stringNote + STRING_MIDI_NOTE_LE6A[string];
                        }

                        auto diff = tick - lastEventTick;

                        tmp.push_back(NoteOffEvent{
                            diff, // delta time
                            channel,
                            midiNote,
                            0
                        });

                        lastEventTick = tick;
                    }
                }
            }

            //
            // Emit track effects
            //
            {
                if constexpr (VERSION == 0x72) {

                    const auto &trackEffectChangesMap = t.body.trackEffectChangesMapList[track];

                    const auto &trackEffectChangesIt = trackEffectChangesMap.find(space);
                    if (trackEffectChangesIt != trackEffectChangesMap.end()) {

                        const auto &changes = trackEffectChangesIt->second;

                        for (const auto &change : changes) {

                            switch(change.effect) {
                            case INSTRUMENT: {

                                auto newInstrument = change.value;

                                dontLetRing = ((newInstrument & 0b10000000) == 0b10000000);
                                midiProgram = (newInstrument & 0b01111111);

                                auto diff = tick - lastEventTick;

                                tmp.push_back(ProgramChangeEvent{
                                    diff, // delta time
                                    channel,
                                    midiProgram
                                });

                                lastEventTick = tick;

                                break;
                            }
                            case VOLUME: {

                                auto newVolume = change.value;

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
                                
                                auto newPan = change.value;

                                auto diff = tick - lastEventTick;

                                tmp.push_back(PanEvent{
                                    diff, // delta time
                                    channel,
                                    static_cast<uint8_t>(newPan)
                                });

                                lastEventTick = tick;

                                break;
                            }
                            case CHORUS: {
                                
                                auto newChorus = change.value;

                                auto diff = tick - lastEventTick;

                                tmp.push_back(ChorusEvent{
                                    diff, // delta time
                                    channel,
                                    static_cast<uint8_t>(newChorus)
                                });

                                lastEventTick = tick;

                                break;
                            }
                            case REVERB: {
                                
                                auto newReverb = change.value;

                                auto diff = tick - lastEventTick;

                                tmp.push_back(ReverbEvent{
                                    diff, // delta time
                                    channel,
                                    static_cast<uint8_t>(newReverb)
                                });

                                lastEventTick = tick;

                                break;
                            }
                            case MODULATION: {
                                
                                auto newModulation = change.value;

                                auto diff = tick - lastEventTick;

                                tmp.push_back(ModulationEvent{
                                    diff, // delta time
                                    channel,
                                    static_cast<uint8_t>(newModulation)
                                });

                                lastEventTick = tick;

                                break;
                            }
                            case PITCH_BEND: {
                                
                                int16_t newPitchBend = static_cast<int16_t>(change.value); // -2400 to 2400
                                //
                                // 0b0000000000000000 to 0b0011111111111111 (0 to 16383)
                                //
                                // important that value of 0 goes to value of 0x2000 (8192)
                                //
                                newPitchBend = static_cast<int16_t>(round(((static_cast<double>(newPitchBend) + 2400.0) * 16383.0) / (2.0 * 2400.0)));

                                auto diff = tick - lastEventTick;

                                tmp.push_back(PitchBendEvent{
                                    diff, // delta time
                                    channel,
                                    newPitchBend
                                });

                                lastEventTick = tick;

                                break;
                            }
                            default:
                                ASSERT(false);
                                break;
                            }
                        }
                    }

                } else {

                    if (notesMapIt != notesMap.end()) {

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

                            auto diff = tick - lastEventTick;

                            tmp.push_back(ProgramChangeEvent{
                                diff, // delta time
                                channel,
                                midiProgram
                            });

                            lastEventTick = tick;

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

                            auto diff = tick - lastEventTick;

                            tmp.push_back(ChorusEvent{
                                diff, // delta time
                                channel,
                                newChorus
                            });

                            lastEventTick = tick;

                            break;
                        }
                        case 'P': { // Pan change

                            auto newPan = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            auto diff = tick - lastEventTick;

                            tmp.push_back(PanEvent{
                                diff, // delta time
                                channel,
                                newPan
                            });

                            lastEventTick = tick;

                            break;
                        }
                        case 'R': { // Reverb change

                            auto newReverb = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            auto diff = tick - lastEventTick;

                            tmp.push_back(ReverbEvent{
                                diff, // delta time
                                channel,
                                newReverb
                            });

                            lastEventTick = tick;

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
                if (notesMapIt != notesMap.end()) {

                    const auto &onVsqs = notesMapIt->second;

                    uint8_t stringCount = t.metadata.tracks[track].stringCount;
                    
                    for (uint8_t string = 0; string < stringCount; string++) {

                        uint8_t on = onVsqs[string];

                        if (on == 0 ||
                            on == MUTED ||
                            on == STOPPED) {
                            continue;
                        }

                        ASSERT(on >= 0x80);

                        uint8_t stringNote = on - 0x80;
                        
                        stringNote += static_cast<uint8_t>(t.metadata.tracks[track].tuning[string]);
                        
                        if constexpr (0x6e <= VERSION) {
                            stringNote += static_cast<uint8_t>(t.metadata.tracks[track].transposeHalfSteps);
                        }

                        uint8_t midiNote;
                        if constexpr (0x6b <= VERSION) {
                            midiNote = stringNote + STRING_MIDI_NOTE[string];
                        } else {
                            midiNote = stringNote + STRING_MIDI_NOTE_LE6A[string];
                        }

                        auto diff = tick - lastEventTick;

                        tmp.push_back(NoteOnEvent{
                            diff, // delta time
                            channel,
                            midiNote,
                            volume // velocity
                        });

                        lastEventTick = tick;
                    }
                }
            }

            //
            // Compute tick
            //
            {
                //
                // The denominator of the Alternate Time Region for this space. For example, for triplets, this is 2.
                //
                double denominator = 1.0;

                //
                // The numerator of the Alternate Time Region for this space. For example, for triplets, this is 3.
                //
                double numerator = 1.0;

                if constexpr (0x70 <= VERSION) {

                    auto hasAlternateTimeRegions = ((t.header.featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK);

                    if (hasAlternateTimeRegions) {

                        const auto &alternateTimeRegionsStruct = t.body.alternateTimeRegionsMapList[track];

                        const auto &alternateTimeRegionsIt = alternateTimeRegionsStruct.find(space);
                        if (alternateTimeRegionsIt != alternateTimeRegionsStruct.end()) {

                            const auto &alternateTimeRegion = alternateTimeRegionsIt->second;

                            denominator = static_cast<double>(alternateTimeRegion[0]);

                            numerator = static_cast<double>(alternateTimeRegion[1]);
                        }
                    }
                }

                double tickInc = denominator * TICKS_PER_SPACE_D / numerator;

                tick_d += tickInc;

                tick = static_cast<uint32_t>(round(tick_d));
            }

        } // for space

        //
        // Emit any final note offs
        //
        {
            auto offVsqs = currentlyPlayingStrings;

            for (uint8_t string = 0; string < t.metadata.tracks[track].stringCount; string++) {

                uint8_t off = offVsqs[string];

                if (off == 0) {
                    continue;
                }

                ASSERT(off >= 0x80);

                uint8_t stringNote = off - 0x80;

                stringNote += static_cast<uint8_t>(t.metadata.tracks[track].tuning[string]);

                if constexpr (0x6e <= VERSION) {
                    stringNote += static_cast<uint8_t>(t.metadata.tracks[track].transposeHalfSteps);
                }

                uint8_t midiNote;
                if constexpr (0x6b <= VERSION) {
                    midiNote = stringNote + STRING_MIDI_NOTE[string];
                } else {
                    midiNote = stringNote + STRING_MIDI_NOTE_LE6A[string];
                }

                auto diff = tick - lastEventTick;

                tmp.push_back(NoteOffEvent{
                    diff, // delta time
                    channel,
                    midiNote,
                    0
                });

                lastEventTick = tick;
            }
        }

        tmp.push_back(EndOfTrackEvent{
            0, // delta time
        });

        out.tracks.push_back(tmp);
    }

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
        
        return TconvertToMidi<0x72, tbt_file71, 8>(t71, out);
    }
    case 0x71: {
        
        auto t71 = std::get<tbt_file71>(t);
        
        return TconvertToMidi<0x71, tbt_file71, 8>(t71, out);
    }
    case 0x70: {
        
        auto t70 = std::get<tbt_file70>(t);
        
        return TconvertToMidi<0x70, tbt_file70, 8>(t70, out);
    }
    case 0x6f: {
        
        auto t6f = std::get<tbt_file6f>(t);
        
        return TconvertToMidi<0x6f, tbt_file6f, 8>(t6f, out);
    }
    case 0x6e: {
        
        auto t6e = std::get<tbt_file6e>(t);
        
        return TconvertToMidi<0x6e, tbt_file6e, 8>(t6e, out);
    }
    case 0x6b: {
        
        auto t6b = std::get<tbt_file6b>(t);
        
        return TconvertToMidi<0x6b, tbt_file6b, 8>(t6b, out);
    }
    case 0x6a: {
        
        auto t6a = std::get<tbt_file6a>(t);
        
        return TconvertToMidi<0x6a, tbt_file6a, 6>(t6a, out);
    }
    case 0x69: {
        
        auto t68 = std::get<tbt_file68>(t);
        
        return TconvertToMidi<0x69, tbt_file68, 6>(t68, out);
    }
    case 0x68: {
        
        auto t68 = std::get<tbt_file68>(t);
        
        return TconvertToMidi<0x68, tbt_file68, 6>(t68, out);
    }
    case 0x67: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TconvertToMidi<0x67, tbt_file65, 6>(t65, out);
    }
    case 0x66: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TconvertToMidi<0x66, tbt_file65, 6>(t65, out);
    }
    case 0x65: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TconvertToMidi<0x65, tbt_file65, 6>(t65, out);
    }
    default:
        ASSERT(false);
        return ERR;
    }
}


struct EventVisitor {
    
    std::vector<uint8_t> &tmp;
    
    void operator()(const TimeSignatureEvent &e);

    void operator()(const TempoChangeEvent &e);

    void operator()(const EndOfTrackEvent &e);

    void operator()(const ProgramChangeEvent &e);

    void operator()(const PanEvent &e);

    void operator()(const ReverbEvent &e);

    void operator()(const ChorusEvent &e);

    void operator()(const ModulationEvent &e);

    void operator()(const RPNParameterMSBEvent &e);

    void operator()(const RPNParameterLSBEvent &e);

    void operator()(const DataEntryMSBEvent &e);

    void operator()(const DataEntryLSBEvent &e);

    void operator()(const PitchBendEvent &e);

    void operator()(const NoteOffEvent &e);

    void operator()(const NoteOnEvent &e);

    void operator()(const NullEvent &e);
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
        auto lengthBytes = toDigitsBE(static_cast<uint32_t>(6));
        auto formatBytes = toDigitsBE(static_cast<uint16_t>(m.header.format));
        auto trackCountBytes = toDigitsBE(static_cast<uint16_t>(m.header.trackCount));
        auto divisionBytes = toDigitsBE(static_cast<uint16_t>(m.header.division));

        out.insert(out.end(), { 'M', 'T', 'h', 'd' }); // type
        out.insert(out.end(), lengthBytes.cbegin(), lengthBytes.cend()); // length
        out.insert(out.end(), formatBytes.cbegin(), formatBytes.cend()); // format
        out.insert(out.end(), trackCountBytes.cbegin(), trackCountBytes.cend()); // track count
        out.insert(out.end(), divisionBytes.cbegin(), divisionBytes.cend()); // division
    }

    //
    // tracks
    //
    for (const auto &track : m.tracks) {

        std::vector<uint8_t> tmp;

        EventVisitor eventVisitor{tmp};

        for (const auto &event : track) {
            std::visit(eventVisitor, event);
        }

        auto lengthBytes = toDigitsBE(static_cast<uint32_t>(tmp.size()));

        out.insert(out.end(), { 'M', 'T', 'r', 'k' }); // type
        out.insert(out.end(), lengthBytes.cbegin(), lengthBytes.cend()); // length
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

    
void EventVisitor::operator()(const TimeSignatureEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

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

void EventVisitor::operator()(const TempoChangeEvent &e) {

    auto vlq = toVLQ(e.deltaTime);
    auto microsPerBeatBytes = toDigitsBE(e.microsPerBeat);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        0xff, // meta event
        0x51, // tempo change
        //
        // FluidSynth hard-codes length of 3, so just do the same
        //
        0x03 // microsPerBeatBytes size VLQ
    });

    tmp.insert(tmp.end(), microsPerBeatBytes.cbegin() + 1, microsPerBeatBytes.cend());
}

void EventVisitor::operator()(const EndOfTrackEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        0xff, // meta event
        0x2f, // end of track
        0x00
    });
}

void EventVisitor::operator()(const ProgramChangeEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        static_cast<uint8_t>(0xc0 | e.channel), // program change
        e.midiProgram,
    });
}

void EventVisitor::operator()(const PanEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        static_cast<uint8_t>(0xb0 | e.channel), // control event
        0x0a, // pan
        e.pan,
    });
}

void EventVisitor::operator()(const ReverbEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        static_cast<uint8_t>(0xb0 | e.channel), // control event
        0x5b, // reverb
        e.reverb,
    });
}

void EventVisitor::operator()(const ChorusEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        static_cast<uint8_t>(0xb0 | e.channel), // control event
        0x5d, // chorus
        e.chorus,
    });
}

void EventVisitor::operator()(const ModulationEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        static_cast<uint8_t>(0xb0 | e.channel), // control event
        0x01, // modulation
        e.modulation,
    });
}

void EventVisitor::operator()(const RPNParameterMSBEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        static_cast<uint8_t>(0xb0 | e.channel), // control event
        0x65, // RPN Parameter MSB
        e.rpnParameterMSB
    });
}

void EventVisitor::operator()(const RPNParameterLSBEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        static_cast<uint8_t>(0xb0 | e.channel), // control event
        0x64, // RPN Parameter LSB
        e.rpnParameterLSB
    });
}

void EventVisitor::operator()(const DataEntryMSBEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        static_cast<uint8_t>(0xb0 | e.channel), // control event
        0x06, // Data Entry MSB
        e.dataEntryMSB
    });
}

void EventVisitor::operator()(const DataEntryLSBEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        static_cast<uint8_t>(0xb0 | e.channel), // control event
        0x26, // Data Entry LSB
        e.dataEntryLSB
    });
}

void EventVisitor::operator()(const PitchBendEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    uint8_t pitchBendLSB = (e.pitchBend & 0b01111111);
    uint8_t pitchBendMSB = ((e.pitchBend >> 7) & 0b01111111);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        static_cast<uint8_t>(0xe0 | e.channel), // pitch bend
        pitchBendLSB,
        pitchBendMSB
    });
}

void EventVisitor::operator()(const NoteOffEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        static_cast<uint8_t>(0x80 | e.channel), // note off
        e.midiNote,
        e.velocity
    });
}

void EventVisitor::operator()(const NoteOnEvent &e) {

    auto vlq = toVLQ(e.deltaTime);

    tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend()); // delta time

    tmp.insert(tmp.end(), {
        static_cast<uint8_t>(0x90 | e.channel), // note on
        e.midiNote,
        e.velocity
    });
}

void EventVisitor::operator()(const NullEvent &e) {
    (void)e;
}












