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


#define TAG "alternate-time-regions"


//
// https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#alternate-time-regions
//


template <uint8_t VERSION, typename tbt_file_t>
Status
parseAlternateTimeRegionsMapList(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    tbt_file_t &out) {

    for (uint8_t track = 0; track < out.header.trackCount; track++) {

        auto trackSpaceCount = out.metadata.tracks[track].spaceCount;

        std::vector<uint8_t> alternateTimeRegionsDeltaListAcc;
        uint32_t dsqCount = 0;

        while (true) {

            std::vector<uint8_t> deltaList;

            Status ret = parseDeltaListChunk(it, end, deltaList);

            if (ret != OK) {
                return ret;
            }

            alternateTimeRegionsDeltaListAcc.insert(
                alternateTimeRegionsDeltaListAcc.end(),
                deltaList.cbegin(),
                deltaList.cend());

            ret = computeDeltaListCount(deltaList, &dsqCount);

            if (ret != OK) {
                return ret;
            }

            CHECK(dsqCount <= 2 * trackSpaceCount, "unhandled");

            if (dsqCount == 2 * trackSpaceCount) {
                break;
            }
        }

        auto &alternateTimeRegionsMap = out.body.mapsList[track].alternateTimeRegionsMap;

        Status ret = expandDeltaList<2>(
            alternateTimeRegionsDeltaListAcc,
            dsqCount,
            1,
            alternateTimeRegionsMap);

        if (ret != OK) {
            return ret;
        }

#ifndef NDEBUG
        rational alternateTimeRegionsCorrection = 0;
        for (uint32_t space = 0; space < trackSpaceCount; space++) {

            const auto &alternateTimeRegionsIt = alternateTimeRegionsMap.find(space);
            if (alternateTimeRegionsIt != alternateTimeRegionsMap.end()) {

                const auto &alternateTimeRegion = alternateTimeRegionsIt->second;

                auto atr = rational{1} - rational{alternateTimeRegion[0], alternateTimeRegion[1]};

                alternateTimeRegionsCorrection += atr;
            }
        }

        ASSERT(rational(out.metadata.tracks[track].spaceCount) == rational(out.body.barsSpaceCount) + alternateTimeRegionsCorrection);
#endif // NDEBUG
    }

    return OK;
}


#undef TAG














