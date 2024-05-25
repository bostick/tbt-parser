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

#pragma once

#include "common/status.h"

#include <array>
#include <vector>
#include <map>
#include <variant>
#include <string>
#include <cstdint>
#include <cstddef> // for size_t


const int HEADER_SIZE = 64;


struct tbt_header70 {

    std::array<uint8_t, 3> magic;

    uint8_t versionNumber;
    uint8_t tempo1;
    uint8_t trackCount;

    // Pascal1 string
    std::array<char, 5> versionString;

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
    std::array<char, 5> versionString;

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
    std::array<char, 5> versionString;

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
    std::array<char, 5> versionString;

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
    std::array<char, 5> versionString;

    uint8_t featureBitfield;

    std::array<uint8_t, 28> unused;

    uint16_t barCount_unused;
    uint16_t spaceCount_unused;
    uint16_t lastNonEmptySpace_unused;
    uint16_t tempo2_unused;

    uint32_t compressedMetadataLen_unused;
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

static_assert(sizeof(tbt_track_metadata71) == 30, "size of tbt_track_metadata71 is not correct");

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

static_assert(sizeof(tbt_track_metadata70) == 27, "size of tbt_track_metadata70 is not correct");

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

static_assert(sizeof(tbt_track_metadata6e) == 23, "size of tbt_track_metadata6e is not correct");

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

static_assert(sizeof(tbt_track_metadata6b) == 19, "size of tbt_track_metadata6b is not correct");

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

static_assert(sizeof(tbt_track_metadata6a) == 15, "size of tbt_track_metadata6a is not correct");

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

static_assert(sizeof(tbt_track_metadata65) == 13, "size of tbt_track_metadata65 is not correct");


struct tbt_metadata71 {

    // Pascal2 string
    std::vector<char> title;
    // Pascal2 string
    std::vector<char> artist;
    // Pascal2 string
    std::vector<char> album;
    // Pascal2 string
    std::vector<char> transcribedBy;
    // Pascal2 string
    std::vector<char> comment;

    std::vector<tbt_track_metadata71> tracks;
};

struct tbt_metadata70 {

    // Pascal2 string
    std::vector<char> title;
    // Pascal2 string
    std::vector<char> artist;
    // Pascal2 string
    std::vector<char> album;
    // Pascal2 string
    std::vector<char> transcribedBy;
    // Pascal2 string
    std::vector<char> comment;

    std::vector<tbt_track_metadata70> tracks;
};

struct tbt_metadata6e {

    // Pascal2 string
    std::vector<char> title;
    // Pascal2 string
    std::vector<char> artist;
    // Pascal2 string
    std::vector<char> album;
    // Pascal2 string
    std::vector<char> transcribedBy;
    // Pascal2 string
    std::vector<char> comment;

    std::vector<tbt_track_metadata6e> tracks;
};

struct tbt_metadata6b {

    // Pascal1 string
    std::vector<char> title;
    // Pascal1 string
    std::vector<char> artist;
    // Pascal1 string
    std::vector<char> comment;

    std::vector<tbt_track_metadata6b> tracks;
};

struct tbt_metadata6a {

    // Pascal1 string
    std::vector<char> title;
    // Pascal1 string
    std::vector<char> artist;
    // Pascal1 string
    std::vector<char> comment;

    std::vector<tbt_track_metadata6a> tracks;
};

struct tbt_metadata65 {

    // Pascal1 string
    std::vector<char> title;
    // Pascal1 string
    std::vector<char> artist;
    // Pascal1 string
    std::vector<char> comment;

