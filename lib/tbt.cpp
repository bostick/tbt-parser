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

#include "tbt-parser.h"

#include "tbt-parser/midi.h"
#include "tbt-parser/tbt-parser-util.h"

#include "common/assert.h"
#include "common/file.h"
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
TparseTbtBytes(
    const std::vector<uint8_t> data,
    tbt_file_t &out) {

    //
    // https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#header
    //
    
    //
    // parse header
    //

    const uint8_t *headerToParse = data.data() + 0;

    memcpy(&out.header, headerToParse, HEADER_SIZE);

    //
    // handle file corruption
    //

    if (memcmp(out.header.magic.data(), "TBT", 3) != 0) {

        LOGE("file is corrupted. magic bytes do not match. expected: TBT, actual: %c%c%c", out.header.magic[0], out.header.magic[1], out.header.magic[2]);

        return ERR;
    }

    if constexpr (0x68 <= VERSION) {
        
        if (data.size() != out.header.totalByteCount) {

            LOGE("file is corrupted. file byte counts do not match. expected: %" PRIu32 ", actual: %zu", out.header.totalByteCount, data.size());

            return ERR;
        }
        
        auto restToCheck = std::vector<uint8_t>(data.data() + HEADER_SIZE, data.data() + data.size());
        
        auto crc32Rest = crc32_checksum(restToCheck);
        
        if (crc32Rest != out.header.crc32Rest) {

            LOGE("file is corrupted. CRC-32 of rest of file does not match. expected: %" PRIu32 ", actual: %" PRIu32,  out.header.crc32Rest, crc32Rest);

            return ERR;
        }

        auto headerToCheck = std::vector<uint8_t>(data.data() + 0, data.data() + HEADER_SIZE - 4);

        auto crc32Header = crc32_checksum(headerToCheck);

        if (crc32Header != out.header.crc32Header) {

            LOGE("file is corrupted. CRC-32 of header does not match. expected: %" PRIu32 ", actual: %" PRIu32, out.header.crc32Header, crc32Header);

            return ERR;
        }
    }

    //
    // no need to return ERR from here and below: just assert
    //

    ASSERT(out.header.versionString[0] == 3 || out.header.versionString[0] == 4);

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
                data.data() + HEADER_SIZE,
                data.data() + HEADER_SIZE + out.header.compressedMetadataLen);

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
                data.data() + HEADER_SIZE,
                data.data() + HEADER_SIZE + metadataLen);

            auto it = metadataToParse.cbegin();

            Status ret = parseMetadata<VERSION, tbt_file_t>(it, out);

            if (ret != OK) {
                return ret;
            }

            ASSERT(it == metadataToParse.cend());

            //
            // now read the remaining title, artist, and comment
            //
            
            it2 = data.data() + HEADER_SIZE + metadataLen;
            
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
                data.data() + HEADER_SIZE + out.header.compressedMetadataLen,
                data.data() + data.size());

            auto ret = zlib_inflate(bodyToInflate, bodyToParse);

            if (ret != OK) {
                return ret;
            }

        } else {

            bodyToParse = std::vector<uint8_t>(
                it2,
                data.data() + data.size());
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
    tbt_file &out) {
    
    Status ret;

    std::vector<uint8_t> buf;

    ret = openFile(path, buf);

    if (ret != OK) {

        return ret;
    }

    auto len = buf.size();

    if (len == 0) {
        
        LOGE("empty file");

        return ERR;
    }

    if (len < HEADER_SIZE) {
        
        LOGE("file is too small to be parsed. size: %zu", len);

        return ERR;
    }

    return parseTbtBytes(buf, out);
}
    

Status
parseTbtBytes(
    const std::vector<uint8_t> data,
    tbt_file &out) {

    auto versionNumber = data[3];

    Status ret;

    switch (versionNumber) {
    case 0x72: {
        
        tbt_file71 t;
        ret = TparseTbtBytes<0x72, tbt_file71>(data, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x71: {
        
        tbt_file71 t;
        ret = TparseTbtBytes<0x71, tbt_file71>(data, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x70: {
        
        tbt_file70 t;
        ret = TparseTbtBytes<0x70, tbt_file70>(data, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x6f: {
        
        tbt_file6f t;
        ret = TparseTbtBytes<0x6f, tbt_file6f>(data, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x6e: {
        
        tbt_file6e t;
        ret = TparseTbtBytes<0x6e, tbt_file6e>(data, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x6b: {
        
        tbt_file6b t;
        ret = TparseTbtBytes<0x6b, tbt_file6b>(data, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x6a: {
        
        tbt_file6a t;
        ret = TparseTbtBytes<0x6a, tbt_file6a>(data, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x69: {
        
        tbt_file68 t;
        ret = TparseTbtBytes<0x69, tbt_file68>(data, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x68: {
        
        tbt_file68 t;
        ret = TparseTbtBytes<0x68, tbt_file68>(data, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x67: {
        
        tbt_file65 t;
        ret = TparseTbtBytes<0x67, tbt_file65>(data, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x66: {
        
        tbt_file65 t;
        ret = TparseTbtBytes<0x66, tbt_file65>(data, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x65: {
        
        tbt_file65 t;
        ret = TparseTbtBytes<0x65, tbt_file65>(data, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    default:
        
        LOGE("unrecognized tbt file version: 0x%02x", versionNumber);

        return ERR;
    }

    return OK;
}


struct VersionNumberVisitor {
    
    uint8_t operator()(const tbt_file65& t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file68& t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file6a& t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file6b& t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file6e& t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file6f& t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file70& t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file71& t) {
        return t.header.versionNumber;
    }
};

uint8_t tbtFileVersionNumber(const tbt_file t) {
    return std::visit(VersionNumberVisitor{}, t);
}

struct VersionStringVisitor {
    
    std::array<uint8_t, 5> operator()(const tbt_file65& t) {
        return t.header.versionString;
    }

    std::array<uint8_t, 5> operator()(const tbt_file68& t) {
        return t.header.versionString;
    }

    std::array<uint8_t, 5> operator()(const tbt_file6a& t) {
        return t.header.versionString;
    }

    std::array<uint8_t, 5> operator()(const tbt_file6b& t) {
        return t.header.versionString;
    }

    std::array<uint8_t, 5> operator()(const tbt_file6e& t) {
        return t.header.versionString;
    }

    std::array<uint8_t, 5> operator()(const tbt_file6f& t) {
        return t.header.versionString;
    }

    std::array<uint8_t, 5> operator()(const tbt_file70& t) {
        return t.header.versionString;
    }

    std::array<uint8_t, 5> operator()(const tbt_file71& t) {
        return t.header.versionString;
    }
};

std::array<uint8_t, 5> tbtFileVersionString(const tbt_file t) {
    return std::visit(VersionStringVisitor{}, t);
}










