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
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstddef> // for size_t


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

struct tbt_track_metadata {

    uint32_t spaceCount;

    uint8_t stringCount;
    uint8_t cleanGuitar;
    uint8_t mutedGuitar;
    uint8_t volume;
    uint8_t modulation;

    // can be negative
    int16_t pitchBend;

    // can be negative
    int8_t transposeHalfSteps;

    uint8_t midiBank;
    uint8_t reverb;
    uint8_t chorus;
    uint8_t pan;
    uint8_t highestNote;
    uint8_t displayMIDINoteNumbers;

    // can be -1
    int8_t midiChannel;

    uint8_t topLineText;
    uint8_t bottomLineText;

    // can be negative
    std::array<int8_t, 8> tuning;
    std::array<int8_t, 6> tuningLE6a;

    uint8_t drums;
};

struct tbt_metadata {

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

    std::vector<tbt_track_metadata> tracks;
};

enum tbt_track_effect : uint8_t {
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

const tbt_header tbtFileHeader(const tbt_file t);

Status exportMidiFile(const tbt_file t, const char *path);

void releaseTbtFile(tbt_file t);












