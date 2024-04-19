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

#include "tbt-parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

class MidiTest : public ::testing::Test {
protected:

    static void SetUpTestSuite() {

    }

    static void TearDownTestSuite() {

        
    }

    void SetUp() override {

    }

    void TearDown() override {

    }
};


TEST_F(MidiTest, Twinkle) {

    midi_file m1;

    Status ret = parseMidiFile("/Users/brenton/development/github/tbt-examples/exported-from-tabit/twinkle.mid", m1);
    ASSERT_EQ(ret, OK);

    midi_file_times times1 = midiFileTimes(m1);

    EXPECT_EQ(times1.lastNoteOnMicros, 23000000.0);
    EXPECT_EQ(times1.lastNoteOffMicros, 24000000.0);
    EXPECT_EQ(times1.lastEndOfTrackMicros, 24000000.0);
    EXPECT_EQ(times1.lastNoteOnTick, 8832);
    EXPECT_EQ(times1.lastNoteOffTick, 9216);
    EXPECT_EQ(times1.lastEndOfTrackTick, 9216);


    tbt_file t;

    ret = parseTbtFile("/Users/brenton/development/github/tbt-examples/twinkle.tbt", t);
    ASSERT_EQ(ret, OK);

    midi_file m2;

    ret = convertToMidi(t, m2);
    ASSERT_EQ(ret, OK);

    midi_file_times times2 = midiFileTimes(m2);

    EXPECT_EQ(times2.lastNoteOnMicros, times1.lastNoteOnMicros);
    EXPECT_EQ(times2.lastNoteOffMicros, times1.lastNoteOffMicros);
    EXPECT_EQ(times2.lastEndOfTrackMicros, times1.lastEndOfTrackMicros);
    EXPECT_EQ(times2.lastNoteOnTick, times1.lastNoteOnTick);
    EXPECT_EQ(times2.lastNoteOffTick, times1.lastNoteOffTick);
    EXPECT_EQ(times2.lastEndOfTrackTick, times1.lastEndOfTrackTick);
}


TEST_F(MidiTest, Back) {

    midi_file m1;

    Status ret = parseMidiFile("/Users/brenton/development/github/tbt-examples/exported-from-tabit/back.mid", m1);
    ASSERT_EQ(ret, OK);

    midi_file_times times1 = midiFileTimes(m1);

    EXPECT_EQ(times1.lastNoteOnMicros, 181305359.0);
    EXPECT_EQ(times1.lastNoteOffMicros, 380242859.0);
    EXPECT_EQ(times1.lastEndOfTrackMicros, 380242859.0);
    EXPECT_EQ(times1.lastNoteOnTick, 90144);
    EXPECT_EQ(times1.lastNoteOffTick, 192000);
    EXPECT_EQ(times1.lastEndOfTrackTick, 192000);


    tbt_file t;

    ret = parseTbtFile("/Users/brenton/development/github/tbt-examples/back.tbt", t);
    ASSERT_EQ(ret, OK);

    midi_file m2;

    ret = convertToMidi(t, m2);
    ASSERT_EQ(ret, OK);

    midi_file_times times2 = midiFileTimes(m2);

    EXPECT_EQ(times2.lastNoteOnMicros, times1.lastNoteOnMicros);
    EXPECT_EQ(times2.lastNoteOffMicros, times1.lastNoteOffMicros);
    EXPECT_EQ(times2.lastEndOfTrackMicros, times1.lastEndOfTrackMicros);
    EXPECT_EQ(times2.lastNoteOnTick, times1.lastNoteOnTick);
    EXPECT_EQ(times2.lastNoteOffTick, times1.lastNoteOffTick);
    EXPECT_EQ(times2.lastEndOfTrackTick, times1.lastEndOfTrackTick);
}


TEST_F(MidiTest, ClosingTime) {

    midi_file m1;

    Status ret = parseMidiFile("/Users/brenton/development/github/tbt-examples/exported-from-tabit/Closing Time.mid", m1);
    ASSERT_EQ(ret, OK);

    midi_file_times times1 = midiFileTimes(m1);

    EXPECT_EQ(times1.lastNoteOnMicros, 326352889.5);
    EXPECT_EQ(times1.lastNoteOffMicros, 552926988.0);
    EXPECT_EQ(times1.lastEndOfTrackMicros, 552926988.0);
    EXPECT_EQ(times1.lastNoteOnTick, 189024);
    EXPECT_EQ(times1.lastNoteOffTick, 320256);
    EXPECT_EQ(times1.lastEndOfTrackTick, 320256);


    tbt_file t;

    ret = parseTbtFile("/Users/brenton/development/github/tbt-examples/Closing Time.tbt", t);
    ASSERT_EQ(ret, OK);

    midi_file m2;

    ret = convertToMidi(t, m2);
    ASSERT_EQ(ret, OK);

    midi_file_times times2 = midiFileTimes(m2);

    EXPECT_EQ(times2.lastNoteOnMicros, times1.lastNoteOnMicros);
    EXPECT_EQ(times2.lastNoteOffMicros, times1.lastNoteOffMicros);
    EXPECT_EQ(times2.lastEndOfTrackMicros, times1.lastEndOfTrackMicros);
    EXPECT_EQ(times2.lastNoteOnTick, times1.lastNoteOnTick);
    EXPECT_EQ(times2.lastNoteOffTick, times1.lastNoteOffTick);
    EXPECT_EQ(times2.lastEndOfTrackTick, times1.lastEndOfTrackTick);
}


