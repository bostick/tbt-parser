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

#include "tbt-parser.h"

#include <string>


uint16_t parseLE2(std::vector<uint8_t>::const_iterator &it);
uint16_t parseLE2(const uint8_t *data);
uint16_t parseLE2(uint8_t b0, uint8_t b1);

uint32_t parseLE4(std::vector<uint8_t>::const_iterator &it);
uint32_t parseLE4(const uint8_t *data);
uint32_t parseLE4(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);


Status
readPascal2String(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    std::vector<char> &out);

Status
parseDeltaListChunk(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    std::vector<uint8_t> &out);

Status
parseChunk4(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    std::vector<uint8_t> &out);

uint32_t crc32_checksum(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end);

Status zlib_inflate(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    std::vector<uint8_t> &acc);

Status computeDeltaListCount(const std::vector<uint8_t> &deltaList, uint32_t *acc);

std::array<uint8_t, 2> toDigitsBE(uint16_t value);
std::array<uint8_t, 4> toDigitsBE(uint32_t value);

std::string fromPascal1String(const char *data);

std::string fromPascal2String(const char *data);











