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


#define TAG "metadata"


//
// https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#metadata
//


template <uint8_t versionNumber, typename tbt_file_t>
Status
parseMetadata(
    std::vector<uint8_t>::const_iterator &it,
    tbt_file_t &out) {

    if constexpr (0x70 <= versionNumber) {
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

    if constexpr (0x71 <= versionNumber) {

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].modulation = *it++;
        }

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].pitchBend = static_cast<int16_t>(parseLE2(it));
        }
    }

    if constexpr (0x6d <= versionNumber) {

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].transposeHalfSteps = static_cast<int8_t>(*it++);
        }

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].midiBank = *it++;
        }
    }

    if constexpr (0x6c <= versionNumber) {

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].reverb = *it++;
        }

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].chorus = *it++;
        }
    }

    if constexpr (0x6b <= versionNumber) {

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].pan = *it++;
        }

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.tracks[track].highestNote = *it++;
        }
    }

    if constexpr (0x6a <= versionNumber) {

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


    if constexpr (0x6b <= versionNumber) {

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

    if constexpr (0x6d <= versionNumber) {

        out.metadata.title = readPascal2String(it);

        out.metadata.artist = readPascal2String(it);

        out.metadata.album = readPascal2String(it);

        out.metadata.transcribedBy = readPascal2String(it);

        out.metadata.comment = readPascal2String(it);
    }

    //
    // if t.header.versionNumber < 0x6d, then title, artist, and comment still need to be read
    //

    return OK;
}


#undef TAG












