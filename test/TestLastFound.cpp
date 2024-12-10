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


#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <map>


#include "last-found.inl"


class LastTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(LastTest, lastFound1) {

    std::map<int, int> m;

    m[0] = 10;
    m[1] = 11;
    m[2] = 12;
    m[3] = 13;

    auto last = last_found(m, 2);

    ASSERT_NE(last, m.rend());

    EXPECT_EQ(last->first, 2);
}

TEST_F(LastTest, lastFound2) {

    std::map<int, int> m;

    m[0] = 10;
    m[1] = 11;
    m[2] = 12;
    m[3] = 13;

    auto last = last_found(m, 4);

    ASSERT_NE(last, m.rend());

    EXPECT_EQ(last->first, 3);
}

TEST_F(LastTest, lastFound3) {

    std::map<int, int> m;

    m[1] = 11;
    m[2] = 12;
    m[3] = 13;

    auto last = last_found(m, 0);

    ASSERT_EQ(last, m.rend());
}

TEST_F(LastTest, lastFound4) {

    std::map<int, int> m;

    m[1] = 11;
    m[3] = 13;

    auto last = last_found(m, 2);

    ASSERT_NE(last, m.rend());
    
    EXPECT_EQ(last->first, 1);
}































