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
#include <map>
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
    uint16_t lastNonEmptySpace;
    uint16_t tempo2;

    uint32_t metadataLen;
    uint32_t crc32Rest;
    uint32_t totalByteCount;
    uint32_t crc32Header;
};

static_assert(sizeof(tbt_header) == HEADER_SIZE, "size of tbt_header is not correct");


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

struct tbt_track_metadata6d {

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

static_assert(sizeof(tbt_track_metadata6d) == 23);

#pragma pack(push, 1)

struct tbt_track_metadata6c {

    uint8_t stringCount;
    uint8_t cleanGuitar;
    uint8_t mutedGuitar;
    uint8_t volume;

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

static_assert(sizeof(tbt_track_metadata6c) == 21);

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

struct tbt_metadata6d {

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

    std::vector<tbt_track_metadata6d> tracks;
};

struct tbt_metadata6c {

    // Pascal1 string
    std::vector<uint8_t> title;
    // Pascal1 string
    std::vector<uint8_t> artist;
    // Pascal1 string
    std::vector<uint8_t> comment;

    std::vector<tbt_track_metadata6c> tracks;
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
    std::map<uint32_t, std::array<uint8_t, 2> > barsMap;
    uint32_t barsSpaceCount;
    std::vector<std::map<uint32_t, std::array<uint8_t, 20> > > notesMapList;
    std::vector<std::map<uint32_t, std::array<uint8_t, 2> > > alternateTimeRegionsMapList;
    std::vector<std::map<uint32_t, std::vector<tbt_track_effect_change> > > trackEffectChangesMapList;
};

struct tbt_body70 {
    std::map<uint32_t, std::array<uint8_t, 2> > barsMap;
    uint32_t barsSpaceCount;
    std::vector<std::map<uint32_t, std::array<uint8_t, 20> > > notesMapList;
    std::vector<std::map<uint32_t, std::array<uint8_t, 2> > > alternateTimeRegionsMapList;
};

struct tbt_body6b {
    std::map<uint32_t, std::array<uint8_t, 1> > barsMap;
    std::vector<std::map<uint32_t, std::array<uint8_t, 20> > > notesMapList;
};

struct tbt_body65 {
    std::map<uint32_t, std::array<uint8_t, 1> > barsMap;
    std::vector<std::map<uint32_t, std::array<uint8_t, 16> > > notesMapList;
};


struct tbt_file71 {
    tbt_header header;
    tbt_metadata71 metadata;
    tbt_body71 body;
};

struct tbt_file70 {
    tbt_header header;
    tbt_metadata70 metadata;
    tbt_body70 body;
};

struct tbt_file6d {
    tbt_header header;
    tbt_metadata6d metadata;
    tbt_body6b body;
};

struct tbt_file6c {
    tbt_header header;
    tbt_metadata6c metadata;
    tbt_body6b body;
};

struct tbt_file6b {
    tbt_header header;
    tbt_metadata6b metadata;
    tbt_body6b body;
};

struct tbt_file6a {
    tbt_header header;
    tbt_metadata6a metadata;
    tbt_body65 body;
};

struct tbt_file65 {
    tbt_header header;
    tbt_metadata65 metadata;
    tbt_body65 body;
};

enum Status {
    OK,
    ERR
};


typedef void *tbt_file;


Status parseTbtFile(const char *path, tbt_file *out);

const tbt_header tbtFileHeader(const tbt_file t);

Status exportMidiFile(const tbt_file t, const char *path);

void releaseTbtFile(tbt_file t);












