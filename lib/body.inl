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


#define TAG "body"


//
// https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#body
//


template <uint8_t VERSION, bool HASALTERNATETIMEREGIONS, typename tbt_file_t>
Status
parseBody(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    tbt_file_t &out) {

    //
    // parse bars
    //

    Status ret;

    ret = parseBarsMap<VERSION, tbt_file_t>(it, end, out);

    if (ret != OK) {
        return ret;
    }

    //
    // parse notes
    //

    if constexpr (0x6b <= VERSION) {

        ret = parseNotesMapList<VERSION, tbt_file_t, 8>(it, end, out);

    } else {

        ret = parseNotesMapList<VERSION, tbt_file_t, 6>(it, end, out);
    }

    if (ret != OK) {
        return ret;
    }

    //
    // parse alternate time regions
    //

    if constexpr (HASALTERNATETIMEREGIONS) {

        ret = parseAlternateTimeRegionsMapList<VERSION, tbt_file_t>(it, end, out);

        if (ret != OK) {
            return ret;
        }
    }

    //
    // parse track effect changes
    //

    if constexpr (0x71 <= VERSION) {

        ret = parseTrackEffectChangesMapList<VERSION, tbt_file_t>(it, end, out);

        if (ret != OK) {
            return ret;
        }
    }

    return OK;
}


#undef TAG









