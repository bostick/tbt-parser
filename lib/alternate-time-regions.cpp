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

#include "tbt-parser/alternate-time-regions.h"

#include "tbt-parser/tbt-parser-util.h"

//#include <cmath> // for round


#define TAG "alternate-time-regions"


//
// https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#alternate-time-regions
//


Status
parseAlternateTimeRegionsMapList(
    std::vector<uint8_t>::const_iterator &it,
    const tbt_file &t,
    std::vector<std::unordered_map<uint32_t, std::array<uint8_t, 2> > > &alternateTimeRegionsMapList) {

    alternateTimeRegionsMapList.clear();
    alternateTimeRegionsMapList.reserve(t.header.trackCount);

    for (uint8_t track = 0; track < t.header.trackCount; track++) {

        uint32_t trackSpaceCount = t.metadata.spaceCountBlock[track];

        std::vector<uint8_t> alternateTimeRegionsDeltaListAcc;
        uint32_t dsqCount = 0;

        while (true) {

            std::vector<uint8_t> deltaList = parseChunk2(it);

            alternateTimeRegionsDeltaListAcc.insert(
                    alternateTimeRegionsDeltaListAcc.end(),
                    deltaList.cbegin(),
                    deltaList.cend());

            Status ret = computeDeltaListCount(deltaList, &dsqCount);

            if (ret != OK) {
                return ret;
            }

            ASSERT(dsqCount <= 2 * trackSpaceCount);

            if (dsqCount == 2 * trackSpaceCount) {
                break;
            }
        }

        std::unordered_map<uint32_t, std::array<uint8_t, 2> > alternateTimeRegionsMap;

        Status ret = expandDeltaList<2>(
                alternateTimeRegionsDeltaListAcc,
                dsqCount,
                1,
                alternateTimeRegionsMap);

        if (ret != OK) {
            return ret;
        }

//        double alternateTimeRegionsCorrection = 0.0;
//        for (const auto &pair : alternateTimeRegionsMap) {
//
//            const auto &region = pair.second;
//
//            alternateTimeRegionsCorrection += (1.0 * region[0] / region[1]) - 1.0;
//        }
//
//        alternateTimeRegionsCorrection = round(alternateTimeRegionsCorrection);
//
//        if (t.metadata.spaceCountBlock[track] + (int32_t static_cast) alternateTimeRegionsCorrection != t.body.barsSpaceCountGE70) {
//            LOGE("alternateTimeRegionsCorrection: trackSpaceCount + correction: %d, barSpaceCount: %d", t.metadata.spaceCountBlock[track] + (int32_t) alternateTimeRegionsCorrection, t.body.barsSpaceCountGE70);
//            return TBT_ERR;
//        }

        alternateTimeRegionsMapList.push_back(alternateTimeRegionsMap);
    }

    return OK;
}
















