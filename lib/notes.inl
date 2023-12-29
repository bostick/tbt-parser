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


#define TAG "notes"


//
// https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#notes
//


Status
parseNotesMapList(
    std::vector<uint8_t>::const_iterator &it,
    tbt_file &out) {

    out.body.notesMapList.clear();
    out.body.notesMapList.reserve(out.header.trackCount);

    for (uint8_t track = 0; track < out.header.trackCount; track++) {

        uint32_t trackSpaceCount;
        if (0x70 <= out.header.versionNumber) {
            trackSpaceCount = out.metadata.tracks[track].spaceCount;
        } else if (out.header.versionNumber == 0x6f) {
            trackSpaceCount = out.header.spaceCount6f;
        } else {
            trackSpaceCount = 4000;
        }

        std::vector<uint8_t> notesDeltaListAcc;
        uint32_t vsqCount = 0;

        while (true) {

            auto deltaList = parseDeltaListChunk(it);

            notesDeltaListAcc.insert(notesDeltaListAcc.end(), deltaList.cbegin(), deltaList.cend());

            Status ret = computeDeltaListCount(deltaList, &vsqCount);

            if (ret != OK) {
                return ret;
            }

            ASSERT(vsqCount <= 20 * trackSpaceCount);

            if (vsqCount == 20 * trackSpaceCount) {
                break;
            }
        }

        std::unordered_map<uint32_t, std::array<uint8_t, 20> > notesMap;

        Status ret = expandDeltaList<20>(notesDeltaListAcc, vsqCount, 0, notesMap);

        if (ret != OK) {
            return ret;
        }

        out.body.notesMapList.push_back(notesMap);
    }

    return OK;
}


#undef TAG








