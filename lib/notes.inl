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


#define TAG "notes"


//
// https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#notes
//


template <uint8_t VERSION, typename tbt_file_t, size_t STRINGS_PER_TRACK>
Status
parseNotesMapList(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator &end,
    tbt_file_t &out) {

    for (uint8_t track = 0; track < out.header.trackCount; track++) {

        uint16_t trackSpaceCount;
        if constexpr (0x70 <= VERSION) {
            //
            // stored as 32-bit int, so must be cast
            //
            trackSpaceCount = static_cast<uint16_t>(out.metadata.tracks[track].spaceCount);
        } else if constexpr (VERSION == 0x6f) {
            trackSpaceCount = out.header.spaceCount;
        } else {
            trackSpaceCount = 4000;
        }

        std::vector<uint8_t> notesDeltaListAcc;
        uint32_t vsqCount = 0;

        while (true) {

            std::vector<uint8_t> deltaList;

            Status ret = parseDeltaListChunk(it, end, deltaList);

            if (ret != OK) {
                return ret;
            }

            notesDeltaListAcc.insert(notesDeltaListAcc.end(), deltaList.cbegin(), deltaList.cend());

            ret = computeDeltaListCount(deltaList, &vsqCount);

            if (ret != OK) {
                return ret;
            }

            CHECK(vsqCount <= (STRINGS_PER_TRACK + STRINGS_PER_TRACK + 4) * trackSpaceCount, "unhandled");

            if (vsqCount == (STRINGS_PER_TRACK + STRINGS_PER_TRACK + 4) * trackSpaceCount) {
                break;
            }
        }

        Status ret = expandDeltaList<STRINGS_PER_TRACK + STRINGS_PER_TRACK + 4>(
            notesDeltaListAcc,
            vsqCount,
            0,
            out.body.mapsList[track].notesMap
        );

        if (ret != OK) {
            return ret;
        }
    }

    return OK;
}


#undef TAG








