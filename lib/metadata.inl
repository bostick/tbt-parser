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


#define TAG "metadata"


//
// https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#metadata
//


template <uint8_t VERSION, typename tbt_file_t>
Status
parseMetadata(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator &end,
    tbt_file_t &out) {

    if constexpr (0x71 <= VERSION) {

        CHECK(static_cast<ptrdiff_t>(sizeof(tbt_track_metadata71) * out.header.trackCount) <= (end - it), "unhandled");

    } else if constexpr (0x70 <= VERSION) {

        CHECK(static_cast<ptrdiff_t>(sizeof(tbt_track_metadata70) * out.header.trackCount) <= (end - it), "unhandled");

    } else if constexpr (0x6e <= VERSION) {

        CHECK(static_cast<ptrdiff_t>(sizeof(tbt_track_metadata6e) * out.header.trackCount) <= (end - it), "unhandled");

    } else if constexpr (0x6b <= VERSION) {

        CHECK(static_cast<ptrdiff_t>(sizeof(tbt_track_metadata6b) * out.header.trackCount) <= (end - it), "unhandled");

    } else if constexpr (0x6a <= VERSION) {

        CHECK(static_cast<ptrdiff_t>(sizeof(tbt_track_metadata6a) * out.header.trackCount) <= (end - it), "unhandled");

    } else if constexpr (0x65 <= VERSION) {

        CHECK(static_cast<ptrdiff_t>(sizeof(tbt_track_metadata65) * out.header.trackCount) <= (end - it), "unhandled");

    } else {

        LOGE("invalid VERSION: 0x%02x", VERSION);
        ASSERT(false && "invalid VERSION");
    }

    if constexpr (0x70 <= VERSION) {
        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].spaceCount = parseLE4(it);
        }
    }

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.tracks[track].stringCount = *it++;
    }

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.tracks[track].cleanGuitar = *it++;
    }

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.tracks[track].mutedGuitar = *it++;
    }

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.tracks[track].volume = *it++;
    }

    if constexpr (0x71 <= VERSION) {

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].modulation = *it++;
        }

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].pitchBend = static_cast<int16_t>(parseLE2(it));
        }
    }

    if constexpr (0x6e <= VERSION) {

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].transposeHalfSteps = static_cast<int8_t>(*it++);
        }

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].midiBank = *it++;
        }

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].reverb = *it++;
        }

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].chorus = *it++;
        }
    }

    if constexpr (0x6b <= VERSION) {

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].pan = *it++;
        }

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].highestNote = *it++;
        }
    }

    if constexpr (0x6a <= VERSION) {

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].displayMIDINoteNumbers = *it++;
        }

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].midiChannel = static_cast<int8_t>(*it++);
        }
    }

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.tracks[track].topLineText = *it++;
    }

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.tracks[track].bottomLineText = *it++;
    }


    if constexpr (0x6b <= VERSION) {

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            for (uint8_t string = 0; string < 8; string++) {
                out.metadata.tracks[track].tuning[string] = static_cast<int8_t>(*it++);
            }
        }

    } else {

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            for (uint8_t string = 0; string < 6; string++) {
                out.metadata.tracks[track].tuning[string] = static_cast<int8_t>(*it++);
            }
        }
    }

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.tracks[track].drums = *it++;
    }

    return OK;
}


#undef TAG












