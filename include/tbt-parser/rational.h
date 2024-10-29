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

#include <cstdint>


class rational {
private:

	int64_t n;
	int64_t d;

	void simplify();

public:
	
	rational();
	rational(int a);
	rational(uint32_t a);
	rational(int64_t a);

	rational(const rational &a);
	rational(int64_t nIn, int64_t dIn);

	rational& operator=(const rational &a);
	rational& operator=(rational &&a);

	int64_t numerator() const;
	int64_t denominator() const;

	rational operator/(const rational x) const;
	rational operator-(const rational x) const;
	rational operator*(const rational x) const;
	rational operator+(const rational x) const;

	bool operator==(const rational x) const;
	bool operator>(const rational x) const;
	bool operator<(const rational x) const;

	bool is_nonnegative() const;
	bool is_positive() const;
	
	rational& operator+=(const rational x);

	rational& operator-=(const rational x);
	
	rational& operator++();
	rational& operator--();

	double to_double() const;
	int16_t to_int16() const;
	uint16_t to_uint16() const;
	int32_t to_int32() const;
	uint32_t to_uint32() const;

	rational floor() const;
	rational round() const;
};
