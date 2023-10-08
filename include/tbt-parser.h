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

#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <unordered_map>

const int HEADER_SIZE = 64;

struct tbt_header {

    std::array<uint8_t, 3> magic;

    uint8_t versionNumber;
    uint8_t tempo1;
    uint8_t trackCount;

    // Pascal1 string
    std::array<uint8_t, 5> versionString;

    uint8_t featureBitfield;

    std::array<uint8_t, 28> unused;

    uint16_t barCountGE70;
    uint16_t spaceCount6f;
    uint16_t lastNonEmptySpaceLE6f;
    uint16_t tempo2;

    uint32_t metadataLen;
    uint32_t crc32Rest;
    uint32_t totalByteCount;
    uint32_t crc32Header;
};

static_assert(sizeof(tbt_header) == HEADER_SIZE, "size of tbt_header is not correct");

struct tbt_metadata {

    std::vector<uint32_t> spaceCountBlock;

    std::vector<uint8_t> stringCountBlock;
    std::vector<uint8_t> cleanGuitarBlock;
    std::vector<uint8_t> mutedGuitarBlock;
    std::vector<uint8_t> volumeBlock;
    std::vector<uint8_t> modulationBlock;

    std::vector<uint16_t> pitchBendBlock;

    // can be negative
    std::vector<int8_t> transposeHalfStepsBlock;

    std::vector<uint8_t> midiBankBlock;
    std::vector<uint8_t> reverbBlock;
    std::vector<uint8_t> chorusBlock;
    std::vector<uint8_t> panBlock;
    std::vector<uint8_t> highestNoteBlock;
    std::vector<uint8_t> displayMIDINoteNumbersBlock;

    // can be -1
    std::vector<int8_t> midiChannelBlock;

    std::vector<uint8_t> topLineTextBlock;
    std::vector<uint8_t> bottomLineTextBlock;

    // can be negative
    std::vector<std::array<int8_t, 8> > tuningBlock;
    std::vector<std::array<int8_t, 6> > tuningBlockLE6a;

    std::vector<uint8_t> drumsBlock;

    // Pascal2 string
    std::vector<uint8_t> title;
    // Pascal2 string
    std::vector<uint8_t> artist;
    // Pascal2 string
    std::vector<uint8_t> album;
    // Pascal2 string
    std::vector<uint8_t> transcribedBy;
    // Pascal2 string
    std::vector<uint8_t> comment;
};

enum tbt_track_effect : uint8_t {
    SKIP = 0,
    STROKE_DOWN = 1,
    STROKE_UP = 2,
    TEMPO = 3,
    INSTRUMENT = 4,
    VOLUME = 5,
    PAN = 6,
    CHORUS = 7,
    REVERB = 8,
    MODULATION = 9,
    PITCH_BEND = 10,
};

struct tbt_track_effect_change {
    tbt_track_effect effect;
    uint16_t value;
};

struct tbt_body {
    std::unordered_map<uint32_t, std::array<uint8_t, 1> > barsMap;
    std::unordered_map<uint32_t, std::array<uint8_t, 2> > barsMapGE70;
    uint32_t barsSpaceCountGE70;
    std::vector<std::unordered_map<uint32_t, std::array<uint8_t, 20> > > notesMapList;
    std::vector<std::unordered_map<uint32_t, std::array<uint8_t, 2> > > alternateTimeRegionsMapList;
    std::vector<std::unordered_map<uint32_t, std::vector<tbt_track_effect_change> > > trackEffectChangesMapList;
};

struct tbt_file {
    tbt_header header;
    tbt_metadata metadata;
    tbt_body body;
};

enum Status {
    OK,
    ERR
};

Status parseTbtFile(const char *path, tbt_file *out);

Status parseTbtBytes(const uint8_t *data, size_t len, tbt_file *out);

Status exportMidiFile(const tbt_file &t, const char *path);

Status exportMidiBytes(const tbt_file &t, std::vector<uint8_t> &out);












