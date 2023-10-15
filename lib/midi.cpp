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

#include "tbt-parser.h"

#include "tbt-parser/tbt-parser-util.h"

#include "common/common.h"

#include <cinttypes>
#include <cmath> // for round
#include <algorithm> // for remove


#define TAG "midi"


const uint8_t TICKS_PER_BEAT = 0xc0; // 192
const uint8_t TICKS_PER_SPACE = (TICKS_PER_BEAT / 4);

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
    // TabIt has 15 tracks
    //
    // Channel 9 is for drums and is not generally available
    //
    std::vector<uint8_t> availableChannels{ 0, 1, 2, 3, 4, 5, 6, 7, 8, /*9,*/ 10, 11, 12, 13, 14, 15 };

    //
    // first just treat any assigned channels as unavailable
    //
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

    //
    // availableChannels now holds generally available channels
    //

    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        if (t.metadata.midiChannelBlock[track] != -1) {
            continue;
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
insertTempoMap_atSpaceGE71(
    const std::unordered_map<uint32_t, std::vector<tbt_track_effect_change> > &trackEffectChangesMap,
    uint32_t space,
    std::unordered_map<uint32_t, uint16_t> &tempoMap) {

    const auto &it = trackEffectChangesMap.find(space);
    if (it == trackEffectChangesMap.end()) {
        return;
    }

    const auto &changes = it->second;

    for (const auto &change : changes) {

        if (change.effect != TEMPO) {
            continue;
        }

        uint16_t newTempo = change.value;

        const auto &tempoMapIt = tempoMap.find(space);
        if (tempoMapIt != tempoMap.end()) {
            if (tempoMapIt->second != newTempo) {
                LOGW("space %d has conflicting tempo changes: %d, %d", space, tempoMapIt->second, newTempo);
            }
        }

        tempoMap[space] = newTempo;
    }
}


void
insertTempoMap_atSpace(
    const std::unordered_map<uint32_t, std::array<uint8_t, 20> > &notesMap,
    uint32_t space,
    std::unordered_map<uint32_t, uint16_t> &tempoMap) {

    const auto &it = notesMap.find(space);
    if (it == notesMap.end()) {
        return;
    }

    const auto &vsqs = it->second;

    auto trackEffect = vsqs[16];

    switch (trackEffect) {
        case 'T': {

            uint16_t newTempo = vsqs[19];

            const auto &tempoMapIt = tempoMap.find(space);
            if (tempoMapIt != tempoMap.end()) {
                if (tempoMapIt->second != newTempo) {
                    LOGW("space %d has conflicting tempo changes: %d, %d", space, tempoMapIt->second, newTempo);
                }
            }

            tempoMap[space] = newTempo;

            break;
        }
        case 't': {

            uint16_t newTempo = vsqs[19] + 250;

            const auto &tempoMapIt = tempoMap.find(space);
            if (tempoMapIt != tempoMap.end()) {
                if (tempoMapIt->second != newTempo) {
                    LOGW("space %d has conflicting tempo changes: %d, %d", space, tempoMapIt->second, newTempo);
                }
            }

            tempoMap[space] = newTempo;

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

                insertTempoMap_atSpaceGE71(trackEffectChangesMap, space, tempoMap);

            } else {

                const auto &notesMap = t.body.notesMapList[track];

                insertTempoMap_atSpace(notesMap, space, tempoMap);
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
        uint32_t tick = 0;
        uint32_t lastEventTick;

        out.insert(out.end(), { 'M', 'T', 'r', 'k' }); // type

        tmp.clear();

        tmp.insert(tmp.end(), { 0x00, 0xff, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08 }); // time signature

        //
        // Emit tempo
        //
        {
            uint16_t tempo = t.header.tempo2;

            //
            // Use floating point here for better accuracy
            //
            auto microsPerBeat = static_cast<uint32_t>(round(60000000.0 / tempo));
            auto microsPerBeatBytes = toDigitsBE(microsPerBeat);

            tmp.insert(tmp.end(), { 0x00, 0xff, 0x51 }); // tempo change, 0x07a120 == 500000 MMPB == 120 BPM

            //
            // FluidSynth hard-codes length of 3, so just do the same
            //
            tmp.insert(tmp.end(), { 3 }); // microsPerBeatBytes size VLQ
            tmp.insert(tmp.end(), microsPerBeatBytes.cbegin() + 1, microsPerBeatBytes.cend());

            lastEventTick = tick;
            //
            // no increment, still at 0
            //
//            tick += TICKS_PER_SPACE;
        }

        for (uint32_t space = 0; space < barsSpaceCount + 1; space++) { // + 1 for any still playing at end

            //
            // Emit tempo changes
            //
            const auto &it = tempoMap.find(space);
            if (it != tempoMap.end()) {

                uint16_t tempo = it->second;

                //
                // Use floating point here for better accuracy
                //
                auto microsPerBeat = static_cast<uint32_t>(round(60000000.0 / tempo));
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

            tick += TICKS_PER_SPACE;
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
        uint32_t tick = 0;
        uint32_t lastEventTick = 0;
        std::array<uint8_t, 8> currentlyPlayingStrings{};

        out.insert(out.end(), { 'M', 'T', 'r', 'k' }); // type

        tmp.clear();

        tmp.insert(tmp.end(), {
            0x00,
            static_cast<uint8_t>(0xc0 | channel), // program change
            static_cast<uint8_t>(midiProgram)
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
                                    static_cast<uint8_t>(midiProgram)
                                });

                                lastEventTick = tick;

                                break;
                            }
                            case VOLUME: {

                                auto newVolume = change.value;

                                volume = static_cast<uint8_t>(newVolume);

                                break;
                            }
                            case SKIP:
                            case STROKE_DOWN:
                            case STROKE_UP:
                            case TEMPO:
                            case PAN:
                            case CHORUS:
                            case REVERB:
                            case MODULATION:
                            case PITCH_BEND:
                                break;
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
                        case 'I': {

                            auto newInstrument = vsqs[19];

                            dontLetRing = ((newInstrument & 0b10000000) == 0b10000000);
                            midiProgram = (newInstrument & 0b01111111);

                            auto diff = tick - lastEventTick;

                            auto vlq = toVLQ(diff);

                            tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                            tmp.insert(tmp.end(), {
                                static_cast<uint8_t>(0xc0 | channel), // program change
                                static_cast<uint8_t>(midiProgram)
                            });

                            lastEventTick = tick;

                            break;
                        }
                        case 'V': {

                            auto newVolume = vsqs[19];

                            volume = newVolume;

                            break;
                        }
                        case 0x00:
                        case 'C':
                        case 'D':
                        case 'P':
                        case 'R':
                        case 'T':
                        case 'U':
                        case 't':
                            break;
                        default:
                            ASSERT(false);
                            break;
                        }
                    }
                }
            }

            //
            // Emit note offs and note ons
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

                        offVsqs = currentlyPlayingStrings;

                        for (uint8_t string = 0; string < stringCount; string++) {

                            uint8_t on = onVsqs[string];

                            if (on == 0) {

                                currentlyPlayingStrings[string] = 0;

                            } else if (on == MUTED ||
                                    on == STOPPED) {

                                currentlyPlayingStrings[string] = 0;

                            } else if (on >= 0x80) {

                                currentlyPlayingStrings[string] = on;

                            } else {
                                ASSERT(false);
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

                                } else if (on >= 0x80) {

                                    offVsqs[string] = 0;
                                    currentlyPlayingStrings[string] = on;

                                } else {
                                    ASSERT(false);
                                }

                            } else if (current >= 0x80) {

                                uint8_t on = onVsqs[string];

                                if (on == 0) {

                                    offVsqs[string] = 0;

                                } else if (on == MUTED ||
                                        on == STOPPED) {

                                    offVsqs[string] = current;
                                    currentlyPlayingStrings[string] = 0;

                                } else if (on >= 0x80) {

                                    offVsqs[string] = current;
                                    currentlyPlayingStrings[string] = on;

                                } else {
                                    ASSERT(false);
                                }

                            } else {
                                ASSERT(false);
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

                        stringNote += static_cast<uint8_t>(t.metadata.transposeHalfStepsBlock[track]);

                        uint8_t midiNote = stringNote + STRING_MIDI_NOTE[string];

                        auto diff = tick - lastEventTick;

                        auto vlq = toVLQ(diff);

                        tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                        tmp.insert(tmp.end(), { static_cast<uint8_t>(0x80 | channel) }); // note off

                        tmp.insert(tmp.end(), { midiNote });

                        tmp.insert(tmp.end(), { 0x00 }); // velocity

                        lastEventTick = tick;
                    }

                    //
                    // Emit note ons
                    //
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
                        
                        stringNote += static_cast<uint8_t>(t.metadata.transposeHalfStepsBlock[track]);

                        uint8_t midiNote = stringNote + STRING_MIDI_NOTE[string];

                        auto diff = tick - lastEventTick;

                        auto vlq = toVLQ(diff);

                        tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                        tmp.insert(tmp.end(), { static_cast<uint8_t>(0x90 | channel) }); // note on

                        tmp.insert(tmp.end(), { midiNote });

                        tmp.insert(tmp.end(), { volume }); // velocity

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

                tick += static_cast<uint32_t>(denominator * TICKS_PER_SPACE / numerator);
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

                stringNote += static_cast<uint8_t>(t.metadata.transposeHalfStepsBlock[track]);

                uint8_t midiNote = stringNote + STRING_MIDI_NOTE[string];

                auto diff = tick - lastEventTick;

                auto vlq = toVLQ(diff);

                tmp.insert(tmp.end(), vlq.cbegin(), vlq.cend());

                tmp.insert(tmp.end(), { static_cast<uint8_t>(0x80 | channel) }); // note off

                tmp.insert(tmp.end(), { midiNote });

                tmp.insert(tmp.end(), { 0x00 }); // velocity

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












