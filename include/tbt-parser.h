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


struct tbt_header70 {

    std::array<uint8_t, 3> magic;

    uint8_t versionNumber;
    uint8_t tempo1;
    uint8_t trackCount;

    // Pascal1 string
    std::array<uint8_t, 5> versionString;

    uint8_t featureBitfield;

    std::array<uint8_t, 28> unused;

    uint16_t barCount;
    uint16_t spaceCount_unused;
    uint16_t lastNonEmptySpace_unused;
    uint16_t tempo2;

    uint32_t compressedMetadataLen;
    uint32_t crc32Rest;
    uint32_t totalByteCount;
    uint32_t crc32Header;
};

static_assert(sizeof(tbt_header70) == HEADER_SIZE, "size of tbt_header70 is not correct");

struct tbt_header6f {

    std::array<uint8_t, 3> magic;

    uint8_t versionNumber;
    uint8_t tempo1;
    uint8_t trackCount;

    // Pascal1 string
    std::array<uint8_t, 5> versionString;

    uint8_t featureBitfield;

    std::array<uint8_t, 28> unused;

    uint16_t barCount_unused;
    uint16_t spaceCount;
    uint16_t lastNonEmptySpace;
    uint16_t tempo2;

    uint32_t compressedMetadataLen;
    uint32_t crc32Rest;
    uint32_t totalByteCount;
    uint32_t crc32Header;
};

static_assert(sizeof(tbt_header6f) == HEADER_SIZE, "size of tbt_header6f is not correct");

struct tbt_header6e {

    std::array<uint8_t, 3> magic;

    uint8_t versionNumber;
    uint8_t tempo1;
    uint8_t trackCount;

    // Pascal1 string
    std::array<uint8_t, 5> versionString;

    uint8_t featureBitfield;

    std::array<uint8_t, 28> unused;

    uint16_t barCount_unused;
    uint16_t spaceCount_unused;
    uint16_t lastNonEmptySpace;
    uint16_t tempo2;

    uint32_t compressedMetadataLen;
    uint32_t crc32Rest;
    uint32_t totalByteCount;
    uint32_t crc32Header;
};

static_assert(sizeof(tbt_header6e) == HEADER_SIZE, "size of tbt_header6e is not correct");

struct tbt_header68 {

    std::array<uint8_t, 3> magic;

    uint8_t versionNumber;
    uint8_t tempo1;
    uint8_t trackCount;

    // Pascal1 string
    std::array<uint8_t, 5> versionString;

    uint8_t featureBitfield;

    std::array<uint8_t, 28> unused;

    uint16_t barCount_unused;
    uint16_t spaceCount_unused;
    uint16_t lastNonEmptySpace_unused;
    uint16_t tempo2_unused;

    uint32_t compressedMetadataLen;
    uint32_t crc32Rest;
    uint32_t totalByteCount;
    uint32_t crc32Header;
};

static_assert(sizeof(tbt_header68) == HEADER_SIZE, "size of tbt_header68 is not correct");

struct tbt_header65 {

    std::array<uint8_t, 3> magic;

    uint8_t versionNumber;
    uint8_t tempo1;
    uint8_t trackCount;

    // Pascal1 string
    std::array<uint8_t, 5> versionString;

    uint8_t featureBitfield;

    std::array<uint8_t, 28> unused;

    uint16_t barCount_unused;
    uint16_t spaceCount_unused;
    uint16_t lastNonEmptySpace_unused;
    uint16_t tempo2_unused;

    uint32_t metadataLen_unused;
    uint32_t crc32Rest_unused;
    uint32_t totalByteCount_unused;
    uint32_t crc32Header_unused;
};

static_assert(sizeof(tbt_header65) == HEADER_SIZE, "size of tbt_header65 is not correct");


#pragma pack(push, 1)

struct tbt_track_metadata71 {

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

    uint8_t drums;
};

#pragma pack(pop)

static_assert(sizeof(tbt_track_metadata71) == 30);

#pragma pack(push, 1)

struct tbt_track_metadata70 {

    uint32_t spaceCount;

    uint8_t stringCount;
    uint8_t cleanGuitar;
    uint8_t mutedGuitar;
    uint8_t volume;

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

    uint8_t drums;
};

#pragma pack(pop)

static_assert(sizeof(tbt_track_metadata70) == 27);

#pragma pack(push, 1)

struct tbt_track_metadata6e {

    uint8_t stringCount;
    uint8_t cleanGuitar;
    uint8_t mutedGuitar;
    uint8_t volume;

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

    uint8_t drums;
};

#pragma pack(pop)

static_assert(sizeof(tbt_track_metadata6e) == 23);

#pragma pack(push, 1)

struct tbt_track_metadata6b {

    uint8_t stringCount;
    uint8_t cleanGuitar;
    uint8_t mutedGuitar;
    uint8_t volume;

    uint8_t pan;
    uint8_t highestNote;
    uint8_t displayMIDINoteNumbers;

    // can be -1
    int8_t midiChannel;

    uint8_t topLineText;
    uint8_t bottomLineText;

