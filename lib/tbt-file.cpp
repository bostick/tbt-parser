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


template <uint8_t versionNumber, typename tbt_file_t>
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

    if constexpr (0x67 < versionNumber) {

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
    }
    
    if (!(out.header.versionString[0] == 3 || out.header.versionString[0] == 4)) {

        LOGE("parseTBT: versionString is weird");

        return ERR;
    }

    if ((out.header.featureBitfield & 0b00000001) != 0b00000001) {

        LOGE("parseTBT: featureBitfield is weird: 0x%02x", out.header.featureBitfield);

        return ERR;
    }

    for (uint8_t &u : out.header.unused) {

        if (u != 0) {

            LOGE("parseTBT: unused is not all 0");

            return ERR;
        }
    }

    if constexpr (0x72 < versionNumber) {

        LOGE("parseTBT: This file was created with a later version of TabIt. You'll need to upgrade to TabIt version %s or later in order to open this file.", out.header.versionString.data() + 1);

        return ERR;
    }

    if constexpr (0x70 <= versionNumber) {

        if (32000 < out.header.barCountGE70) {

            LOGE("parseTBT: Unable to load the file because it contains more bars than this version of TabIt supports");

            return ERR;
        }
    }

    if (15 < out.header.trackCount) {

        LOGE("parseTBT: Unable to load the file because it contains more tracks than this version of TabIt supports");

        return ERR;
    }

    if constexpr (0x6e <= versionNumber) {
        if (out.header.tempo1 != out.header.tempo2) {
            ASSERT(out.header.tempo1 == 250);
            ASSERT(out.header.tempo2 != 0);
        }
    } else {
        ASSERT(out.header.tempo2 == 0);
    }

    const uint8_t *it2;
    
    //
    // parse metadata
    //
    {
        if constexpr (0x6e <= versionNumber) {

            if constexpr (0x71 <= versionNumber) {

                out.metadata.tracks = std::vector<tbt_track_metadata71>(out.header.trackCount);

            } else if constexpr (0x70 <= versionNumber) {

                out.metadata.tracks = std::vector<tbt_track_metadata70>(out.header.trackCount);

            } else {

                out.metadata.tracks = std::vector<tbt_track_metadata6d>(out.header.trackCount);
            }

            auto metadataToInflate = std::vector<uint8_t>(
                data + HEADER_SIZE,
                data + HEADER_SIZE + out.header.metadataLen);

            std::vector<uint8_t> metadataToParse;

            Status ret = zlib_inflate(metadataToInflate, metadataToParse);

            if (ret != OK) {
                return ret;
            }

            auto it = metadataToParse.cbegin();

            ret = parseMetadata<versionNumber, tbt_file_t>(it, out);

            if (ret != OK) {
                return ret;
            }

            ASSERT(it == metadataToParse.end());

        } else {

            uint32_t metadataLen;

            if constexpr (versionNumber == 0x6d) {

                metadataLen = sizeof(tbt_track_metadata6d) * out.header.trackCount;

                out.metadata.tracks = std::vector<tbt_track_metadata6d>(out.header.trackCount);

            } else if constexpr (versionNumber == 0x6c) {

                metadataLen = sizeof(tbt_track_metadata6c) * out.header.trackCount;

                out.metadata.tracks = std::vector<tbt_track_metadata6c>(out.header.trackCount);

            } else if constexpr (versionNumber == 0x6b) {

                metadataLen = sizeof(tbt_track_metadata6b) * out.header.trackCount;

                out.metadata.tracks = std::vector<tbt_track_metadata6b>(out.header.trackCount);

            } else if constexpr (versionNumber == 0x6a) {

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

            Status ret = parseMetadata<versionNumber, tbt_file_t>(it, out);

            if (ret != OK) {
                return ret;
            }

            ASSERT(it == metadataToParse.end());

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
        }
    }

    //
    // parse body
    //
    {
        if constexpr (0x6e <= versionNumber) {

            auto bodyToInflate = std::vector<uint8_t>(
                data + HEADER_SIZE + out.header.metadataLen,
                data + len);

            Status ret;

            std::vector<uint8_t> bodyToParse;
            ret = zlib_inflate(bodyToInflate, bodyToParse);

            if (ret != OK) {
                return ret;
            }

            auto it = bodyToParse.cbegin();

            ret = parseBody<versionNumber, tbt_file_t>(it, out);

            if (ret != OK) {
                return ret;
            }

            ASSERT(it == bodyToParse.end());

        } else {

            Status ret;

            auto bodyToParse = std::vector<uint8_t>(
                it2,
                data + len);

            auto it = bodyToParse.cbegin();

            ret = parseBody<versionNumber, tbt_file_t>(it, out);

            if (ret != OK) {
                return ret;
            }

            ASSERT(it == bodyToParse.end());
        }
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

    auto buf = std::vector<uint8_t>(len);

    size_t r = fread(buf.data(), sizeof(uint8_t), len, file);

    if (r != len) {

        LOGE("fread failed");

        return ERR;
    }

    fclose(file);


    if (len == 0) {
        
        LOGE("empty file");

        return ERR;
        
    } else if (len < HEADER_SIZE) {
        
        LOGE("file is too small to be parsed. size: %zu", len);

        return ERR;
    }
    
    
    auto versionNumber = buf[3];


    Status ret;

    switch (versionNumber) {
    case 0x72: {
        
        tbt_file71 t;
        ret = parseTbtBytes<0x72, tbt_file71>(buf.data(), len, t);

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file71{t};
        
        break;
    }
    case 0x71: {
        
        tbt_file71 t;
        ret = parseTbtBytes<0x71, tbt_file71>(buf.data(), len, t);

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file71{t};
        
        break;
    }
    case 0x70: {
        
        tbt_file70 t;
        ret = parseTbtBytes<0x70, tbt_file70>(buf.data(), len, t);

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file70{t};
        
        break;
    }
    case 0x6f: {
        
        tbt_file6d t;
        ret = parseTbtBytes<0x6f, tbt_file6d>(buf.data(), len, t);

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file6d{t};
        
        break;
    }
    case 0x6e: {
        
        tbt_file6d t;
        ret = parseTbtBytes<0x6e, tbt_file6d>(buf.data(), len, t);

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file6d{t};
        
        break;
    }
    case 0x6b: {
        
        tbt_file6b t;
        ret = parseTbtBytes<0x6b, tbt_file6b>(buf.data(), len, t);

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file6b{t};
        
        break;
    }
    case 0x6a: {
        
        tbt_file6a t;
        ret = parseTbtBytes<0x6a, tbt_file6a>(buf.data(), len, t);

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file6a{t};
        
        break;
    }
    case 0x69: {
        
        tbt_file65 t;
        ret = parseTbtBytes<0x69, tbt_file65>(buf.data(), len, t);

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file65{t};
        
        break;
    }
    case 0x68: {
        
        tbt_file65 t;
        ret = parseTbtBytes<0x68, tbt_file65>(buf.data(), len, t);

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file65{t};
        
        break;
    }
    case 0x67: {
        
        tbt_file65 t;
        ret = parseTbtBytes<0x67, tbt_file65>(buf.data(), len, t);

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file65{t};
        
        break;
    }
    case 0x66: {
        
        tbt_file65 t;
        ret = parseTbtBytes<0x66, tbt_file65>(buf.data(), len, t);

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file65{t};
        
        break;
    }
    case 0x65: {
        
        tbt_file65 t;
        ret = parseTbtBytes<0x65, tbt_file65>(buf.data(), len, t);

        if (ret != OK) {
            return ret;
        }

        *out = new tbt_file65{t};
        
        break;
    }
    default:
        
        LOGE("unrecognized tbt file version: 0x%02x", versionNumber);

        return ERR;
    }

    return OK;
}


const tbt_header tbtFileHeader(const tbt_file t) {

    auto versionNumber = reinterpret_cast<const uint8_t *>(t)[3];

    switch (versionNumber) {
    case 0x72:
    case 0x71: {
        
        auto t71 = reinterpret_cast<tbt_file71 *>(t);
        
        return t71->header;
    }
    case 0x70: {
        
        auto t70 = reinterpret_cast<tbt_file70 *>(t);
        
        return t70->header;
    }
    case 0x6f:
    case 0x6e: {
        
        auto t6d = reinterpret_cast<tbt_file6d *>(t);
        
        return t6d->header;
    }
    case 0x6b: {
        
        auto t6b = reinterpret_cast<tbt_file6b *>(t);
        
        return t6b->header;
    }
    case 0x6a: {
        
        auto t6a = reinterpret_cast<tbt_file6a *>(t);
        
        return t6a->header;
    }
    case 0x69:
    case 0x68:
    case 0x67:
    case 0x66:
    case 0x65: {
        
        auto t65 = reinterpret_cast<tbt_file65 *>(t);
        
        return t65->header;
    }
    default:
        
        ASSERT(false);
        
        return tbt_header{};
    }
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
        
        auto t6d = reinterpret_cast<tbt_file6d *>(t);
        
        delete t6d;
        
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
    case 0x68:
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










