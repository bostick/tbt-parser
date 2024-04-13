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


#define TAG "track-effect-changes"


//
// https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#track-effect-changes
//


template <uint8_t VERSION, typename tbt_file_t>
Status
parseTrackEffectChangesMapList(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    tbt_file_t &out) {

    out.body.trackEffectChangesMapList.clear();
    out.body.trackEffectChangesMapList.reserve(out.header.trackCount);

    for (uint8_t track = 0; track < out.header.trackCount; track++) {

        std::vector<uint8_t> arrayList;

        Status ret = parseChunk4(it, end, arrayList);

        if (ret != OK) {
            return ret;
        }

        std::map<uint32_t, std::vector<tbt_track_effect_change> > trackEffectChangesMap;

        std::vector<std::array<uint8_t, 8> > parts;

        ret = partitionInto<8>(arrayList, parts);

        if (ret != OK) {
            return ret;
        }

        uint32_t space = 0;

        for (const auto &part : parts) {

            uint16_t s = parseLE2(part[0], part[1]);
            uint16_t e = parseLE2(part[2], part[3]);
            uint16_t r = parseLE2(part[4], part[5]);
            uint16_t v = parseLE2(part[6], part[7]);

            CHECK(r == 0x02, "unhandled");

            space += s;

            std::vector<tbt_track_effect_change> &changes = trackEffectChangesMap[space];

            tbt_track_effect_change change{ static_cast<tbt_track_effect>(e), v };

            changes.push_back(change);
        }

        out.body.trackEffectChangesMapList.push_back(trackEffectChangesMap);
    }

    return OK;
}


#undef TAG