    // can be negative
    std::array<int8_t, 8> tuning;

    uint8_t drums;
};

#pragma pack(pop)

static_assert(sizeof(tbt_track_metadata6b) == 19);

#pragma pack(push, 1)

struct tbt_track_metadata6a {

    uint8_t stringCount;
    uint8_t cleanGuitar;
    uint8_t mutedGuitar;
    uint8_t volume;

    uint8_t displayMIDINoteNumbers;

    // can be -1
    int8_t midiChannel;

    uint8_t topLineText;
    uint8_t bottomLineText;

    // can be negative
    std::array<int8_t, 6> tuning;

    uint8_t drums;
};

#pragma pack(pop)

static_assert(sizeof(tbt_track_metadata6a) == 15);

#pragma pack(push, 1)

struct tbt_track_metadata65 {

    uint8_t stringCount;
    uint8_t cleanGuitar;
    uint8_t mutedGuitar;
    uint8_t volume;

    uint8_t topLineText;
    uint8_t bottomLineText;

    // can be negative
    std::array<int8_t, 6> tuning;

    uint8_t drums;
};

#pragma pack(pop)

static_assert(sizeof(tbt_track_metadata65) == 13);


struct tbt_metadata71 {

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

    std::vector<tbt_track_metadata71> tracks;
};

struct tbt_metadata70 {

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

    std::vector<tbt_track_metadata70> tracks;
};

struct tbt_metadata6e {

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

    std::vector<tbt_track_metadata6e> tracks;
};

struct tbt_metadata6b {

    // Pascal1 string
    std::vector<uint8_t> title;
    // Pascal1 string
    std::vector<uint8_t> artist;
    // Pascal1 string
    std::vector<uint8_t> comment;

    std::vector<tbt_track_metadata6b> tracks;
};

struct tbt_metadata6a {

    // Pascal1 string
    std::vector<uint8_t> title;
    // Pascal1 string
    std::vector<uint8_t> artist;
    // Pascal1 string
    std::vector<uint8_t> comment;

    std::vector<tbt_track_metadata6a> tracks;
};

struct tbt_metadata65 {

    // Pascal1 string
    std::vector<uint8_t> title;
    // Pascal1 string
    std::vector<uint8_t> artist;
    // Pascal1 string
    std::vector<uint8_t> comment;

    std::vector<tbt_track_metadata65> tracks;
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

struct tbt_body71 {
    std::unordered_map<uint32_t, std::array<uint8_t, 2> > barsMap;
    uint32_t barsSpaceCount;
    std::vector<std::unordered_map<uint32_t, std::array<uint8_t, 20> > > notesMapList;
    std::vector<std::unordered_map<uint32_t, std::array<uint8_t, 2> > > alternateTimeRegionsMapList;
    std::vector<std::unordered_map<uint32_t, std::vector<tbt_track_effect_change> > > trackEffectChangesMapList;
};

struct tbt_body70 {
    std::unordered_map<uint32_t, std::array<uint8_t, 2> > barsMap;
    uint32_t barsSpaceCount;
    std::vector<std::unordered_map<uint32_t, std::array<uint8_t, 20> > > notesMapList;
    std::vector<std::unordered_map<uint32_t, std::array<uint8_t, 2> > > alternateTimeRegionsMapList;
};

struct tbt_body6b {
    std::unordered_map<uint32_t, std::array<uint8_t, 1> > barsMap;
    std::vector<std::unordered_map<uint32_t, std::array<uint8_t, 20> > > notesMapList;
};

struct tbt_body65 {
    std::unordered_map<uint32_t, std::array<uint8_t, 1> > barsMap;
    std::vector<std::unordered_map<uint32_t, std::array<uint8_t, 16> > > notesMapList;
};


struct tbt_file71 {
    tbt_header70 header;
    tbt_metadata71 metadata;
    tbt_body71 body;
};

struct tbt_file70 {
    tbt_header70 header;
    tbt_metadata70 metadata;
    tbt_body70 body;
};

struct tbt_file6f {
    tbt_header6f header;
    tbt_metadata6e metadata;
    tbt_body6b body;
};

struct tbt_file6e {
    tbt_header6e header;
    tbt_metadata6e metadata;
    tbt_body6b body;
};

struct tbt_file6b {
    tbt_header68 header;
    tbt_metadata6b metadata;
    tbt_body6b body;
};

struct tbt_file6a {
    tbt_header68 header;
    tbt_metadata6a metadata;
    tbt_body65 body;
};

struct tbt_file68 {
    tbt_header68 header;
    tbt_metadata65 metadata;
    tbt_body65 body;
};

struct tbt_file65 {
    tbt_header65 header;
    tbt_metadata65 metadata;
    tbt_body65 body;
};

enum Status {
    OK,
    ERR
};


typedef void *tbt_file;


Status parseTbtFile(const char *path, tbt_file *out);

const std::array<uint8_t, HEADER_SIZE> tbtFileHeader(const tbt_file t);

Status exportMidiFile(const tbt_file t, const char *path);

void releaseTbtFile(tbt_file t);












