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

#include "tbt-parser/rational.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

class RationalTest : public ::testing::Test {
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


TEST_F(RationalTest, basic) {

   auto tick = rational{189024, 1};

   auto lastMicrosPerTick = rational{110497, 64};

   auto inc = tick * lastMicrosPerTick;

   EXPECT_EQ(inc.numerator(), 652705779);
   EXPECT_EQ(inc.denominator(), 2);
}


TEST_F(RationalTest, pitchBend1) {
    
    //
    // important that value of 0 goes to value of 0x2000 (8192)
    //
    auto a = (((rational(0) + 2400) * 16383) / (2 * 2400)).round();
    auto b = rational{8192, 1};
    EXPECT_EQ(a, b);
    
    a = (((rational(-2400) + 2400) * 16383) / (2 * 2400)).round();
    b = rational{0, 1};
    EXPECT_EQ(a, b);
    
    a = (((rational(2400) + 2400) * 16383) / (2 * 2400)).round();
    b = rational{16383, 1};
    EXPECT_EQ(a, b);
}


TEST_F(RationalTest, negative) {

   auto r = rational{-11, 5};

   EXPECT_EQ(r.numerator(), -11);
   EXPECT_EQ(r.denominator(), 5);

   r = rational{-12, 3};

   EXPECT_EQ(r.numerator(), -4);
   EXPECT_EQ(r.denominator(), 1);

   r = rational{12, -3};

   EXPECT_EQ(r.numerator(), -4);
   EXPECT_EQ(r.denominator(), 1);

   r = rational{-16, 4};

   EXPECT_EQ(r.numerator(), -4);
   EXPECT_EQ(r.denominator(), 1);

   r = rational{16, -4};

   EXPECT_EQ(r.numerator(), -4);
   EXPECT_EQ(r.denominator(), 1);
}
















