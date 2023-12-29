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
#include "metadata.inl"
#include "alternate-time-regions.inl"
#include "bars.inl"
#include "track-effect-changes.inl"
#include "notes.inl"
#include "body.inl"


#define TAG "tbt-file"


template <uint8_t VERSION, typename tbt_file_t>
Status
parseTbtBytes(
    const uint8_t *data,
    size_t len,
    tbt_file_t &out) {

    //
    // https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#header
    //
    
    //
    // parse header
    //

    const uint8_t *headerToParse = data + 0;

    memcpy(&out.header, headerToParse, HEADER_SIZE);

    if (memcmp(out.header.magic.data(), "TBT", 3) != 0) {

        LOGE("parseTBT: magic does not match");

        return ERR;
    }

    if (!(out.header.versionString[0] == 3 || out.header.versionString[0] == 4)) {

        LOGE("parseTBT: versionString is weird");

        return ERR;
    }

    if constexpr (0x70 <= VERSION) {

        ASSERT(out.header.barCount != 0);

    } else {

        ASSERT(out.header.barCount_unused == 0);
    }

    if constexpr (VERSION == 0x6f) {

        ASSERT(out.header.spaceCount != 0);

    } else {

        ASSERT(out.header.spaceCount_unused == 0);
    }

    if constexpr (0x6e <= VERSION && VERSION <= 0x6f) {

        //
        // Nothing to assert: lastNonEmptySpace may be 0
        //
        ;

    } else {
        
        ASSERT(out.header.lastNonEmptySpace_unused == 0);
    }

    if constexpr (0x6e <= VERSION) {

        ASSERT(out.header.tempo2 != 0);

        if (250 <= out.header.tempo2) {
            ASSERT(out.header.tempo1 == 250);
        } else {
            ASSERT(out.header.tempo1 == out.header.tempo2);
        }

    } else {

        ASSERT(out.header.tempo2_unused == 0);
    }

    if constexpr (0x72 < VERSION) {

        LOGE("parseTBT: This file was created with a later version of TabIt. You'll need to upgrade to TabIt version %s or later in order to open this file.", out.header.versionString.data() + 1);

        return ERR;
    }

    if constexpr (0x70 <= VERSION) {

        if (32000 < out.header.barCount) {

            LOGE("parseTBT: Unable to load the file because it contains more bars than this version of TabIt supports");

            return ERR;
        }
    }

    if constexpr (0x68 <= VERSION) {

        auto restToCheck = std::vector<uint8_t>(data + HEADER_SIZE, data + len);
        
        if (len != out.header.totalByteCount) {

            LOGE("parseTBT: bad byte count. expected: %" PRIu32 " actual: %zu", out.header.totalByteCount, len);

            return ERR;
        }
        
        auto crc32Rest = crc32_checksum(restToCheck);
        
        if (crc32Rest != out.header.crc32Rest) {

            LOGE("parseTBT: bad crc32Rest. expected: %" PRIu32 " actual: %" PRIu32,  out.header.crc32Rest, crc32Rest);

            return ERR;
        }

        auto headerToCheck = std::vector<uint8_t>(data + 0, data + HEADER_SIZE - 4);

        auto crc32Header = crc32_checksum(headerToCheck);

        if (crc32Header != out.header.crc32Header) {

            LOGE("parseTBT: bad crc32Header. expected: %" PRIu32 " actual: %" PRIu32, out.header.crc32Header, crc32Header);

            return ERR;
        }

    } else {

        ASSERT(out.header.metadataLen_unused == 0);
        ASSERT(out.header.crc32Rest_unused == 0);
        ASSERT(out.header.totalByteCount_unused == 0);
        ASSERT(out.header.crc32Header_unused == 0);
    }

    if (15 < out.header.trackCount) {

        LOGE("parseTBT: Unable to load the file because it contains more tracks than this version of TabIt supports");

        return ERR;
    }
    
    const uint8_t *it2;

    //
    // parse metadata
    //
    {
        // uint32_t metadataLen;

        if constexpr (0x6e <= VERSION) {

            uint32_t metadataLen;

            if constexpr (0x71 <= VERSION) {

                metadataLen = sizeof(tbt_track_metadata71) * out.header.trackCount;

                out.metadata.tracks = std::vector<tbt_track_metadata71>(out.header.trackCount);

            } else if constexpr (0x70 <= VERSION) {

                metadataLen = sizeof(tbt_track_metadata70) * out.header.trackCount;

                out.metadata.tracks = std::vector<tbt_track_metadata70>(out.header.trackCount);

            } else {

                metadataLen = sizeof(tbt_track_metadata6e) * out.header.trackCount;

                out.metadata.tracks = std::vector<tbt_track_metadata6e>(out.header.trackCount);
            }

            auto metadataToInflate = std::vector<uint8_t>(
                data + HEADER_SIZE,
                data + HEADER_SIZE + out.header.compressedMetadataLen);

            std::vector<uint8_t> metadataInflated;

            Status ret;

            ret = zlib_inflate(metadataToInflate, metadataInflated);

            if (ret != OK) {
                return ret;
            }

            auto metadataToParse = metadataInflated;

            auto it = metadataToParse.cbegin();

            ret = parseMetadata<VERSION, tbt_file_t>(it, out);

            if (ret != OK) {
                return ret;
            }

            ASSERT(it == metadataToParse.cbegin() + metadataLen);

            //
            // now read the remaining title, artist, album, transcribedBy, and comment
            //

            out.metadata.title = readPascal2String(it);
            
            out.metadata.artist = readPascal2String(it);
            
            out.metadata.album = readPascal2String(it);
            
            out.metadata.transcribedBy = readPascal2String(it);
            
            out.metadata.comment = readPascal2String(it);

            ASSERT(it == metadataToParse.cend());

        } else {

            uint32_t metadataLen;

            if constexpr (VERSION == 0x6b) {

                metadataLen = sizeof(tbt_track_metadata6b) * out.header.trackCount;

                out.metadata.tracks = std::vector<tbt_track_metadata6b>(out.header.trackCount);

            } else if constexpr (VERSION == 0x6a) {

                metadataLen = sizeof(tbt_track_metadata6a) * out.header.trackCount;

                out.metadata.tracks = std::vector<tbt_track_metadata6a>(out.header.trackCount);

            } else {

                metadataLen = sizeof(tbt_track_metadata65) * out.header.trackCount;

                out.metadata.tracks = std::vector<tbt_track_metadata65>(out.header.trackCount);
            }

            auto metadataToParse = std::vector<uint8_t>(
                data + HEADER_SIZE,
                data + HEADER_SIZE + metadataLen);

            auto it = metadataToParse.cbegin();

            Status ret = parseMetadata<VERSION, tbt_file_t>(it, out);

            if (ret != OK) {
                return ret;
            }

            ASSERT(it == metadataToParse.cend());

            //
            // now read the remaining title, artist, and comment
            //
            
            it2 = data + HEADER_SIZE + metadataLen;
            
            auto strLen = *it2;
            
            out.metadata.title = std::vector<uint8_t>(it2, it2 + 1 + strLen);
            it2 += 1;
            it2 += strLen;
            
            strLen = *it2;
            
            out.metadata.artist = std::vector<uint8_t>(it2, it2 + 1 + strLen);
            it2 += 1;
            it2 += strLen;
            
            strLen = *it2;
            
            out.metadata.comment = std::vector<uint8_t>(it2, it2 + 1 + strLen);
            it2 += 1;
            it2 += strLen;

            //
            // Nothing to assert
            //
        }
    }

    //
    // parse body
    //
    {
        std::vector<uint8_t> bodyToParse;

        if constexpr (0x6e <= VERSION) {

            auto bodyToInflate = std::vector<uint8_t>(
                data + HEADER_SIZE + out.header.compressedMetadataLen,
                data + len);

            auto ret = zlib_inflate(bodyToInflate, bodyToParse);

            if (ret != OK) {
                return ret;
            }

        } else {

            bodyToParse = std::vector<uint8_t>(
                it2,
                data + len);
        }

        auto it = bodyToParse.cbegin();

        auto ret = parseBody<VERSION, tbt_file_t>(it, out);

        if (ret != OK) {
            return ret;
        }

        ASSERT(it == bodyToParse.cend());
    }
      
    return OK;
}


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


    auto versionNumber = buf[3];


    Status ret;

    switch (versionNumber) {
    case 0x72: {
        
        tbt_file71 t;
        ret = parseTbtBytes<0x72, tbt_file71>(buf, len, t);

        delete[] buf;

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file71{t};
        
        break;
    }
    case 0x71: {
        
        tbt_file71 t;
        ret = parseTbtBytes<0x71, tbt_file71>(buf, len, t);

        delete[] buf;

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file71{t};
        
        break;
    }
    case 0x70: {
        
        tbt_file70 t;
        ret = parseTbtBytes<0x70, tbt_file70>(buf, len, t);

        delete[] buf;

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file70{t};
        
        break;
    }
    case 0x6f: {
        
        tbt_file6f t;
        ret = parseTbtBytes<0x6f, tbt_file6f>(buf, len, t);

        delete[] buf;

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file6f{t};
        
        break;
    }
    case 0x6e: {
        
        tbt_file6e t;
        ret = parseTbtBytes<0x6e, tbt_file6e>(buf, len, t);

        delete[] buf;

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file6e{t};
        
        break;
    }
    case 0x6b: {
        
        tbt_file6b t;
        ret = parseTbtBytes<0x6b, tbt_file6b>(buf, len, t);

        delete[] buf;

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file6b{t};
        
        break;
    }
    case 0x6a: {
        
        tbt_file6a t;
        ret = parseTbtBytes<0x6a, tbt_file6a>(buf, len, t);

        delete[] buf;

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file6a{t};
        
        break;
    }
    case 0x69: {
        
        tbt_file68 t;
        ret = parseTbtBytes<0x69, tbt_file68>(buf, len, t);

        delete[] buf;

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file68{t};
        
        break;
    }
    case 0x68: {
        
        tbt_file68 t;
        ret = parseTbtBytes<0x68, tbt_file68>(buf, len, t);

        delete[] buf;

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file68{t};
        
        break;
    }
    case 0x67: {
        
        tbt_file65 t;
        ret = parseTbtBytes<0x67, tbt_file65>(buf, len, t);

        delete[] buf;

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file65{t};
        
        break;
    }
    case 0x66: {
        
        tbt_file65 t;
        ret = parseTbtBytes<0x66, tbt_file65>(buf, len, t);

        delete[] buf;

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file65{t};
        
        break;
    }
    case 0x65: {
        
        tbt_file65 t;
        ret = parseTbtBytes<0x65, tbt_file65>(buf, len, t);

        delete[] buf;

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file65{t};
        
        break;
    }
    default:
        
        LOGE("unrecognized tbt file version: 0x%02x", versionNumber);
            
        delete[] buf;

        return ERR;
    }

    return OK;
}


