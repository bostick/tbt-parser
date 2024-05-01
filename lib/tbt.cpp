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

#include "tbt-parser.h"

#include "tbt-parser/rational.h"
#include "tbt-parser/tbt-parser-util.h"

#include "common/assert.h"
#include "common/check.h"
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


template <uint8_t VERSION, bool HASALTERNATETIMEREGIONS, typename tbt_file_t>
Status
TparseTbtBytes(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    tbt_file_t &out) {

    //
    // https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#header
    //

    auto size = end - it;

    //
    // parse header
    //

    std::memcpy(&out.header, &*it, HEADER_SIZE);

    //
    // handle file corruption
    //

    CHECK(std::memcmp(out.header.magic.data(), "TBT", 3) == 0, "file is corrupted. magic bytes do not match. expected: TBT, actual: %c%c%c", out.header.magic[0], out.header.magic[1], out.header.magic[2]);

    if constexpr (0x68 <= VERSION) {

        CHECK(out.header.compressedMetadataLen < size, "unhandled");
        
        CHECK(out.header.totalByteCount == size, "file is corrupted. file byte counts do not match. expected: %" PRIu32 ", actual: %zu", out.header.totalByteCount, size);

        auto restToCheck_it = it + HEADER_SIZE;

        auto crc32Rest = crc32_checksum(restToCheck_it, end);

        CHECK(crc32Rest == out.header.crc32Rest, "file is corrupted. CRC-32 of rest of file does not match. expected: %" PRIu32 ", actual: %" PRIu32,  out.header.crc32Rest, crc32Rest);

        auto headerToCheck_it = it;

        auto crc32Header = crc32_checksum(headerToCheck_it, it + HEADER_SIZE - 4);

        CHECK(crc32Header == out.header.crc32Header, "file is corrupted. CRC-32 of header does not match. expected: %" PRIu32 ", actual: %" PRIu32, out.header.crc32Header, crc32Header);
    }

    CHECK(out.header.versionString[0] == 3 || out.header.versionString[0] == 4, "file is corrupted.");

    if constexpr (0x70 <= VERSION) {

        CHECK(out.header.barCount != 0, "file is corrupted.");

    } else {

        CHECK(out.header.barCount_unused == 0, "file is corrupted.");
    }

    if constexpr (VERSION == 0x6f) {

        CHECK(out.header.spaceCount != 0, "file is corrupted.");

    } else {

        CHECK(out.header.spaceCount_unused == 0, "file is corrupted.");
    }

    if constexpr (0x6e <= VERSION && VERSION <= 0x6f) {

        //
        // Nothing to assert: lastNonEmptySpace may be 0
        //
        ;

    } else {

        CHECK(out.header.lastNonEmptySpace_unused == 0, "file is corrupted.");
    }

    if constexpr (0x6e <= VERSION) {

        CHECK(out.header.tempo2 != 0, "file is corrupted.");

        if (250 <= out.header.tempo2) {

            CHECK(out.header.tempo1 == 250, "file is corrupted.");

        } else {

            CHECK(out.header.tempo1 == out.header.tempo2, "file is corrupted.");
        }

    } else {

        CHECK(out.header.tempo2_unused == 0, "file is corrupted.");
    }

    it += HEADER_SIZE;

    //
    // parse metadata
    //
    {
        if constexpr (0x6e <= VERSION) {

#ifndef NDEBUG
            uint32_t metadataLen;
#endif // NDEBUG

            if constexpr (0x71 <= VERSION) {

#ifndef NDEBUG
                metadataLen = sizeof(tbt_track_metadata71) * out.header.trackCount;
#endif // NDEBUG

                out.metadata.tracks = std::vector<tbt_track_metadata71>(out.header.trackCount);

            } else if constexpr (0x70 <= VERSION) {

#ifndef NDEBUG
                metadataLen = sizeof(tbt_track_metadata70) * out.header.trackCount;
#endif // NDEBUG

                out.metadata.tracks = std::vector<tbt_track_metadata70>(out.header.trackCount);

            } else {

#ifndef NDEBUG
                metadataLen = sizeof(tbt_track_metadata6e) * out.header.trackCount;
#endif // NDEBUG

                out.metadata.tracks = std::vector<tbt_track_metadata6e>(out.header.trackCount);
            }

            auto metadataToInflate_it = it;

            it += out.header.compressedMetadataLen;

            CHECK(it <= end, "file is corrupted.");

            auto metadataToInflate_end = it;

            std::vector<uint8_t> metadataToParse;

            Status ret = zlib_inflate(metadataToInflate_it, metadataToInflate_end, metadataToParse);

            if (ret != OK) {
                return ret;
            }

            auto metadataToParse_begin = metadataToParse.cbegin();

            auto metadataToParse_it = metadataToParse_begin;

            auto metadataToParse_end = metadataToParse.cend();

            ret = parseMetadata<VERSION, tbt_file_t>(metadataToParse_it, metadataToParse_end, out);

            if (ret != OK) {
                return ret;
            }

            ASSERT(metadataToParse_it == metadataToParse_begin + metadataLen);

            //
            // now read the remaining title, artist, album, transcribedBy, and comment
            //

            ret = readPascal2String(metadataToParse_it, metadataToParse_end, out.metadata.title);

            if (ret != OK) {
                return ret;
            }

            ret = readPascal2String(metadataToParse_it, metadataToParse_end, out.metadata.artist);

            if (ret != OK) {
                return ret;
            }

            ret = readPascal2String(metadataToParse_it, metadataToParse_end, out.metadata.album);

            if (ret != OK) {
                return ret;
            }

            ret = readPascal2String(metadataToParse_it, metadataToParse_end, out.metadata.transcribedBy);

            if (ret != OK) {
                return ret;
            }

            ret = readPascal2String(metadataToParse_it, metadataToParse_end, out.metadata.comment);

            if (ret != OK) {
                return ret;
            }

            CHECK(metadataToParse_it == metadataToParse_end, "unhandled");

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

            auto metadataToParse_end = it + metadataLen;

            CHECK(metadataToParse_end <= end, "file is corrupted.");

            Status ret = parseMetadata<VERSION, tbt_file_t>(it, metadataToParse_end, out);

            if (ret != OK) {
                return ret;
            }

            ASSERT(it == metadataToParse_end);

            //
            // now read the remaining title, artist, and comment
            //

            CHECK(it + 1 <= end, "file is corrupted.");

            auto strLen = *it;

            CHECK(it + 1 + strLen <= end, "file is corrupted.");

            auto begin = it;

            it += 1 + strLen;

            out.metadata.title = std::vector<char>(begin, it);

            CHECK(it + 1 <= end, "file is corrupted.");

            strLen = *it;

            CHECK(it + 1 + strLen <= end, "file is corrupted.");

            begin = it;

            it += 1 + strLen;

            out.metadata.artist = std::vector<char>(begin, it);

            CHECK(it + 1 <= end, "file is corrupted.");

            strLen = *it;

            CHECK(it + 1 + strLen <= end, "file is corrupted.");

            begin = it;

            it += 1 + strLen;

            out.metadata.comment = std::vector<char>(begin, it);

            //
            // Nothing to assert
            //
        }
    }

    //
    // parse body
    //
    {
        if constexpr (0x6e <= VERSION) {

            CHECK(it <= end, "unhandled");

            std::vector<uint8_t> bodyToParse;

            Status ret = zlib_inflate(it, end, bodyToParse);

            if (ret != OK) {
                return ret;
            }

            CHECK(it == end, "file is corrupted.");

            auto bodyToParse_it = bodyToParse.cbegin();

            auto bodyToParse_end = bodyToParse.cend();

            ret = parseBody<VERSION, HASALTERNATETIMEREGIONS, tbt_file_t>(bodyToParse_it, bodyToParse_end, out);

            if (ret != OK) {
                return ret;
            }

            CHECK(bodyToParse_it == bodyToParse_end, "file is corrupted.");

        } else {

            CHECK(it <= end, "unhandled");

            Status ret = parseBody<VERSION, false, tbt_file_t>(it, end, out);

            if (ret != OK) {
                return ret;
            }

            CHECK(it == end, "file is corrupted.");
        }
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

    auto buf_it = buf.cbegin();

    auto buf_end = buf.cend();

    return parseTbtBytes(buf_it, buf_end, out);
}
    

Status
parseTbtBytes(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    tbt_file &out) {

    auto len = end - it;

    CHECK(len != 0, "empty file");

    CHECK_NOT(len <= HEADER_SIZE, "file is too small to be parsed. size: %zu", len);

    auto versionNumber_it = it + 3;

    auto versionNumber = *versionNumber_it;

    auto featureBitfield_it = it + 11;

    auto featureBitfield = *featureBitfield_it;

    Status ret;

    switch (versionNumber) {
    case 0x72: {

        tbt_file71 t;
        
        if ((featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            ret = TparseTbtBytes<0x72, true, tbt_file71>(it, end, t);
        } else {
            ret = TparseTbtBytes<0x72, false, tbt_file71>(it, end, t);
        }

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x71: {

        tbt_file71 t;

        if ((featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            ret = TparseTbtBytes<0x71, true, tbt_file71>(it, end, t);
        } else {
            ret = TparseTbtBytes<0x71, false, tbt_file71>(it, end, t);
        }

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x70: {

        tbt_file70 t;
        
        if ((featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            ret = TparseTbtBytes<0x70, true, tbt_file70>(it, end, t);
        } else {
            ret = TparseTbtBytes<0x70, false, tbt_file70>(it, end, t);
        }

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x6f: {

        tbt_file6f t;
        ret = TparseTbtBytes<0x6f, false, tbt_file6f>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x6e: {

        tbt_file6e t;
        ret = TparseTbtBytes<0x6e, false, tbt_file6e>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x6b: {

        tbt_file6b t;
        ret = TparseTbtBytes<0x6b, false, tbt_file6b>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x6a: {

        tbt_file6a t;
        ret = TparseTbtBytes<0x6a, false, tbt_file6a>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x69: {

        tbt_file68 t;
        ret = TparseTbtBytes<0x69, false, tbt_file68>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x68: {

        tbt_file68 t;
        ret = TparseTbtBytes<0x68, false, tbt_file68>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x67: {

        tbt_file65 t;
        ret = TparseTbtBytes<0x67, false, tbt_file65>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x66: {

        tbt_file65 t;
        ret = TparseTbtBytes<0x66, false, tbt_file65>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x65: {

        tbt_file65 t;
        ret = TparseTbtBytes<0x65, false, tbt_file65>(it, end, t);

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


struct TbtVersionNumberVisitor {
    
    uint8_t operator()(const tbt_file65 &t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file68 &t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file6a &t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file6b &t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file6e &t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file6f &t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file70 &t) {
        return t.header.versionNumber;
    }

    uint8_t operator()(const tbt_file71 &t) {
        return t.header.versionNumber;
    }
};

uint8_t tbtFileVersionNumber(const tbt_file &t) {
    return std::visit(TbtVersionNumberVisitor{}, t);
}


struct TbtVersionStringVisitor {

    std::string operator()(const tbt_file65 &t) {
        return fromPascal1String(t.header.versionString.data());
    }

    std::string operator()(const tbt_file68 &t) {
        return fromPascal1String(t.header.versionString.data());
    }

    std::string operator()(const tbt_file6a &t) {
        return fromPascal1String(t.header.versionString.data());
    }

    std::string operator()(const tbt_file6b &t) {
        return fromPascal1String(t.header.versionString.data());
    }

    std::string operator()(const tbt_file6e &t) {
        return fromPascal1String(t.header.versionString.data());
    }

    std::string operator()(const tbt_file6f &t) {
        return fromPascal1String(t.header.versionString.data());
    }

    std::string operator()(const tbt_file70 &t) {
        return fromPascal1String(t.header.versionString.data());
    }

    std::string operator()(const tbt_file71 &t) {
        return fromPascal1String(t.header.versionString.data());
    }
};

std::string tbtFileVersionString(const tbt_file &t) {
    return std::visit(TbtVersionStringVisitor{}, t);
}


struct TbtInfoVisitor {
    
    void operator()(const tbt_file65 &t) {

        auto versionString = fromPascal1String(t.header.versionString.data());

        auto versionNumber = t.header.versionNumber;

        LOGI("tbt file version: %s (0x%02x)", versionString.c_str(), versionNumber);

        LOGI("title: %s", trim(fromPascal1String(t.metadata.title.data())).c_str());

        LOGI("artist: %s", trim(fromPascal1String(t.metadata.artist.data())).c_str());

        // LOGI("%s", trim(fromPascal1String(t.metadata.comment.data())).c_str());
    }

    void operator()(const tbt_file68 &t) {

        auto versionString = fromPascal1String(t.header.versionString.data());

        auto versionNumber = t.header.versionNumber;

        LOGI("tbt file version: %s (0x%02x)", versionString.c_str(), versionNumber);

        LOGI("title: %s", trim(fromPascal1String(t.metadata.title.data())).c_str());

        LOGI("artist: %s", trim(fromPascal1String(t.metadata.artist.data())).c_str());

        // LOGI("%s", trim(fromPascal1String(t.metadata.comment.data())).c_str());
    }

    void operator()(const tbt_file6a &t) {

        auto versionString = fromPascal1String(t.header.versionString.data());

        auto versionNumber = t.header.versionNumber;

        LOGI("tbt file version: %s (0x%02x)", versionString.c_str(), versionNumber);

        LOGI("title: %s", trim(fromPascal1String(t.metadata.title.data())).c_str());

        LOGI("artist: %s", trim(fromPascal1String(t.metadata.artist.data())).c_str());

        // LOGI("%s", trim(fromPascal1String(t.metadata.comment.data())).c_str());
    }

    void operator()(const tbt_file6b &t) {

        auto versionString = fromPascal1String(t.header.versionString.data());

        auto versionNumber = t.header.versionNumber;

        LOGI("tbt file version: %s (0x%02x)", versionString.c_str(), versionNumber);

        LOGI("title: %s", trim(fromPascal1String(t.metadata.title.data())).c_str());

        LOGI("artist: %s", trim(fromPascal1String(t.metadata.artist.data())).c_str());

        // LOGI("%s", trim(fromPascal1String(t.metadata.comment.data())).c_str());
    }

    void operator()(const tbt_file6e &t) {

        auto versionString = fromPascal1String(t.header.versionString.data());

        auto versionNumber = t.header.versionNumber;

        LOGI("tbt file version: %s (0x%02x)", versionString.c_str(), versionNumber);

        LOGI("title: %s", trim(fromPascal2String(t.metadata.title.data())).c_str());

        LOGI("artist: %s", trim(fromPascal2String(t.metadata.artist.data())).c_str());

        LOGI("album: %s", trim(fromPascal2String(t.metadata.album.data())).c_str());

        LOGI("transcribed by: %s", trim(fromPascal2String(t.metadata.transcribedBy.data())).c_str());

        // LOGI("%s", trim(fromPascal2String(t.metadata.comment.data())).c_str());
    }

    void operator()(const tbt_file6f &t) {

        auto versionString = fromPascal1String(t.header.versionString.data());

        auto versionNumber = t.header.versionNumber;

        LOGI("tbt file version: %s (0x%02x)", versionString.c_str(), versionNumber);

        LOGI("title: %s", trim(fromPascal2String(t.metadata.title.data())).c_str());

        LOGI("artist: %s", trim(fromPascal2String(t.metadata.artist.data())).c_str());

        LOGI("album: %s", trim(fromPascal2String(t.metadata.album.data())).c_str());

        LOGI("transcribed by: %s", trim(fromPascal2String(t.metadata.transcribedBy.data())).c_str());

        // LOGI("%s", trim(fromPascal2String(t.metadata.comment.data())).c_str());
    }

    void operator()(const tbt_file70 &t) {

        auto versionString = fromPascal1String(t.header.versionString.data());

        auto versionNumber = t.header.versionNumber;

        LOGI("tbt file version: %s (0x%02x)", versionString.c_str(), versionNumber);

        LOGI("title: %s", trim(fromPascal2String(t.metadata.title.data())).c_str());

        LOGI("artist: %s", trim(fromPascal2String(t.metadata.artist.data())).c_str());

        LOGI("album: %s", trim(fromPascal2String(t.metadata.album.data())).c_str());

        LOGI("transcribed by: %s", trim(fromPascal2String(t.metadata.transcribedBy.data())).c_str());

        // LOGI("%s", trim(fromPascal2String(t.metadata.comment.data())).c_str());
    }

    void operator()(const tbt_file71 &t) {

        auto versionString = fromPascal1String(t.header.versionString.data());

        auto versionNumber = t.header.versionNumber;

        LOGI("tbt file version: %s (0x%02x)", versionString.c_str(), versionNumber);

        LOGI("title: %s", trim(fromPascal2String(t.metadata.title.data())).c_str());

        LOGI("artist: %s", trim(fromPascal2String(t.metadata.artist.data())).c_str());

        LOGI("album: %s", trim(fromPascal2String(t.metadata.album.data())).c_str());

        LOGI("transcribed by: %s", trim(fromPascal2String(t.metadata.transcribedBy.data())).c_str());

        // LOGI("%s", trim(fromPascal2String(t.metadata.comment.data())).c_str());
    }
};

void tbtFileInfo(const tbt_file &t) {
    std::visit(TbtInfoVisitor{}, t);
}










