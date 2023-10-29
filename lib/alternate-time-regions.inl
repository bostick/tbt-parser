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


#define TAG "alternate-time-regions"


//
// https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#alternate-time-regions
//


template <uint8_t VERSION, typename tbt_file_t>
Status
parseAlternateTimeRegionsMapList(
    std::vector<uint8_t>::const_iterator &it,
    tbt_file_t &out) {

    out.body.alternateTimeRegionsMapList.clear();
    out.body.alternateTimeRegionsMapList.reserve(out.header.trackCount);

    for (uint8_t track = 0; track < out.header.trackCount; track++) {

        uint32_t trackSpaceCount = out.metadata.tracks[track].spaceCount;

        std::vector<uint8_t> alternateTimeRegionsDeltaListAcc;
        uint32_t dsqCount = 0;

        while (true) {

            std::vector<uint8_t> deltaList = parseDeltaListChunk(it);

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

        std::map<uint32_t, std::array<uint8_t, 2> > alternateTimeRegionsMap;

        Status ret = expandDeltaList<2>(
            alternateTimeRegionsDeltaListAcc,
            dsqCount,
            1,
            alternateTimeRegionsMap);

        if (ret != OK) {
            return ret;
        }

        double alternateTimeRegionsCorrection = 0.0;
        for (uint32_t space = 0; space < trackSpaceCount; space++) {

            //
            // The denominator of the Alternate Time Region for this space. For example, for triplets, this is 2.
            //
            uint8_t denominator = 1;

            //
            // The numerator of the Alternate Time Region for this space. For example, for triplets, this is 3.
            //
            uint8_t numerator = 1;

            const auto &alternateTimeRegionsIt = alternateTimeRegionsMap.find(space);
            if (alternateTimeRegionsIt != alternateTimeRegionsMap.end()) {

                const auto &alternateTimeRegion = alternateTimeRegionsIt->second;

                denominator = alternateTimeRegion[0];

                numerator = alternateTimeRegion[1];
            }

            double actualSpaceInc = (static_cast<double>(denominator) / static_cast<double>(numerator));

            alternateTimeRegionsCorrection += (1.0 - actualSpaceInc);
        }

        alternateTimeRegionsCorrection = round(alternateTimeRegionsCorrection);

        ASSERT(out.metadata.tracks[track].spaceCount == out.body.barsSpaceCount + static_cast<uint32_t>(alternateTimeRegionsCorrection));

        out.body.alternateTimeRegionsMapList.push_back(alternateTimeRegionsMap);
    }

    return OK;
}


#undef TAG