const std::array<uint8_t, HEADER_SIZE> tbtFileHeader(const tbt_file t) {

    auto header = *reinterpret_cast<std::array<uint8_t, HEADER_SIZE> *>(t);
    return header;
}


void releaseTbtFile(tbt_file t) {

    auto versionNumber = reinterpret_cast<const uint8_t *>(t)[3];

    switch (versionNumber) {
    case 0x72:
    case 0x71: {
        
        auto t71 = reinterpret_cast<tbt_file71 *>(t);
        
        delete t71;
        
        break;
    }
    case 0x70: {
        
        auto t70 = reinterpret_cast<tbt_file70 *>(t);
        
        delete t70;
        
        break;
    }
    case 0x6f:
    case 0x6e: {
        
        auto t6e = reinterpret_cast<tbt_file6e *>(t);
        
        delete t6e;
        
        break;
    }
    case 0x6b: {
        
        auto t6b = reinterpret_cast<tbt_file6b *>(t);
        
        delete t6b;
        
        break;
    }
    case 0x6a: {
        
        auto t6a = reinterpret_cast<tbt_file6a *>(t);
        
        delete t6a;
        
        break;
    }
    case 0x69:
    case 0x68: {
        
        auto t68 = reinterpret_cast<tbt_file68 *>(t);
        
        delete t68;
        
        break;
    }
    case 0x67:
    case 0x66:
    case 0x65: {
        
        auto t65 = reinterpret_cast<tbt_file65 *>(t);
        
        delete t65;
        
        break;
    }
    default:
        ASSERT(false);
        break;
    }
}










