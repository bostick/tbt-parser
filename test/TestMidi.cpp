
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
}


TEST_F(MidiTest, Back) {

    midi_file m1;

    Status ret = parseMidiFile("/Users/brenton/development/github/tbt-examples/exported-from-tabit/back.mid", m1);
    ASSERT_EQ(ret, OK);

    midi_file_times times1 = midiFileTimes(m1);

    EXPECT_EQ(times1.lastNoteOnMicros, 181305359.0);
    EXPECT_EQ(times1.lastNoteOffMicros, 380242859.0);
    EXPECT_EQ(times1.lastEndOfTrackMicros, 380242859.0);


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
}


TEST_F(MidiTest, ClosingTime) {

    midi_file m1;

    Status ret = parseMidiFile("/Users/brenton/development/github/tbt-examples/exported-from-tabit/Closing Time.mid", m1);
    ASSERT_EQ(ret, OK);

    midi_file_times times1 = midiFileTimes(m1);

    EXPECT_EQ(times1.lastNoteOnMicros, 326352889.5);
    EXPECT_EQ(times1.lastNoteOffMicros, 552926988.0);
    EXPECT_EQ(times1.lastEndOfTrackMicros, 552926988.0);


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
}









