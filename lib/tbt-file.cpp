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

#define _CRT_SECURE_NO_DEPRECATE // disable warnings about fopen being insecure on MSVC

#include "tbt-parser.h"

#include "tbt-parser/metadata.h"
#include "tbt-parser/midi.h"
#include "tbt-parser/tbt-parser-util.h"

#include "common/assert.h"
#include "common/logging.h"

#include <cinttypes>
#include <cstring>
#include <cmath> // for round in alternate-time-regions.inl, in body.inl


#include "splitat.inl"
#include "partitioninto.inl"
#include "expanddeltalist.inl"
#include "alternate-time-regions.inl"
#include "bars.inl"
#include "track-effect-changes.inl"
#include "notes.inl"
#include "body.inl"


#define TAG "tbt-file"


Status
parseTbtFile(
    const char *path,
    tbt_file *out) {
    
    FILE *file = fopen(path, "rb");

    if (!file) {

        LOGE("cannot open %s\n", path);

        return ERR;
    }

    if (fseek(file, 0, SEEK_END)) {

        LOGE("fseek failed");

        return ERR;
    }

    long res = ftell(file);

    if (res < 0) {

        LOGE("ftell failed");

        return ERR;
    }

    size_t len = static_cast<size_t>(res);

    rewind(file);

    auto buf = new uint8_t[len];

    size_t r = fread(buf, sizeof(uint8_t), len, file);

    if (r != len) {

        LOGE("fread failed");

        return ERR;
    }

    fclose(file);

    return parseTbtBytes(buf, len, out);
}


Status
parseTbtBytes(
    const uint8_t *data,
    size_t len,
    tbt_file *out) {

    //
    // https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#header
    //
    
    //
    // parse header
    //

    const uint8_t *headerToParse = data + 0;

    memcpy(&out->header, headerToParse, HEADER_SIZE);

    if (memcmp(out->header.magic.data(), "TBT", 3) != 0) {

        LOGE("parseTBT: magic does not match");

        return ERR;
    }

    if (!(out->header.versionString[0] == 3 || out->header.versionString[0] == 4)) {

        LOGE("parseTBT: versionString is weird");

        return ERR;
    }

    if ((out->header.featureBitfield & 0b00001011) != 0b00001011) {

        LOGE("parseTBT: featureBitfield is weird");

        return ERR;
    }

    for (uint8_t &u : out->header.unused) {

        if (u != 0) {

            LOGE("parseTBT: unused is not all 0");

            return ERR;
        }
    }

    if (0x72 < out->header.versionNumber) {

        LOGE("parseTBT: This file was created with a later version of TabIt. You'll need to upgrade to TabIt version %s or later in order to open this file.", out->header.versionString.data() + 1);

        return ERR;
    }

    if (0x70 <= out->header.versionNumber) {

        if (32000 < out->header.barCountGE70) {

            LOGE("parseTBT: Unable to load the file because it contains more bars than this version of TabIt supports");

            return ERR;
        }
    }

    if (0x67 < out->header.versionNumber) {

        auto restToCheck = std::vector<uint8_t>(data + HEADER_SIZE, data + len);

        auto crc32Rest = crc32_checksum(restToCheck);

        if (crc32Rest != out->header.crc32Rest) {

            LOGE("parseTBT: bad crc32Rest. expected: %" PRIu32 " actual: %" PRIu32,  out->header.crc32Rest, crc32Rest);

            return ERR;
        }

        if (len != out->header.totalByteCount) {

            LOGE("parseTBT: bad byte count. expected: %" PRIu32 " actual: %zu", out->header.totalByteCount, len);

            return ERR;
        }

        auto headerToCheck = std::vector<uint8_t>(data + 0, data + HEADER_SIZE - 4);

        auto crc32Header = crc32_checksum(headerToCheck);

        if (crc32Header != out->header.crc32Header) {

            LOGE("parseTBT: bad crc32Header. expected: %" PRIu32 " actual: %" PRIu32, out->header.crc32Header, crc32Header);

            return ERR;
        }
    }

    if (15 < out->header.trackCount) {

        LOGE("parseTBT: Unable to load the file because it contains more tracks than this version of TabIt supports");

        return ERR;
    }

    if (out->header.tempo1 != out->header.tempo2) {
        ASSERT(out->header.tempo1 == 250);
    }

    //
    // parse metadata
    //
    {
        auto metadataToInflate = std::vector<uint8_t>(
                data + HEADER_SIZE,
                data + HEADER_SIZE + out->header.metadataLen);

        std::vector<uint8_t> metadataToParse;

        Status ret = zlib_inflate(metadataToInflate, metadataToParse);

        if (ret != OK) {
            return ret;
        }

        auto it = metadataToParse.cbegin();

        ret = parseMetadata(it, *out, &out->metadata);

        if (ret != OK) {
            return ret;
        }

        ASSERT(it == metadataToParse.end());
    }

    //
    // parse body
    //
    {
        auto bodyToInflate = std::vector<uint8_t>(
                data + HEADER_SIZE + out->header.metadataLen,
                data + len);

        std::vector<uint8_t> bodyToParse;

        Status ret = zlib_inflate(bodyToInflate, bodyToParse);

        if (ret != OK) {
            return ret;
        }

        auto it = bodyToParse.cbegin();

        ret = parseBody(it, *out, &out->body);

        if (ret != OK) {
            return ret;
        }

        ASSERT(it == bodyToParse.end());
    }
      
    return OK;
}
















