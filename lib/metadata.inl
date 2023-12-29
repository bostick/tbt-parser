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
    tbt_file &out) {

    if (0x70 <= out.header.versionNumber) {

        out.metadata.spaceCountBlock = std::vector<uint32_t>(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.spaceCountBlock[track] = parseLE4(it);
        }
    }

    out.metadata.stringCountBlock = std::vector<uint8_t>(out.header.trackCount);

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.stringCountBlock[track] = *it++;
    }

    out.metadata.cleanGuitarBlock = std::vector<uint8_t>(out.header.trackCount);

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.cleanGuitarBlock[track] = *it++;
    }

    out.metadata.mutedGuitarBlock = std::vector<uint8_t>(out.header.trackCount);

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.mutedGuitarBlock[track] = *it++;
    }

    out.metadata.volumeBlock = std::vector<uint8_t>(out.header.trackCount);

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.volumeBlock[track] = *it++;
    }

    if (0x71 <= out.header.versionNumber) {

        out.metadata.modulationBlock = std::vector<uint8_t>(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.modulationBlock[track] = *it++;
        }

        out.metadata.pitchBendBlock = std::vector<int16_t>(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.pitchBendBlock[track] = static_cast<int16_t>(parseLE2(it));
        }
    }

    if (0x6d <= out.header.versionNumber) {

        out.metadata.transposeHalfStepsBlock = std::vector<int8_t>(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.transposeHalfStepsBlock[track] = static_cast<int8_t>(*it++);
        }

        out.metadata.midiBankBlock = std::vector<uint8_t>(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.midiBankBlock[track] = *it++;
        }
    }

    if (0x6c <= out.header.versionNumber) {

        out.metadata.reverbBlock = std::vector<uint8_t>(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.reverbBlock[track] = *it++;
        }

        out.metadata.chorusBlock = std::vector<uint8_t>(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.chorusBlock[track] = *it++;
        }
    }

    if (0x6b <= out.header.versionNumber) {

        out.metadata.panBlock = std::vector<uint8_t>(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.panBlock[track] = *it++;
        }

        out.metadata.highestNoteBlock = std::vector<uint8_t>(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.highestNoteBlock[track] = *it++;
        }
    }

    if (0x6a <= out.header.versionNumber) {

        out.metadata.displayMIDINoteNumbersBlock = std::vector<uint8_t>(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.displayMIDINoteNumbersBlock[track] = *it++;
        }

        out.metadata.midiChannelBlock = std::vector<int8_t>(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            out.metadata.midiChannelBlock[track] = static_cast<int8_t>(*it++);
        }
    }

    out.metadata.topLineTextBlock = std::vector<uint8_t>(out.header.trackCount);

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.topLineTextBlock[track] = *it++;
    }

    out.metadata.bottomLineTextBlock = std::vector<uint8_t>(out.header.trackCount);

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.bottomLineTextBlock[track] = *it++;
    }


    if (0x6b <= out.header.versionNumber) {

        out.metadata.tuningBlock = std::vector<std::array<int8_t, 8> >(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            for (uint8_t string = 0; string < 8; string++) {
                out.metadata.tuningBlock[track][string] = static_cast<int8_t>(*it++);
            }
        }

    } else {

        out.metadata.tuningBlockLE6a = std::vector<std::array<int8_t, 6> >(out.header.trackCount);

        for (uint8_t track = 0; track < out.header.trackCount; track++) {
            for (uint8_t string = 0; string < 6; string++) {
                out.metadata.tuningBlockLE6a[track][string] = static_cast<int8_t>(*it++);
            }
        }
    }

    out.metadata.drumsBlock = std::vector<uint8_t>(out.header.trackCount);

    for (uint8_t track = 0; track < out.header.trackCount; track++) {
        out.metadata.drumsBlock[track] = *it++;
    }

    if (0x6d <= out.header.versionNumber) {

        out.metadata.title = readPascal2String(it);

        out.metadata.artist = readPascal2String(it);

        out.metadata.album = readPascal2String(it);

        out.metadata.transcribedBy = readPascal2String(it);

        out.metadata.comment = readPascal2String(it);

    } else {

        out.metadata.title = readPascal2String(it);

        out.metadata.artist = readPascal2String(it);

        out.metadata.comment = readPascal2String(it);
    }

    return OK;
}


#undef TAG












