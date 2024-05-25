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


#define TAG "bar-lines"


//
// https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#bar-lines
//


template <uint8_t VERSION, typename tbt_file_t>
Status
parseBarLinesMap(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    tbt_file_t &out) {

    if constexpr (0x70 <= VERSION) {
        
        auto begin = it;

        it += out.header.barCount * 6;

        CHECK(it <= end, "unhandled");

        std::vector<uint8_t> data(begin, it);

        std::vector<std::array<uint8_t, 6> > parts;

        Status ret = partitionInto<6>(data, parts);

        if (ret != OK) {
            return ret;
        }

        out.body.barLinesSpaceCount = 0;

        out.body.barLinesMap.clear();

        for (const std::array<uint8_t, 6> &part : parts) {

            auto space = out.body.barLinesSpaceCount;

            //
            // stored as 32-bit int, so must be cast
            //
            out.body.barLinesSpaceCount += static_cast<uint16_t>(parseLE4(part[0], part[1], part[2], part[3]));

            out.body.barLinesMap[space] = { part[4], part[5] };
        }

        return OK;

    } else {

        uint16_t barLinesSpaceCount;
        if constexpr (VERSION == 0x6f) {
            barLinesSpaceCount = out.header.spaceCount;
        } else {
            barLinesSpaceCount = 4000;
        }

        std::vector<uint8_t> barLinesDeltaListAcc;
        uint32_t sqCount = 0;

        while (true) {

            std::vector<uint8_t> deltaList;

            Status ret = parseDeltaListChunk(it, end, deltaList);

            if (ret != OK) {
                return ret;
            }

            barLinesDeltaListAcc.insert(barLinesDeltaListAcc.end(), deltaList.cbegin(), deltaList.cend());

            ret = computeDeltaListCount(deltaList, &sqCount);

            if (ret != OK) {
                return ret;
            }

            CHECK(sqCount <= barLinesSpaceCount, "unhandled");

            if (sqCount == barLinesSpaceCount) {
                break;
            }
        }

        Status ret = expandDeltaList<1>(barLinesDeltaListAcc, barLinesSpaceCount, 0, out.body.barLinesMap);

        if (ret != OK) {
            return ret;
        }

        return OK;
    }
}


#undef TAG







