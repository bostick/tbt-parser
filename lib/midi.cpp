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

#define _CRT_SECURE_NO_DEPRECATE // disable warnings about fopen being insecure on MSVC

#include "tbt-parser/midi.h"

#include "tbt-parser/tbt-parser-util.h"

#include "common/assert.h"
#include "common/logging.h"

#include <cinttypes>
#include <cmath> // for round
#include <algorithm> // for remove


#define TAG "midi"


const std::array<uint8_t, 8> STRING_MIDI_NOTE =      { 0x28, 0x2d, 0x32, 0x37, 0x3b, 0x40, 0x00, 0x00 };
const std::array<uint8_t, 6> STRING_MIDI_NOTE_LE6A = { 0x40, 0x3b, 0x37, 0x32, 0x2d, 0x28 };

const uint8_t MUTED = 0x11; // 17
const uint8_t STOPPED = 0x12; // 18


//
// resolve all of the Automatically Assign -1 values to actual channels
//
template <uint8_t VERSION, typename tbt_file_t>
void
computeChannelMap(
    const tbt_file_t &t,
    std::unordered_map<uint8_t, uint8_t> &channelMap) {

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
    std::unordered_map<uint32_t, uint16_t> &tempoMap) {

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
    std::unordered_map<uint32_t, uint16_t> &tempoMap) {

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
    std::unordered_map<uint32_t, uint16_t> &tempoMap) {

    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        uint32_t tick = 0;
        double actualSpace = 0.0;

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
                uint8_t denominator = 1;

                //
                // The numerator of the Alternate Time Region for this space. For example, for triplets, this is 3.
                //
                uint8_t numerator = 1;

                if constexpr (0x70 <= VERSION) {

                    auto hasAlternateTimeRegions = ((t.header.featureBitfield & 0b00010000) == 0b00010000);

                    if (hasAlternateTimeRegions) {

                        const auto &alternateTimeRegionsStruct = t.body.alternateTimeRegionsMapList[track];

                        const auto &alternateTimeRegionsIt = alternateTimeRegionsStruct.find(space);
                        if (alternateTimeRegionsIt != alternateTimeRegionsStruct.end()) {

                            const auto &alternateTimeRegion = alternateTimeRegionsIt->second;

                            denominator = alternateTimeRegion[0];

                            numerator = alternateTimeRegion[1];
                        }
                    }
                }

                double actualSpaceInc = (static_cast<double>(denominator) / static_cast<double>(numerator));

                actualSpace += actualSpaceInc;

                tick = static_cast<uint32_t>(round(actualSpace * static_cast<double>(TICKS_PER_SPACE)));
            }
        }
    }
}


