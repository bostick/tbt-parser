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

#include "tbt-parser/rational.h"

#include "common/assert.h"
#include "common/logging.h"

#include <utility>
#include <cmath>


#define TAG "rational"


rational::rational() : n(0), d(1) {}

rational::rational(int a) : n(a), d(1) {}

rational::rational(uint32_t a) : n(a), d(1) {}

rational::rational(int64_t a) : n(a), d(1) {}

rational::rational(const rational &a) : n(a.n), d(a.d) {}

rational::rational(int64_t nIn, int64_t dIn) : n(nIn), d(dIn) {
    simplify();
}


rational& rational::operator=(const rational &a) {

    n = a.n;

    d = a.d;

    return *this;
}

rational& rational::operator=(const rational &&a) {

    n = std::move(a.n);

    d = std::move(a.d);

    return *this;
}


int64_t rational::numerator() const {
    return n;
}

int64_t rational::denominator() const {
    return d;
}


void rational::simplify() {

    if (d < 0) {
        n *= -1;
        d *= -1;
    }

    if (d == 1) {
        
        return;
        
    } else if (d == 2) {
        
        if (n % 2 == 0) {
            
            n >>= 1;
            d = 1;
            
            return;
            
        } else {
            
            return;
        }
        
    } else if (d == 3) {
        
        if (n % 3 == 0) {
            
            n /= 3;
            d = 1;
            
            return;
            
        } else {
            
            return;
        }
    }

    //
    // general case
    //

    int64_t denom = gcd(n, d);
    n /= denom;
    d /= denom;
}


int64_t gcd(int64_t p, int64_t q) {
    
    if (q == 0) {
        return p;
    }

    return gcd(q, p % q);
}


rational rational::operator/(const rational x) const {

    if (x.n == 0) {
        LOGE("DIVIDE BY 0");
    }

    return { n * x.d, d * x.n };
}

rational rational::operator-(const rational x) const {

    if (d == x.d) {
        
        if (d == 1) {
            return { n - x.n };
        }
        
        return { n - x.n, d };
    }

    return { n * x.d - x.n * d, d * x.d };
}

rational rational::operator*(const rational x) const {
    return { n * x.n, d * x.d };
}

rational rational::operator+(const rational x) const {

    if (d == x.d) {

        if (d == 1) {
            return { n + x.n };
        }
        
        return { n + x.n, d };
    }

    return { n * x.d + x.n * d, d * x.d };
}

bool rational::operator==(const rational x) const {
    return n == x.n && d == x.d;
}

bool rational::operator>(const rational x) const {
    return n * x.d > d * x.n;
}

bool rational::operator<(const rational x) const {
    return n * x.d < d * x.n;
}


bool rational::is_nonnegative() const {
    return n >= 0;
}

bool rational::is_positive() const {
    return n > 0;
}


rational& rational::operator+=(const rational x) {

    if (x.n == 0) {
        return *this;
    }

    if (d == x.d) {

        if (d == 1) {

            n += x.n;

            return *this;
            
        } else if (d == 2) {
            
            n += x.n;

            if (n % 2 == 0) {
                
                n >>= 1;
                d = 1;
                
                return *this;
                
            } else {
                
                return *this;
            }
        }

        //
        // general case
        //

        n += x.n;

        simplify();

        return *this;
    }

    if (d == 1) {
        
        n *= x.d;
        n += x.n;

        d = x.d;

        simplify();

        return *this;
    }
    
    n *= x.d;
    n += x.n * d;

    d = d * x.d;

    simplify();

    return *this;
}

rational& rational::operator-=(const rational x) {

    if (x.n == 0) {
        return *this;
    }

    if (d == x.d) {

        if (d == 1) {

            n -= x.n;

            return *this;
        }

        n -= x.n;

        simplify();

        return *this;
    }

    n = n * x.d - x.n * d;

    d = d * x.d;

    simplify();

    return *this;
}

rational& rational::operator++() {

    if (d == 1) {

        n++;

        return *this;
    }

    n += d;

    simplify();

    return *this;
}

rational& rational::operator--() {

    if (d == 1) {

        n--;

        return *this;
    }

    n -= d;

    simplify();

    return *this;
}

double rational::to_double() const {
    return static_cast<double>(n) / static_cast<double>(d);
}

int16_t rational::to_int16() const {

    ASSERT(d == 1);

    ASSERT(n <= 0x7fff);

    return static_cast<int16_t>(n);
}

uint16_t rational::to_uint16() const {
    
    ASSERT(n >= 0);

    ASSERT(d == 1);

    ASSERT(n <= 0xffff);

    return static_cast<uint16_t>(n);
}

int32_t rational::to_int32() const {

    ASSERT(d == 1);

    ASSERT(n <= 0x7fffffff);

    return static_cast<int32_t>(n);
}

uint32_t rational::to_uint32() const {

    ASSERT(n >= 0);

    ASSERT(d == 1);

    ASSERT(n <= 0xffffffff);

    return static_cast<uint32_t>(n);
}

rational rational::floor() const {
    
    if (d == 1) {
        return *this;
    }

    return { n / d, 1 };
}


rational rational::round() const {
    
    if (d == 1) {

        return *this;

    } else if (d == 2) {

        //
        // we already know n is odd, since the denominator is 2
        //

        if (n % 4 == 1) {

            return { (n - 1) / 2, 1 };

        } else {

            //
            // n % 4 == 3
            //
            return { (n + 1) / 2, 1 };
        }
    }

    //
    // general case
    //

    return { static_cast<int64_t>(std::round(to_double())), 1 };
}








