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

#include "tbt-parser/bars.h"

#include "tbt-parser/tbt-parser-util.h"

#include <algorithm>


#define TAG "bars"


//
// https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#bars
//


Status
parseBarsMapGE70(
    std::vector<uint8_t>::const_iterator &it,
    const tbt_file &t,
    std::unordered_map<uint32_t, std::array<uint8_t, 2> > &barsMap,
    uint32_t *barsSpaceCount_p) {

    std::vector<uint8_t> data(it, it + t.header.barCountGE70 * 6);
    it += t.header.barCountGE70 * 6;

    std::vector<std::array<uint8_t, 6> > parts;

    Status ret = partitionInto<6>(data, parts);

    if (ret != OK) {
        return ret;
    }

    *barsSpaceCount_p = 0;

    barsMap.clear();

    for (const std::array<uint8_t, 6> &part : parts) {

        uint32_t space = *barsSpaceCount_p;

        *barsSpaceCount_p += parseLE4(part.data());

        barsMap[space] = { part[4], part[5] };
    }

    return OK;
}


Status
parseBarsMap(
    std::vector<uint8_t>::const_iterator &it,
    const tbt_file &t,
    std::unordered_map<uint32_t, std::array<uint8_t, 1> > &barsMap) {

    uint32_t barsSpaceCount;
    if (t.header.versionNumber == 0x6f) {
        barsSpaceCount = t.header.spaceCount6f;
    } else {
        barsSpaceCount = 4000;
    }

    std::vector<uint8_t> barsDeltaList = parseDeltaListChunk(it);

    Status ret = expandDeltaList<1>(barsDeltaList, barsSpaceCount, 0, barsMap);

    if (ret != OK) {
        return ret;
    }

    return OK;
}









