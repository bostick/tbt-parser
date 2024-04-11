
#include "tbt-parser/tbt-parser-util.h"

#include "common/assert.h"
#include "common/check.h"
#include "common/logging.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <cstring>

#include "splitat.inl"
#include "partitioninto.inl"
#include "expanddeltalist.inl"


class UtilTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {}

  static void TearDownTestSuite() {}

  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(UtilTest, crc32_checksum1) {

  std::vector<uint8_t> data{'a', 'b', 'c'};

  auto it = data.cbegin();

  auto end = data.cend();

  uint32_t cs = crc32_checksum(it, end);

  ASSERT_EQ(cs, 0x352441c2);
}

TEST_F(UtilTest, crc32_checksum2) {

  //
  // twinkle.tbt header
  //
  std::vector<uint8_t> data{
      0x54, 0x42, 0x54, 0x6f, 0x78, 0x01, 0x03, 0x31, 0x2e, 0x36, 0x00, 0x0b,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0xb8, 0x00, 0x78, 0x00,
      0x15, 0x00, 0x00, 0x00, 0xe0, 0x7a, 0x79, 0x15, 0x8f, 0x00, 0x00, 0x00};

  auto it = data.cbegin();

  auto end = data.cend();

  uint32_t cs = crc32_checksum(it, end);

  ASSERT_EQ(cs, 0x18b670a2);
}

TEST_F(UtilTest, zlib_inflate1) {

  std::vector<uint8_t> data{0x78, 0xda, 0x63, 0x93, 0x96, 0x49, 0x60,
                            0x00, 0x02, 0x07, 0x09, 0x86, 0xff, 0x0c,
                            0xd8, 0x00, 0x00, 0x31, 0x55, 0x01, 0xf5};

  std::vector<uint8_t> test{
      0x06, 0x1b, 0x1c, 0x60, 0x00, 0x00, 0x00, 0x00, 0x40, 0x18, 0x00,
      0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  auto it = data.cbegin();

  auto end = data.cend();

  std::vector<uint8_t> inflated;
  
  Status ret = zlib_inflate(it, end, inflated);

  ASSERT_EQ(ret, OK);

  ASSERT_EQ(inflated, test);
}

TEST_F(UtilTest, splitAt1) {

  std::vector<std::vector<uint8_t> > pairs{{1, 2}, {0, 1}, {2, 0}, {2, 2},
                                          {1, 0}, {0, 0}, {1, 1}};

  std::vector<std::vector<std::vector<uint8_t> > > dest;

  splitAt(pairs.begin(), pairs.end(), dest,
          [](std::vector<uint8_t> x) { return x[0] == 0; });

  EXPECT_THAT(dest, testing::ElementsAre(
                        testing::ElementsAre(testing::ElementsAre(1, 2)),
                        testing::ElementsAre(testing::ElementsAre(0, 1),
                                             testing::ElementsAre(2, 0)),
                        testing::ElementsAre(testing::ElementsAre(2, 2)),
                        testing::ElementsAre(testing::ElementsAre(1, 0)),
                        testing::ElementsAre(testing::ElementsAre(0, 0),
                                             testing::ElementsAre(1, 1))));
}

TEST_F(UtilTest, partitionInto1) {

  std::vector<uint8_t> data{1, 2, 3, 4, 5, 6};

  std::vector<std::array<uint8_t, 2> > dest;

  Status ret = partitionInto<2>(data, dest);

  ASSERT_EQ(ret, OK);

  EXPECT_THAT(dest, testing::ElementsAre(testing::ElementsAre(1, 2),
                                         testing::ElementsAre(3, 4),
                                         testing::ElementsAre(5, 6)));
}
