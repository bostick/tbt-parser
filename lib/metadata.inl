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


Status
parseMetadata(
    std::vector<uint8_t>::const_iterator &it,
    const tbt_file &t,
    tbt_metadata *out) {

    if (0x70 <= t.header.versionNumber) {

        out->spaceCountBlock = std::vector<uint32_t>(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            out->spaceCountBlock[track] = parseLE4(it);
        }
    }

    out->stringCountBlock = std::vector<uint8_t>(t.header.trackCount);

    for (uint8_t track = 0; track < t.header.trackCount; track++) {
        out->stringCountBlock[track] = *it++;
    }

    out->cleanGuitarBlock = std::vector<uint8_t>(t.header.trackCount);

    for (uint8_t track = 0; track < t.header.trackCount; track++) {
        out->cleanGuitarBlock[track] = *it++;
    }

    out->mutedGuitarBlock = std::vector<uint8_t>(t.header.trackCount);

    for (uint8_t track = 0; track < t.header.trackCount; track++) {
        out->mutedGuitarBlock[track] = *it++;
    }

    out->volumeBlock = std::vector<uint8_t>(t.header.trackCount);

    for (uint8_t track = 0; track < t.header.trackCount; track++) {
        out->volumeBlock[track] = *it++;
    }

    if (0x71 <= t.header.versionNumber) {

        out->modulationBlock = std::vector<uint8_t>(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            out->modulationBlock[track] = *it++;
        }

        out->pitchBendBlock = std::vector<int16_t>(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            out->pitchBendBlock[track] = static_cast<int16_t>(parseLE2(it));
        }
    }

    if (0x6d <= t.header.versionNumber) {

        out->transposeHalfStepsBlock = std::vector<int8_t>(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            out->transposeHalfStepsBlock[track] = static_cast<int8_t>(*it++);
        }

        out->midiBankBlock = std::vector<uint8_t>(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            out->midiBankBlock[track] = *it++;
        }
    }

    if (0x6c <= t.header.versionNumber) {

        out->reverbBlock = std::vector<uint8_t>(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            out->reverbBlock[track] = *it++;
        }

        out->chorusBlock = std::vector<uint8_t>(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            out->chorusBlock[track] = *it++;
        }
    }

    if (0x6b <= t.header.versionNumber) {

        out->panBlock = std::vector<uint8_t>(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            out->panBlock[track] = *it++;
        }

        out->highestNoteBlock = std::vector<uint8_t>(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            out->highestNoteBlock[track] = *it++;
        }
    }

    if (0x6a <= t.header.versionNumber) {

        out->displayMIDINoteNumbersBlock = std::vector<uint8_t>(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            out->displayMIDINoteNumbersBlock[track] = *it++;
        }

        out->midiChannelBlock = std::vector<int8_t>(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            out->midiChannelBlock[track] = static_cast<int8_t>(*it++);
        }
    }

    out->topLineTextBlock = std::vector<uint8_t>(t.header.trackCount);

    for (uint8_t track = 0; track < t.header.trackCount; track++) {
        out->topLineTextBlock[track] = *it++;
    }

    out->bottomLineTextBlock = std::vector<uint8_t>(t.header.trackCount);

    for (uint8_t track = 0; track < t.header.trackCount; track++) {
        out->bottomLineTextBlock[track] = *it++;
    }


    if (0x6b <= t.header.versionNumber) {

        out->tuningBlock = std::vector<std::array<int8_t, 8> >(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            for (uint8_t string = 0; string < 8; string++) {
                out->tuningBlock[track][string] = static_cast<int8_t>(*it++);
            }
        }

    } else {

        out->tuningBlockLE6a = std::vector<std::array<int8_t, 6> >(t.header.trackCount);

        for (uint8_t track = 0; track < t.header.trackCount; track++) {
            for (uint8_t string = 0; string < 6; string++) {
                out->tuningBlockLE6a[track][string] = static_cast<int8_t>(*it++);
            }
        }
    }

    out->drumsBlock = std::vector<uint8_t>(t.header.trackCount);

    for (uint8_t track = 0; track < t.header.trackCount; track++) {
        out->drumsBlock[track] = *it++;
    }

    if (0x6d <= t.header.versionNumber) {

        out->title = readPascal2String(it);

        out->artist = readPascal2String(it);

        out->album = readPascal2String(it);

        out->transcribedBy = readPascal2String(it);

        out->comment = readPascal2String(it);

    } else {

        out->title = readPascal2String(it);

        out->artist = readPascal2String(it);

        out->comment = readPascal2String(it);
    }

    return OK;
}


#undef TAG












