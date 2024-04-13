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

class TbtTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(TbtTest, twinkle1) {

    //
    // twinkle.tbt
    //
    // clang-format off
    std::vector<uint8_t> data{
        0x54, 0x42, 0x54, 0x6f, 0x78, 0x01, 0x03, 0x31, 0x2e, 0x36, 0x00, 0x0b,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0xb8, 0x00, 0x78, 0x00,
        0x15, 0x00, 0x00, 0x00, 0xe0, 0x7a, 0x79, 0x15, 0x8f, 0x00, 0x00, 0x00,
        0xa2, 0x70, 0xb6, 0x18, 0x78, 0xda, 0x63, 0x93, 0x96, 0x49, 0x60, 0x00,
        0x02, 0x07, 0x09, 0x86, 0xff, 0x0c, 0xd8, 0x00, 0x00, 0x31, 0x55, 0x01,
        0xf5, 0x78, 0xda, 0x93, 0x60, 0xe0, 0x67, 0x60, 0x64, 0x24, 0x05, 0x87,
        0x32, 0x30, 0x32, 0x30, 0x36, 0xfb, 0x03, 0x71, 0x20, 0x03, 0x63, 0x83,
        0x3f, 0x04, 0x37, 0x41, 0x71, 0xc3, 0x3c, 0xa8, 0x1c, 0xb2, 0x18, 0x08,
        0xfb, 0x01, 0xc5, 0x16, 0x22, 0xb1, 0xd1, 0xd5, 0x2c, 0xc0, 0x23, 0x37,
        0x8f, 0x4c, 0xfb, 0xe6, 0x31, 0x00, 0x00, 0xd3, 0x54, 0x25, 0x7c
    };
    // clang-format on

    tbt_file t;

    auto it = data.cbegin();

    auto end = data.cend();

    Status ret = parseTbtBytes(it, end, t);
    ASSERT_EQ(ret, OK);

    auto versionNumber = tbtFileVersionNumber(t);
    EXPECT_EQ(versionNumber, 0x6f);

    auto versionString =  tbtFileVersionString(t);
    EXPECT_EQ(versionString, "1.6");

    auto t6f = std::get<tbt_file6f>(t);

    EXPECT_EQ(t6f.header.versionNumber, 0x6f);
    EXPECT_EQ(t6f.header.tempo1, 120);
    EXPECT_EQ(t6f.header.trackCount, 1);
    // Pascal1String
    EXPECT_THAT(t6f.header.versionString, testing::ElementsAre(3, '1', '.', '6', '\0'));
    EXPECT_EQ(t6f.header.featureBitfield, 0b00001011);
    EXPECT_EQ(t6f.header.barCount_unused, 0);
    EXPECT_EQ(t6f.header.spaceCount, 192);
    EXPECT_EQ(t6f.header.lastNonEmptySpace, 184);
    EXPECT_EQ(t6f.header.tempo2, 120);
    EXPECT_EQ(t6f.header.compressedMetadataLen, 21);
    EXPECT_EQ(t6f.header.crc32Rest, 0x15797ae0);
    EXPECT_EQ(t6f.header.totalByteCount, 143);
    EXPECT_EQ(t6f.header.crc32Header, 0x18b670a2);

    // EXPECT_THAT(t6f.metadata.stringCount, testing::ElementsAre(6));
    // EXPECT_THAT(t6f.metadata.cleanGuitar, testing::ElementsAre(27));
    // EXPECT_THAT(t6f.metadata.mutedGuitar, testing::ElementsAre(28));
    // EXPECT_THAT(t6f.metadata.volume, testing::ElementsAre(96));
    // EXPECT_THAT(t6f.metadata.transposeHalfSteps, testing::ElementsAre(0));
    // EXPECT_THAT(t6f.metadata.midiBank, testing::ElementsAre(0));
    // EXPECT_THAT(t6f.metadata.reverb, testing::ElementsAre(0));
    // EXPECT_THAT(t6f.metadata.chorus, testing::ElementsAre(0));
    // EXPECT_THAT(t6f.metadata.pan, testing::ElementsAre(64));
    // EXPECT_THAT(t6f.metadata.highestNote, testing::ElementsAre(24));
    // EXPECT_THAT(t6f.metadata.displayMIDINoteNumbers, testing::ElementsAre(0));
    // EXPECT_THAT(t6f.metadata.midiChannel, testing::ElementsAre(-1));
    // EXPECT_THAT(t6f.metadata.topLineText, testing::ElementsAre(0));
    // EXPECT_THAT(t6f.metadata.bottomLineText, testing::ElementsAre(0));
    // EXPECT_THAT(t6f.metadata.tuning, testing::ElementsAre(testing::ElementsAre(0, 0, 0, 0, 0, 0, 0, 0)));
    // EXPECT_THAT(t6f.metadata.drums, testing::ElementsAre(0));
    // Pascal2String
    EXPECT_THAT(t6f.metadata.title, testing::ElementsAre(0, 0));
    // Pascal2String
    EXPECT_THAT(t6f.metadata.artist, testing::ElementsAre(0, 0));
    // Pascal2String
    EXPECT_THAT(t6f.metadata.album, testing::ElementsAre(0, 0));
    // Pascal2String
    EXPECT_THAT(t6f.metadata.transcribedBy, testing::ElementsAre(0, 0));
    // Pascal2String
    EXPECT_THAT(t6f.metadata.comment, testing::ElementsAre(0, 0));

    // clang-format off
    std::map<uint32_t, std::array<uint8_t, 1> > expectedBars{
        { 15, {1} },  { 31, {1} },  { 47, {1} },  { 63,  {1} },
        { 79, {1} },  { 95, {1} },  { 111, {1} }, { 127, {1} },
        { 143, {1} }, { 159, {1} }, { 175, {1} }, { 191, {1} }
    };
    EXPECT_EQ(t6f.body.barsMap, expectedBars);
    // clang-format on

    // clang-format off
    std::vector<std::map<uint32_t, std::array<uint8_t, 20> > > expectedNotes{
        {
            {0, {0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {4, {0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {8, {0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {12, {0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {16, {0, 0, 0, 130, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {20, {0, 0, 0, 130, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {24, {0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {32, {0, 0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {36, {0, 0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {40, {0, 0, 130, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {44, {0, 0, 130, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {48, {0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {52, {0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {56, {0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {64, {0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {68, {0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {72, {0, 0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {76, {0, 0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {80, {0, 0, 130, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {84, {0, 0, 130, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {88, {0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {96, {0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {100, {0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {104, {0, 0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {108, {0, 0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {112, {0, 0, 130, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {116, {0, 0, 130, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {120, {0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {128, {0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {132, {0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {136, {0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {140, {0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {144, {0, 0, 0, 130, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {148, {0, 0, 0, 130, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {152, {0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {160, {0, 0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {164, {0, 0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {168, {0, 0, 130, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {172, {0, 0, 130, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {176, {0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {180, {0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {184, {0, 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
        }
    };
    // clang-format on
    EXPECT_EQ(t6f.body.notesMapList, expectedNotes);
}