template <uint8_t VERSION, typename tbt_file_t, size_t STRINGS_PER_TRACK>
Status
exportMidiBytes(
    const tbt_file_t &t,
    std::vector<uint8_t> &out) {

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
    std::unordered_map<uint32_t, uint16_t> tempoMap;

    computeTempoMap<VERSION, tbt_file_t, STRINGS_PER_TRACK>(t, tempoMap);

    //
    // compute channel map
    //
    std::unordered_map<uint8_t, uint8_t> channelMap;

    computeChannelMap<VERSION>(t, channelMap);

    //
    // header
    //
    out.insert(out.end(), { 'M', 'T', 'h', 'd' }); // type
    out.insert(out.end(), { 0x00, 0x00, 0x00, 0x06 }); // length
    out.insert(out.end(), { 0x00, 0x01 }); // format
    out.insert(out.end(), { 0x00, static_cast<uint8_t>(t.header.trackCount + 1) }); // track count
    out.insert(out.end(), { 0x00, TICKS_PER_BEAT }); // division

    std::vector<uint8_t> tmp;

    //
    // Track 0
    //
    {
        uint32_t tick = 0;
        uint32_t lastEventTick = 0;

        out.insert(out.end(), { 'M', 'T', 'r', 'k' }); // type

        tmp.clear();

        tmp.insert(tmp.end(), { 0x00, 0xff, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08 }); // time signature

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
            auto microsPerBeat = static_cast<uint32_t>(round(60000000.0 / tempoBPM));
            auto microsPerBeatBytes = toDigitsBE(microsPerBeat);

            tmp.insert(tmp.end(), { 0x00, 0xff, 0x51 }); // tempo change, 0x07a120 == 500000 MMPB == 120 BPM

            //
            // FluidSynth hard-codes length of 3, so just do the same
            //
            tmp.insert(tmp.end(), { 3 }); // microsPerBeatBytes size VLQ
            tmp.insert(tmp.end(), microsPerBeatBytes.cbegin() + 1, microsPerBeatBytes.cend());

            lastEventTick = tick;
        }

        for (uint32_t space = 0; space < barsSpaceCount + 1; space++) { // + 1 for any still playing at end

            //
            // Emit tempo changes
            //
            const auto &it = tempoMap.find(tick);
            if (it != tempoMap.end()) {

                uint16_t tempoBPM = it->second;

                //
                // Use floating point here for better accuracy
                //
                auto microsPerBeat = static_cast<uint32_t>(round(60000000.0 / tempoBPM));
                auto microsPerBeatBytes = toDigitsBE(microsPerBeat);

                uint32_t diff = tick - lastEventTick;

                auto vlq = toVLQ(diff);

                tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                tmp.insert(tmp.end(), { 0xff, 0x51 }); // tempo change, 0x07a120 == 500000 MMPB == 120 BPM

                //
                // FluidSynth hard-codes length of 3, so just do the same
                //
                tmp.insert(tmp.end(), { 3 }); // microsPerBeatBytes size VLQ
                tmp.insert(tmp.end(), microsPerBeatBytes.cbegin() + 1, microsPerBeatBytes.cend());

                lastEventTick = tick;
            }

            //     every bar: lyric for current bars ?
            //     every space: lyric for current tempo
            //     tmp;

            // tmp.insert(tmp.end(), { 0x00, 0xFF, 0x05, 0x11 }); // lyric
            // tmp.insert(tmp.end(), { 0x73, 0x70, 0x61, 0x63, 0x65, 0x20, 0x30, 0x20, 0x74, 0x65, 0x6d, 0x70, 0x6f, 0x20, 0x31, 0x32, 0x30, });
        
            tick = space * TICKS_PER_SPACE;
        }

        tmp.insert(tmp.end(), { 0x00, 0xff, 0x2f, 0x00 }); // end of track

        //
        // append tmp to out
        //
        {
            auto tmpSizeBytes = toDigitsBE(static_cast<uint32_t>(tmp.size()));
            out.insert(out.end(), tmpSizeBytes.cbegin(), tmpSizeBytes.cend()); // length
            out.insert(out.end(), tmp.cbegin(), tmp.cend());
        }
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
        uint8_t pitchBendLSB = (pitchBend & 0b01111111);
        uint8_t pitchBendMSB = ((pitchBend >> 7) & 0b01111111);

        uint32_t tick = 0;
        double actualSpace = 0.0;
        uint32_t lastEventTick = 0;
        std::array<uint8_t, STRINGS_PER_TRACK> currentlyPlayingStrings{};

        out.insert(out.end(), { 'M', 'T', 'r', 'k' }); // type

        tmp.clear();

        tmp.insert(tmp.end(), {
            0x00,
            static_cast<uint8_t>(0xc0 | channel), // program change
            midiProgram,

            0x00,
            static_cast<uint8_t>(0xb0 | channel), // control change
            0x0a, // pan
            pan,

            0x00,
            static_cast<uint8_t>(0xb0 | channel), // control change
            0x5b, // reverb
            reverb,
            
            0x00,
            static_cast<uint8_t>(0xb0 | channel), // control change
            0x5d, // chorus
            chorus,
            
            0x00,
            static_cast<uint8_t>(0xb0 | channel), // control change
            0x01, // modulation
            modulation,
            
            0x00,
            static_cast<uint8_t>(0xb0 | channel), // control change
            0x65, // RPN Parameter MSB
            0x00,
            
            0x00,
            static_cast<uint8_t>(0xb0 | channel), // control change
            0x64, // RPN Parameter LSB
            0x00,
            
            0x00,
            static_cast<uint8_t>(0xb0 | channel), // control change
            0x06, // Data Entry MSB
            0x18, // 24 semi-tones
            
            0x00,
            static_cast<uint8_t>(0xb0 | channel), // control change
            0x26, // Data Entry LSB
            0x00, // 0 cents
            
            0x00,
            static_cast<uint8_t>(0xe0 | channel), // pitch bend
            pitchBendLSB,
            pitchBendMSB
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

                        auto vlq = toVLQ(diff);

                        tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                        tmp.insert(tmp.end(), {
                            static_cast<uint8_t>(0x80 | channel), // note off
                            midiNote,
                            0x00 // velocity
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

                                auto vlq = toVLQ(diff);

                                tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                                tmp.insert(tmp.end(), {
                                    static_cast<uint8_t>(0xc0 | channel), // program change
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

                                auto vlq = toVLQ(diff);

                                tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                                tmp.insert(tmp.end(), {
                                    static_cast<uint8_t>(0xb0 | channel), // control change
                                    0x0a, // pan
                                    static_cast<uint8_t>(newPan)
                                });

                                lastEventTick = tick;

                                break;
                            }
                            case CHORUS: {
                                
                                auto newChorus = change.value;

                                auto diff = tick - lastEventTick;

                                auto vlq = toVLQ(diff);

                                tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                                tmp.insert(tmp.end(), {
                                    static_cast<uint8_t>(0xb0 | channel), // control change
                                    0x5d, // chorus
                                    static_cast<uint8_t>(newChorus)
                                });

                                lastEventTick = tick;

                                break;
                            }
                            case REVERB: {
                                
                                auto newReverb = change.value;

                                auto diff = tick - lastEventTick;

                                auto vlq = toVLQ(diff);

                                tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                                tmp.insert(tmp.end(), {
                                    static_cast<uint8_t>(0xb0 | channel), // control change
                                    0x5b, // reverb
                                    static_cast<uint8_t>(newReverb)
                                });

                                lastEventTick = tick;

                                break;
                            }
                            case MODULATION: {
                                
                                auto newModulation = change.value;

                                auto diff = tick - lastEventTick;

                                auto vlq = toVLQ(diff);

                                tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                                tmp.insert(tmp.end(), {
                                    static_cast<uint8_t>(0xb0 | channel), // control change
                                    0x01, // modulation
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
                                uint8_t newPitchBendLSB = (newPitchBend & 0b01111111);
                                uint8_t newPitchBendMSB = ((newPitchBend >> 7) & 0b01111111);

                                auto diff = tick - lastEventTick;

                                auto vlq = toVLQ(diff);

                                tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                                tmp.insert(tmp.end(), {
                                    static_cast<uint8_t>(0xe0 | channel), // pitch bend
                                    newPitchBendLSB,
                                    newPitchBendMSB
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

                            auto vlq = toVLQ(diff);

                            tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                            tmp.insert(tmp.end(), {
                                static_cast<uint8_t>(0xc0 | channel), // program change
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

                            auto vlq = toVLQ(diff);

                            tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                            tmp.insert(tmp.end(), {
                                static_cast<uint8_t>(0xb0 | channel), // control change
                                0x5d, // chorus
                                newChorus
                            });

                            lastEventTick = tick;

                            break;
                        }
                        case 'P': { // Pan change

                            auto newPan = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            auto diff = tick - lastEventTick;

                            auto vlq = toVLQ(diff);

                            tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                            tmp.insert(tmp.end(), {
                                static_cast<uint8_t>(0xb0 | channel), // control change
                                0x0a, // pan
                                newPan
                            });

                            lastEventTick = tick;

                            break;
                        }
                        case 'R': { // Reverb change

                            auto newReverb = vsqs[STRINGS_PER_TRACK + STRINGS_PER_TRACK + 3];

                            auto diff = tick - lastEventTick;

                            auto vlq = toVLQ(diff);

                            tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                            tmp.insert(tmp.end(), {
                                static_cast<uint8_t>(0xb0 | channel), // control change
                                0x5b, // reverb
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

                        auto vlq = toVLQ(diff);

                        tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                        tmp.insert(tmp.end(), {
                            static_cast<uint8_t>(0x90 | channel), // note on
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
                uint8_t denominator = 1;

                //
                // The numerator of the Alternate Time Region for this space. For example, for triplets, this is 3.
                //
                uint8_t numerator = 1;

                if constexpr (0x70 <= VERSION) {

                    auto hasAlternateTimeRegions = ((t.header.featureBitfield & 0b00010000) == 0b00010000);

                    if (hasAlternateTimeRegions) {

                        const auto &alternateTimeRegionsStruct = t.body.alternateTimeRegionsMapList[track];

                        const auto &alternateTimeRegionsIt = alternateTimeRegionsStruct.find(space);
                        if (alternateTimeRegionsIt != alternateTimeRegionsStruct.end()) {

                            const auto &alternateTimeRegion = alternateTimeRegionsIt->second;

                            denominator = alternateTimeRegion[0];

                            numerator = alternateTimeRegion[1];
                        }
                    }
                }

                double actualSpaceInc = (static_cast<double>(denominator) / static_cast<double>(numerator));

                actualSpace += actualSpaceInc;

                tick = static_cast<uint32_t>(round(actualSpace * static_cast<double>(TICKS_PER_SPACE)));
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

                auto vlq = toVLQ(diff);

                tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                tmp.insert(tmp.end(), {
                    static_cast<uint8_t>(0x80 | channel), // note off
                    midiNote,
                    0x00 // velocity
                });

                lastEventTick = tick;
            }
        }

        tmp.insert(tmp.end(), { 0x00, 0xff, 0x2f, 0x00 }); // end of track

        //
        // append tmp to out
        //
        {
            auto tmpSizeBytes = toDigitsBE(static_cast<uint32_t>(tmp.size()));
            out.insert(out.end(), tmpSizeBytes.cbegin(), tmpSizeBytes.cend()); // length
            out.insert(out.end(), tmp.cbegin(), tmp.cend());
        }
    }

    return OK;
}


Status
exportMidiFile(
    const tbt_file t,
    const char *path) {

    std::vector<uint8_t> midiBytes;

    Status ret;

    auto versionNumber = reinterpret_cast<const uint8_t *>(t)[3];

    switch (versionNumber) {
    case 0x72: {

        auto t71 = *reinterpret_cast<const tbt_file71 *>(t);
        
        ret = exportMidiBytes<0x72, tbt_file71, 8>(t71, midiBytes);
        
        break;
    }
    case 0x71: {
        
        auto t71 = *reinterpret_cast<const tbt_file71 *>(t);
        
        ret = exportMidiBytes<0x71, tbt_file71, 8>(t71, midiBytes);
        
        break;
    }
    case 0x70: {
        
        auto t70 = *reinterpret_cast<const tbt_file70 *>(t);
        
        ret = exportMidiBytes<0x70, tbt_file70, 8>(t70, midiBytes);
        
        break;
    }
    case 0x6f: {
        
        auto t6f = *reinterpret_cast<const tbt_file6f *>(t);
        
        ret = exportMidiBytes<0x6f, tbt_file6f, 8>(t6f, midiBytes);
        
        break;
    }
    case 0x6e: {
        
        auto t6e = *reinterpret_cast<const tbt_file6e *>(t);
        
        ret = exportMidiBytes<0x6e, tbt_file6e, 8>(t6e, midiBytes);
        
        break;
    }
    case 0x6b: {
        
        auto t6b = *reinterpret_cast<const tbt_file6b *>(t);
        
        ret = exportMidiBytes<0x6b, tbt_file6b, 8>(t6b, midiBytes);
        
        break;
    }
    case 0x6a: {
        
        auto t6a = *reinterpret_cast<const tbt_file6a *>(t);
        
        ret = exportMidiBytes<0x6a, tbt_file6a, 6>(t6a, midiBytes);
        
        break;
    }
    case 0x69: {
        
        auto t68 = *reinterpret_cast<const tbt_file68 *>(t);
        
        ret = exportMidiBytes<0x69, tbt_file68, 6>(t68, midiBytes);
        
        break;
    }
    case 0x68: {
        
        auto t68 = *reinterpret_cast<const tbt_file68 *>(t);
        
        ret = exportMidiBytes<0x68, tbt_file68, 6>(t68, midiBytes);
        
        break;
    }
    case 0x67: {
        
        auto t65 = *reinterpret_cast<const tbt_file65 *>(t);
        
        ret = exportMidiBytes<0x67, tbt_file65, 6>(t65, midiBytes);
        
        break;
    }
    case 0x66: {
        
        auto t65 = *reinterpret_cast<const tbt_file65 *>(t);
        
        ret = exportMidiBytes<0x66, tbt_file65, 6>(t65, midiBytes);
        
        break;
    }
    case 0x65: {
        
        auto t65 = *reinterpret_cast<const tbt_file65 *>(t);
        
        ret = exportMidiBytes<0x65, tbt_file65, 6>(t65, midiBytes);
        
        break;
    }
    default:
        ASSERT(false);
        return ERR;
    }

    if (ret != OK) {
        return ret;
    }

    FILE *file = fopen(path, "wb");

    if (!file) {

        LOGE("cannot open %s\n", path);

        return ERR;
    }

    auto r = fwrite(midiBytes.data(), sizeof(uint8_t), midiBytes.size(), file);

    if (r != midiBytes.size()) {

        LOGE("fwrite failed");

        return ERR;
    }

    fclose(file);

    return OK;
}