    std::vector<tbt_track_metadata65> tracks;
};

enum tbt_track_effect : uint8_t {
    TE_STROKE_DOWN = 1,
    TE_STROKE_UP = 2,
    TE_TEMPO = 3,
    TE_INSTRUMENT = 4,
    TE_VOLUME = 5,
    TE_PAN = 6,
    TE_CHORUS = 7,
    TE_REVERB = 8,
    TE_MODULATION = 9,
    TE_PITCH_BEND = 10,
};

struct maps71 {
    std::map<uint16_t, std::array<uint8_t, 20> > notesMap;
    std::map<uint16_t, std::array<uint8_t, 2> > alternateTimeRegionsMap;
    std::map<uint16_t, std::map<tbt_track_effect, uint16_t> > trackEffectChangesMap;
};

struct maps70 {
    std::map<uint16_t, std::array<uint8_t, 20> > notesMap;
    std::map<uint16_t, std::array<uint8_t, 2> > alternateTimeRegionsMap;
};

struct maps6b {
    std::map<uint16_t, std::array<uint8_t, 20> > notesMap;
};

struct maps65 {
    std::map<uint16_t, std::array<uint8_t, 16> > notesMap;
};

struct tbt_body71 {
    std::map<uint16_t, std::array<uint8_t, 2> > barLinesMap;
    uint16_t barLinesSpaceCount;
    std::vector<maps71> mapsList;
};

struct tbt_body70 {
    std::map<uint16_t, std::array<uint8_t, 2> > barLinesMap;
    uint16_t barLinesSpaceCount;
    std::vector<maps70> mapsList;
};

struct tbt_body6b {
    std::map<uint16_t, std::array<uint8_t, 1> > barLinesMap;
    std::vector<maps6b> mapsList;
};

struct tbt_body65 {
    std::map<uint16_t, std::array<uint8_t, 1> > barLinesMap;
    std::vector<maps65> mapsList;
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

using tbt_file = std::variant<tbt_file65, tbt_file68, tbt_file6a, tbt_file6b, tbt_file6e, tbt_file6f, tbt_file70, tbt_file71>;


struct midi_convert_opts {

    //
    // Custom Lyric events may be used to trigger callbacks in FluidSynth
    //
    bool emit_custom_lyric_events = false;
};


struct midi_header {
    uint16_t format;
    uint16_t trackCount;
    uint16_t division;
};


struct ProgramChangeEvent {
    int32_t deltaTime;
    uint8_t channel;
    uint8_t midiProgram;
};

struct PitchBendEvent {
    int32_t deltaTime;
    uint8_t channel;
    int16_t pitchBend;
};

struct NoteOffEvent {
    int32_t deltaTime;
    uint8_t channel;
    uint8_t midiNote;
    uint8_t velocity;
};

struct NoteOnEvent {
    int32_t deltaTime;
    uint8_t channel;
    uint8_t midiNote;
    uint8_t velocity;
};

struct ControlChangeEvent {
    int32_t deltaTime;
    uint8_t channel;
    uint8_t controller;
    uint8_t value;
};

struct MetaEvent {
    int32_t deltaTime;
    uint8_t type;
    std::vector<uint8_t> data;
};

struct PolyphonicKeyPressureEvent {
    int32_t deltaTime;
    uint8_t channel;
    uint8_t midiNote;
    uint8_t pressure;
};

struct ChannelPressureEvent {
    int32_t deltaTime;
    uint8_t channel;
    uint8_t pressure;
};

struct SysExEvent {
    int32_t deltaTime;
    std::vector<uint8_t> data;
};

using midi_track_event = std::variant<ProgramChangeEvent, PitchBendEvent, NoteOffEvent, NoteOnEvent,
    ControlChangeEvent, MetaEvent, PolyphonicKeyPressureEvent, ChannelPressureEvent, SysExEvent>;

struct midi_file {
    midi_header header;
    std::vector<std::vector<midi_track_event> > tracks;
};


Status parseTbtFile(const char *path, tbt_file &out);

Status parseTbtBytes(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    tbt_file &out);

uint8_t tbtFileVersionNumber(const tbt_file &t);

std::string tbtFileVersionString(const tbt_file &t);

std::string tbtFileInfo(const tbt_file &t);

Status convertToMidi(const tbt_file &t, const midi_convert_opts &opts, midi_file &m);

Status exportMidiFile(const midi_file &m, const char *path);

Status exportMidiBytes(const midi_file &m, std::vector<uint8_t> &out);

Status parseMidiFile(const char *path, midi_file &out);

Status parseMidiBytes(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    midi_file &out);

struct midi_file_times {
    double lastNoteOnMicros;
    double lastNoteOffMicros;
    double lastEndOfTrackMicros;

    int32_t lastNoteOnTick;
    int32_t lastNoteOffTick;
    int32_t lastEndOfTrackTick;
};

midi_file_times midiFileTimes(const midi_file &m);

void midiFileInfo(const midi_file &m);