TEST_F(MidiTest, JusticeNoTempoChanges) {

    midi_file m1;

    Status ret = parseMidiFile("/Users/brenton/development/github/tbt-examples/exported-from-tabit/justice-no-tempo-changes.mid", m1);
    ASSERT_EQ(ret, OK);

    midi_file_times times1 = midiFileTimes(m1);

    EXPECT_EQ(times1.lastNoteOnMicros, 947009236.0);
    EXPECT_EQ(times1.lastNoteOffMicros, 948864904.0);
    EXPECT_EQ(times1.lastEndOfTrackMicros, 948864904.0);
    EXPECT_EQ(times1.lastNoteOnTick, 293952);
    EXPECT_EQ(times1.lastNoteOffTick, 294528);
    EXPECT_EQ(times1.lastEndOfTrackTick, 294528);


    tbt_file t;

    ret = parseTbtFile("/Users/brenton/development/github/tbt-examples/justice-no-tempo-changes.tbt", t);
    ASSERT_EQ(ret, OK);

    midi_file m2;

    ret = convertToMidi(t, m2);
    ASSERT_EQ(ret, OK);

    midi_file_times times2 = midiFileTimes(m2);

    EXPECT_EQ(times2.lastNoteOnMicros, times1.lastNoteOnMicros);
    EXPECT_EQ(times2.lastNoteOffMicros, times1.lastNoteOffMicros);
    EXPECT_EQ(times2.lastEndOfTrackMicros, times1.lastEndOfTrackMicros);
    EXPECT_EQ(times2.lastNoteOnTick, times1.lastNoteOnTick);
    EXPECT_EQ(times2.lastNoteOffTick, times1.lastNoteOffTick);
    EXPECT_EQ(times2.lastEndOfTrackTick, times1.lastEndOfTrackTick);
}


TEST_F(MidiTest, Justice) {

    midi_file m1;

    Status ret = parseMidiFile("/Users/brenton/development/github/tbt-examples/exported-from-tabit/justice.mid", m1);
    ASSERT_EQ(ret, OK);

    midi_file_times times1 = midiFileTimes(m1);

    EXPECT_EQ(times1.lastNoteOnMicros, 587300235.0);
    EXPECT_EQ(times1.lastNoteOffMicros, 588359058.0);
    EXPECT_EQ(times1.lastEndOfTrackMicros, 588359058.0);
    EXPECT_EQ(times1.lastNoteOnTick, 293952);
    EXPECT_EQ(times1.lastNoteOffTick, 294528);
    EXPECT_EQ(times1.lastEndOfTrackTick, 294528);


    tbt_file t;

    ret = parseTbtFile("/Users/brenton/development/github/tbt-examples/justice.tbt", t);
    ASSERT_EQ(ret, OK);

    midi_file m2;

    ret = convertToMidi(t, m2);
    ASSERT_EQ(ret, OK);

    midi_file_times times2 = midiFileTimes(m2);

    EXPECT_EQ(times2.lastNoteOnMicros, times1.lastNoteOnMicros);
    EXPECT_EQ(times2.lastNoteOffMicros, times1.lastNoteOffMicros);
    EXPECT_EQ(times2.lastEndOfTrackMicros, times1.lastEndOfTrackMicros);
    EXPECT_EQ(times2.lastNoteOnTick, times1.lastNoteOnTick);
    EXPECT_EQ(times2.lastNoteOffTick, times1.lastNoteOffTick);
    EXPECT_EQ(times2.lastEndOfTrackTick, times1.lastEndOfTrackTick);
}


