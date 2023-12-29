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


const std::array<uint8_t, 8> STRING_MIDI_NOTE = { 0x28, 0x2d, 0x32, 0x37, 0x3b, 0x40, 0x00, 0x00 };

const uint8_t MUTED = 0x11; // 17
const uint8_t STOPPED = 0x12; // 18


//
// resolve all of the Automatically Assign -1 values to actual channels
//
void
computeChannelMap(
    const tbt_file &t,
    std::unordered_map<uint8_t, uint8_t> &channelMap) {

    channelMap.clear();

    //
    // Channel 9 is for drums and is not generally available
    //
    std::vector<uint8_t> availableChannels{ 0, 1, 2, 3, 4, 5, 6, 7, 8, /*9,*/ 10, 11, 12, 13, 14, 15 };

    //
    // first just treat any assigned channels as unavailable
    //
    if (0x6a <= t.header.versionNumber) {
        
        for (uint8_t track = 0; track < t.header.trackCount; track++) {

            if (t.metadata.midiChannelBlock[track] == -1) {
                continue;
            }

            //
            // https://en.wikipedia.org/wiki/Erase%E2%80%93remove_idiom
            //
            availableChannels.erase(
                    std::remove(
                            availableChannels.begin(),
                            availableChannels.end(),
                            t.metadata.midiChannelBlock[track]),
                    availableChannels.end()
            );

            channelMap[track] = static_cast<uint8_t>(t.metadata.midiChannelBlock[track]);
        }
    }

    //
    // availableChannels now holds generally available channels
    //

    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        if (0x6a <= t.header.versionNumber) {
            if (t.metadata.midiChannelBlock[track] != -1) {
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
insertTempoMap_atTickGE71(
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


void
insertTempoMap_atTick(
    const std::array<uint8_t, 20> &vsqs,
    uint32_t tick,
    std::unordered_map<uint32_t, uint16_t> &tempoMap) {

    auto trackEffect = vsqs[16];

    switch (trackEffect) {
        case 'T': {

            uint16_t newTempo = vsqs[19];

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

            uint16_t newTempo = vsqs[19] + 250;

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


void
computeTempoMap(
    const tbt_file &t,
    std::unordered_map<uint32_t, uint16_t> &tempoMap) {

    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        uint32_t tick = 0;
        double floatingTick = 0.0;

        uint32_t trackSpaceCount;
        if (0x70 <= t.header.versionNumber) {
            trackSpaceCount = t.metadata.spaceCountBlock[track];
        } else if (t.header.versionNumber == 0x6f) {
            trackSpaceCount = t.header.spaceCount6f;
        } else {
            trackSpaceCount = 4000;
        }

        for (uint32_t space = 0; space < trackSpaceCount; space++) {

            if (0x71 <= t.header.versionNumber) {

                const auto &trackEffectChangesMap = t.body.trackEffectChangesMapList[track];

                const auto &it = trackEffectChangesMap.find(space);

                if (it != trackEffectChangesMap.end()) {
                    
                    const auto &changes = it->second;

                    insertTempoMap_atTickGE71(changes, tick, tempoMap);
                }

            } else {

                const auto &notesMap = t.body.notesMapList[track];

                const auto &it = notesMap.find(space);
                
                if (it != notesMap.end()) {
                    
                    const auto &vsqs = it->second;

                    insertTempoMap_atTick(vsqs, tick, tempoMap);
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

                double actualTicksInSpace = (static_cast<double>(denominator) * static_cast<double>(TICKS_PER_SPACE) / static_cast<double>(numerator));

                floatingTick += actualTicksInSpace;

                tick = static_cast<uint32_t>(round(floatingTick));
            }
        }
    }
}


Status
exportMidiBytes(
    const tbt_file &t,
    std::vector<uint8_t> &out) {

    uint32_t barsSpaceCount;
    if (0x70 <= t.header.versionNumber) {
        barsSpaceCount = t.body.barsSpaceCountGE70;
    } else if (t.header.versionNumber == 0x6f) {
        barsSpaceCount = t.header.spaceCount6f;
    } else {
        barsSpaceCount = 4000;
    }

    //
    // compute tempo map
    //
    // tick -> tempoBPM
    //
    std::unordered_map<uint32_t, uint16_t> tempoMap;

    computeTempoMap(t, tempoMap);

    //
    // compute channel map
    //
    std::unordered_map<uint8_t, uint8_t> channelMap;

    computeChannelMap(t, channelMap);

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
        uint32_t lastEventTick;

        out.insert(out.end(), { 'M', 'T', 'r', 'k' }); // type

        tmp.clear();

        tmp.insert(tmp.end(), { 0x00, 0xff, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08 }); // time signature

        //
        // Emit tempo
        //
        {
            uint16_t tempoBPM = t.header.tempo2;

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

            lastEventTick = 0;
        }

        for (uint32_t tick = 0; tick < TICKS_PER_SPACE * (barsSpaceCount + 1); tick++) { // + 1 for any still playing at end

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
        }

        tmp.insert(tmp.end(), { 0x00, 0xff, 0x2f, 0x00 }); // end of track

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
        uint8_t volume = t.metadata.volumeBlock[track];
        bool dontLetRing = ((t.metadata.cleanGuitarBlock[track] & 0b10000000) == 0b10000000);
        uint8_t midiProgram = (t.metadata.cleanGuitarBlock[track] & 0b01111111);

        uint8_t pan;
        if (0x6b <= t.header.versionNumber) {
            pan = t.metadata.panBlock[track];
        } else {
            pan = 0x40; // 64
        }
        
        uint8_t reverb;
        uint8_t chorus;
        if (0x6c <= t.header.versionNumber) {
            
            reverb = t.metadata.reverbBlock[track];
            chorus = t.metadata.chorusBlock[track];
            
        } else {
            
            reverb = 0;
            chorus = 0;
        }

        uint8_t modulation;
        int16_t pitchBend;
        if (0x71 <= t.header.versionNumber) {
            
            modulation = t.metadata.modulationBlock[track];
            pitchBend = t.metadata.pitchBendBlock[track]; // -2400 to 2400
            
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
        double floatingTick = 0.0;
        uint32_t lastEventTick = 0;
        std::array<uint8_t, 8> currentlyPlayingStrings{};

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
        if (0x70 <= t.header.versionNumber) {
            trackSpaceCount = t.metadata.spaceCountBlock[track];
        } else if (t.header.versionNumber == 0x6f) {
            trackSpaceCount = t.header.spaceCount6f;
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

                    uint8_t stringCount = t.metadata.stringCountBlock[track];

                    std::array<uint8_t, 8> offVsqs{};

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

                        if (0x6b <= t.header.versionNumber) {
                            stringNote += static_cast<uint8_t>(t.metadata.tuningBlock[track][string]);
                        } else {
                            stringNote += static_cast<uint8_t>(t.metadata.tuningBlockLE6a[track][string]);
                        }

                        if (0x6d <= t.header.versionNumber) {
                            stringNote += static_cast<uint8_t>(t.metadata.transposeHalfStepsBlock[track]);
                        }
                        
                        uint8_t midiNote = stringNote + STRING_MIDI_NOTE[string];

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
                if (0x71 <= t.header.versionNumber) {

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

                        auto trackEffect = vsqs[16];

                        switch (trackEffect) {
                        case 0x00:
                            //
                            // skip
                            //
                            break;
                        case 'I': { // Instrument change

                            auto newInstrument = vsqs[19];

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

                            auto newVolume = vsqs[19];

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

                            auto newChorus = vsqs[19];

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

                            auto newPan = vsqs[19];

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

                            auto newReverb = vsqs[19];

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

                    uint8_t stringCount = t.metadata.stringCountBlock[track];
                    
                    for (uint8_t string = 0; string < stringCount; string++) {

                        uint8_t on = onVsqs[string];

                        if (on == 0 ||
                                on == MUTED ||
                                on == STOPPED) {
                            continue;
                        }

                        ASSERT(on >= 0x80);

                        uint8_t stringNote = on - 0x80;
                        
                        if (0x6b <= t.header.versionNumber) {
                            stringNote += static_cast<uint8_t>(t.metadata.tuningBlock[track][string]);
                        } else {
                            stringNote += static_cast<uint8_t>(t.metadata.tuningBlockLE6a[track][string]);
                        }
                        
                        if (0x6d <= t.header.versionNumber) {
                            stringNote += static_cast<uint8_t>(t.metadata.transposeHalfStepsBlock[track]);
                        }

                        uint8_t midiNote = stringNote + STRING_MIDI_NOTE[string];

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

                double actualTicksInSpace = (static_cast<double>(denominator) * static_cast<double>(TICKS_PER_SPACE) / static_cast<double>(numerator));

                floatingTick += actualTicksInSpace;

                tick = static_cast<uint32_t>(round(floatingTick));
            }

        } // for space

        //
        // Emit any final note offs
        //
        {
            std::array<uint8_t, 8> offVsqs = currentlyPlayingStrings;

            for (uint8_t string = 0; string < t.metadata.stringCountBlock[track]; string++) {

                uint8_t off = offVsqs[string];

                if (off == 0) {
                    continue;
                }

                ASSERT(off >= 0x80);

                uint8_t stringNote = off - 0x80;

                if (0x6b <= t.header.versionNumber) {
                    stringNote += static_cast<uint8_t>(t.metadata.tuningBlock[track][string]);
                } else {
                    stringNote += static_cast<uint8_t>(t.metadata.tuningBlockLE6a[track][string]);
                }

                if (0x6d <= t.header.versionNumber) {
                    stringNote += static_cast<uint8_t>(t.metadata.transposeHalfStepsBlock[track]);
                }

                uint8_t midiNote = stringNote + STRING_MIDI_NOTE[string];

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
    const tbt_file &t,
    const char *path) {

    std::vector<uint8_t> midiBytes;

    Status ret = exportMidiBytes(t, midiBytes);

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