TEST_F(MidiTest, TheArcane) {

    midi_file m1;

    Status ret = parseMidiFile("/Users/brenton/development/github/tbt-examples/exported-from-tabit/The Arcane.mid", m1);
    ASSERT_EQ(ret, OK);

    midi_file_times times1 = midiFileTimes(m1);

    EXPECT_EQ(times1.lastNoteOnMicros, 131400000.0);
    EXPECT_EQ(times1.lastNoteOffMicros, 134400000.0);
    EXPECT_EQ(times1.lastEndOfTrackMicros, 134400000.0);
    EXPECT_EQ(times1.lastNoteOnTick, 84096);
    EXPECT_EQ(times1.lastNoteOffTick, 86016);
    EXPECT_EQ(times1.lastEndOfTrackTick, 86016);


    tbt_file t;

    ret = parseTbtFile("/Users/brenton/development/github/tbt-examples/The Arcane.tbt", t);
    ASSERT_EQ(ret, OK);

    midi_file m2;

    ret = convertToMidi(t, m2);
    ASSERT_EQ(ret, OK);

    midi_file_times times2 = midiFileTimes(m2);

    EXPECT_EQ(times2.lastNoteOnMicros, times1.lastNoteOnMicros);
    EXPECT_EQ(times2.lastNoteOffMicros, times1.lastNoteOffMicros);
    EXPECT_EQ(times2.lastEndOfTrackMicros, times1.lastEndOfTrackMicros);
    EXPECT_EQ(times2.lastNoteOnTick, times1.lastNoteOnTick);
    EXPECT_EQ(times2.lastNoteOffTick, times1.lastNoteOffTick);
    EXPECT_EQ(times2.lastEndOfTrackTick, times1.lastEndOfTrackTick);
}


TEST_F(MidiTest, ClassicalMadness) {

    midi_file m1;

    Status ret = parseMidiFile("/Users/brenton/development/github/tbt-examples/exported-from-tabit/Classical Madness!.mid", m1);
    ASSERT_EQ(ret, OK);

    midi_file_times times1 = midiFileTimes(m1);

    EXPECT_EQ(times1.lastNoteOnMicros, 64500000.0);
    EXPECT_EQ(times1.lastNoteOffMicros, 66000000.0);
    EXPECT_EQ(times1.lastEndOfTrackMicros, 268000000.0);
    EXPECT_EQ(times1.lastNoteOnTick, 49536);
    EXPECT_EQ(times1.lastNoteOffTick, 50688);
    EXPECT_EQ(times1.lastEndOfTrackTick, 205824);


    tbt_file t;

    ret = parseTbtFile("/Users/brenton/development/github/tbt-examples/Classical Madness!.tbt", t);
    ASSERT_EQ(ret, OK);

    midi_file m2;

    ret = convertToMidi(t, m2);
    ASSERT_EQ(ret, OK);

    midi_file_times times2 = midiFileTimes(m2);

    EXPECT_EQ(times2.lastNoteOnMicros, times1.lastNoteOnMicros);
    EXPECT_EQ(times2.lastNoteOffMicros, times1.lastNoteOffMicros);
    EXPECT_EQ(times2.lastEndOfTrackMicros, times1.lastEndOfTrackMicros);
    EXPECT_EQ(times2.lastNoteOnTick, times1.lastNoteOnTick);
    EXPECT_EQ(times2.lastNoteOffTick, times1.lastNoteOffTick);
    EXPECT_EQ(times2.lastEndOfTrackTick, times1.lastEndOfTrackTick);
}


TEST_F(MidiTest, DecomposingTruth) {

    midi_file m1;

    Status ret = parseMidiFile("/Users/brenton/development/github/tbt-examples/exported-from-tabit/[With Intent of Butchery] Decomposing Truth.mid", m1);
    ASSERT_EQ(ret, OK);

    midi_file_times times1 = midiFileTimes(m1);

    EXPECT_EQ(times1.lastNoteOnMicros, 380420720.41666668653488159);
    EXPECT_EQ(times1.lastNoteOffMicros, 382042340.41666668653488159);
    EXPECT_EQ(times1.lastEndOfTrackMicros, 382042340.41666668653488159);
    EXPECT_EQ(times1.lastNoteOnTick, 172608);
    EXPECT_EQ(times1.lastNoteOffTick, 173376);
    EXPECT_EQ(times1.lastEndOfTrackTick, 173376);


    tbt_file t;

    ret = parseTbtFile("/Users/brenton/development/github/tbt-examples/[With Intent of Butchery] Decomposing Truth.tbt", t);
    ASSERT_EQ(ret, OK);

    midi_file m2;

    ret = convertToMidi(t, m2);
    ASSERT_EQ(ret, OK);

    midi_file_times times2 = midiFileTimes(m2);

    EXPECT_EQ(times2.lastNoteOnMicros, times1.lastNoteOnMicros);
    EXPECT_EQ(times2.lastNoteOffMicros, times1.lastNoteOffMicros);
    EXPECT_EQ(times2.lastEndOfTrackMicros, times1.lastEndOfTrackMicros);
    EXPECT_EQ(times2.lastNoteOnTick, times1.lastNoteOnTick);
    EXPECT_EQ(times2.lastNoteOffTick, times1.lastNoteOffTick);
    EXPECT_EQ(times2.lastEndOfTrackTick, times1.lastEndOfTrackTick);
}













