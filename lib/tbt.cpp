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

#include "tbt-parser/tbt-parser-util.h"
#include "tbt-parser/tbt.h"

#include "rational/rational.h"

#undef NDEBUG

#include "common/abort.h"
#include "common/assert.h"
#include "common/check.h"
#include "common/file.h"
#include "common/logging.h"

#include <cinttypes>
#include <cstring> // for memcpy, strrchr, strcmp
#include <cmath> // for round in alternate-time-regions.inl, in body.inl


#include "splitat.inl"
#include "partitioninto.inl"
#include "expanddeltalist.inl"
#include "metadata.inl"
#include "alternate-time-regions.inl"
#include "bar-lines.inl"
#include "track-effect-changes.inl"
#include "notes.inl"
#include "body.inl"


#define TAG "tbt"


template <uint8_t VERSION, bool HASALTERNATETIMEREGIONS, typename tbt_file_t>
Status
TparseTbtBytes(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator &end,
    tbt_file_t &out) {

    //
    // https://bostick.github.io/tabit-file-format/description/tabit-file-format-description.html#header
    //

    auto size = end - it;

    //
    // parse header
    //

    std::memcpy(&out.header, &*it, TBT_HEADER_SIZE);

    //
    // handle file corruption
    //

    CHECK(std::memcmp(out.header.magic.data(), "TBT", 3) == 0, "file is corrupted. magic bytes do not match. expected: TBT, actual: %c%c%c", out.header.magic[0], out.header.magic[1], out.header.magic[2]);

    if constexpr (0x68 <= VERSION) {

        CHECK(out.header.compressedMetadataLen >= 0, "file is corrupted. compressedMetadataLen is negative: %" PRIi32, out.header.compressedMetadataLen);

        CHECK(out.header.compressedMetadataLen < size, "file is corrupted. compressedMetadataLen is smaller than expected. expected: %" PRIi32 ", actual: %td", out.header.compressedMetadataLen, size);
        
        CHECK(out.header.totalByteCount == size, "file is corrupted. file byte counts do not match. expected: %" PRIi32 ", actual: %td", out.header.totalByteCount, size);

        auto restToCheck_it = it + TBT_HEADER_SIZE;

        auto crc32Rest = crc32_checksum(restToCheck_it, end);

        CHECK(crc32Rest == out.header.crc32Rest, "file is corrupted. CRC-32 of rest of file does not match. expected: %" PRIu32 ", actual: %" PRIu32,  out.header.crc32Rest, crc32Rest);

        auto headerToCheck_it = it;

        auto crc32Header = crc32_checksum(headerToCheck_it, it + TBT_HEADER_SIZE - 4);

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

    it += TBT_HEADER_SIZE;

    //
    // parse metadata
    //
    {
        if constexpr (0x6e <= VERSION) {

            int32_t metadataLen;

            if constexpr (0x71 <= VERSION) {

                metadataLen = static_cast<int32_t>(sizeof(tbt_track_metadata71) * out.header.trackCount);

                out.metadata.tracks = std::vector<tbt_track_metadata71>(out.header.trackCount);

            } else if constexpr (0x70 <= VERSION) {

                metadataLen = static_cast<int32_t>(sizeof(tbt_track_metadata70) * out.header.trackCount);

                out.metadata.tracks = std::vector<tbt_track_metadata70>(out.header.trackCount);

            } else {

                metadataLen = static_cast<int32_t>(sizeof(tbt_track_metadata6e) * out.header.trackCount);

                out.metadata.tracks = std::vector<tbt_track_metadata6e>(out.header.trackCount);
            }

            auto metadataToInflate_it = it;

            CHECK(out.header.compressedMetadataLen <= (end - it), "file is corrupted.");

            it += out.header.compressedMetadataLen;

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

            ASSERT(metadataLen == (metadataToParse_it - metadataToParse_begin));

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

            int32_t metadataLen;

            if constexpr (VERSION == 0x6b) {

                metadataLen = static_cast<int32_t>(sizeof(tbt_track_metadata6b) * out.header.trackCount);

                out.metadata.tracks = std::vector<tbt_track_metadata6b>(out.header.trackCount);

            } else if constexpr (VERSION == 0x6a) {

                metadataLen = static_cast<int32_t>(sizeof(tbt_track_metadata6a) * out.header.trackCount);

                out.metadata.tracks = std::vector<tbt_track_metadata6a>(out.header.trackCount);

            } else {

                metadataLen = static_cast<int32_t>(sizeof(tbt_track_metadata65) * out.header.trackCount);

                out.metadata.tracks = std::vector<tbt_track_metadata65>(out.header.trackCount);
            }

            CHECK(metadataLen >= 0, "file is corrupted.");

            CHECK(metadataLen <= (end - it), "file is corrupted.");

            auto metadataToParse_end = it + metadataLen;

            Status ret = parseMetadata<VERSION, tbt_file_t>(it, metadataToParse_end, out);

            if (ret != OK) {
                return ret;
            }

            ASSERT(it == metadataToParse_end);

            //
            // now read the remaining title, artist, and comment
            //

            CHECK(1 <= (end - it), "file is corrupted.");

            auto strLen = *it;

            CHECK(1 + strLen <= (end - it), "file is corrupted.");

            auto begin = it;

            it += 1 + strLen;

            out.metadata.title = std::vector<char>(begin, it);

            CHECK(1 <= (end - it), "file is corrupted.");

            strLen = *it;

            CHECK(1 + strLen <= (end - it), "file is corrupted.");

            begin = it;

            it += 1 + strLen;

            out.metadata.artist = std::vector<char>(begin, it);

            CHECK(1 <= (end - it), "file is corrupted.");

            strLen = *it;

            CHECK(1 + strLen <= (end - it), "file is corrupted.");

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
    
    const char *dot = std::strrchr(path, '.');
    if (!(dot && std::strcmp(dot, ".tbt") == 0)) {
        LOGW("tbt file does not end with .tbt: %s", path);
    }

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
    const std::vector<uint8_t>::const_iterator &end,
    tbt_file &out) {

    auto len = end - it;

    CHECK(len != 0, "empty file");

    CHECK_NOT(len <= TBT_HEADER_SIZE, "file is too small to be parsed. size: %zu", len);

    auto versionNumber_it = it + 3;

    auto versionNumber = *versionNumber_it;

    auto featureBitfield_it = it + 11;

    auto featureBitfield = *featureBitfield_it;

    Status ret;

    switch (versionNumber) {
    case 0x72: {

        tbt_file71 t;
        
        if ((featureBitfield & HASALTERNATETIMEREGIONS_MASK) == HASALTERNATETIMEREGIONS_MASK) {
            ret = TparseTbtBytes<0x72, true>(it, end, t);
        } else {
            ret = TparseTbtBytes<0x72, false>(it, end, t);
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
            ret = TparseTbtBytes<0x71, true>(it, end, t);
        } else {
            ret = TparseTbtBytes<0x71, false>(it, end, t);
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
            ret = TparseTbtBytes<0x70, true>(it, end, t);
        } else {
            ret = TparseTbtBytes<0x70, false>(it, end, t);
        }

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x6f: {

        tbt_file6f t;
        ret = TparseTbtBytes<0x6f, false>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x6e: {

        tbt_file6e t;
        ret = TparseTbtBytes<0x6e, false>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x6b: {

        tbt_file6b t;
        ret = TparseTbtBytes<0x6b, false>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x6a: {

        tbt_file6a t;
        ret = TparseTbtBytes<0x6a, false>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x69: {

        tbt_file68 t;
        ret = TparseTbtBytes<0x69, false>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x68: {

        tbt_file68 t;
        ret = TparseTbtBytes<0x68, false>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x67: {

        tbt_file65 t;
        ret = TparseTbtBytes<0x67, false>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x66: {

        tbt_file65 t;
        ret = TparseTbtBytes<0x66, false>(it, end, t);

        if (ret != OK) {
            return ret;
        }

        out = t;
        
        break;
    }
    case 0x65: {

        tbt_file65 t;
        ret = TparseTbtBytes<0x65, false>(it, end, t);

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


uint8_t tbtFileVersionNumber(const tbt_file &t) {
    return std::visit([](auto&& t) -> uint8_t {
        return t.header.versionNumber;
    }, t);
}


std::string tbtFileVersionString(const tbt_file &t) {
    return std::visit([](auto&& t) -> std::string {
        return fromPascal1String(t.header.versionString.data());
    }, t);
}


template <uint8_t VERSION, typename tbt_file_t>
std::string
TtbtFileInfo(const tbt_file_t &t) {

    std::string acc;

    if constexpr (0x6e <= VERSION) {

        acc += std::string("title: ") + trim(fromPascal2String(t.metadata.title.data()));
        acc += '\n';

        acc += std::string("artist: ") + trim(fromPascal2String(t.metadata.artist.data()));
        acc += '\n';

        acc += std::string("album: ") + trim(fromPascal2String(t.metadata.album.data()));
        acc += '\n';

        acc += std::string("transcribed by: ") + trim(fromPascal2String(t.metadata.transcribedBy.data()));
        acc += '\n';

        // acc += std::string("comment: ") + trim(fromPascal2String(t.metadata.comment.data()));
        // acc += '\n';

    } else {

        acc += std::string("title: ") + trim(fromPascal1String(t.metadata.title.data()));
        acc += '\n';

        acc += std::string("artist: ") + trim(fromPascal1String(t.metadata.artist.data()));
        acc += '\n';

        // acc += std::string("comment: ") + trim(fromPascal1String(t.metadata.comment.data()));
        // acc += '\n';
    }

    return acc;
}


std::string tbtFileInfo(const tbt_file &t) {

    auto versionNumber = tbtFileVersionNumber(t);

    switch (versionNumber) {
    case 0x72: {

        auto t71 = std::get<tbt_file71>(t);
        
        return TtbtFileInfo<0x72>(t71);
    }
    case 0x71: {
        
        auto t71 = std::get<tbt_file71>(t);
        
        return TtbtFileInfo<0x71>(t71);
    }
    case 0x70: {
        
        auto t70 = std::get<tbt_file70>(t);
        
        return TtbtFileInfo<0x70>(t70);
    }
    case 0x6f: {
        
        auto t6f = std::get<tbt_file6f>(t);
        
        return TtbtFileInfo<0x6f>(t6f);
    }
    case 0x6e: {
        
        auto t6e = std::get<tbt_file6e>(t);
        
        return TtbtFileInfo<0x6e>(t6e);
    }
    case 0x6b: {
        
        auto t6b = std::get<tbt_file6b>(t);
        
        return TtbtFileInfo<0x6b>(t6b);
    }
    case 0x6a: {
        
        auto t6a = std::get<tbt_file6a>(t);
        
        return TtbtFileInfo<0x6a>(t6a);
    }
    case 0x69: {
        
        auto t68 = std::get<tbt_file68>(t);
        
        return TtbtFileInfo<0x69>(t68);
    }
    case 0x68: {
        
        auto t68 = std::get<tbt_file68>(t);
        
        return TtbtFileInfo<0x68>(t68);
    }
    case 0x67: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TtbtFileInfo<0x67>(t65);
    }
    case 0x66: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TtbtFileInfo<0x66>(t65);
    }
    case 0x65: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TtbtFileInfo<0x65>(t65);
    }
    default:
        ABORT("invalid versionNumber: 0x%02x", versionNumber);
    }
}


template <uint8_t VERSION, typename tbt_file_t>
std::string
TtbtFileComment(const tbt_file_t &t) {

    if constexpr (0x6e <= VERSION) {

        return fromPascal2String(t.metadata.comment.data());

    } else {

        return fromPascal1String(t.metadata.comment.data());
    }
}

std::string tbtFileComment(const tbt_file &t) {

    auto versionNumber = tbtFileVersionNumber(t);

    switch (versionNumber) {
    case 0x72: {

        auto t71 = std::get<tbt_file71>(t);
        
        return TtbtFileComment<0x72>(t71);
    }
    case 0x71: {
        
        auto t71 = std::get<tbt_file71>(t);
        
        return TtbtFileComment<0x71>(t71);
    }
    case 0x70: {
        
        auto t70 = std::get<tbt_file70>(t);
        
        return TtbtFileComment<0x70>(t70);
    }
    case 0x6f: {
        
        auto t6f = std::get<tbt_file6f>(t);
        
        return TtbtFileComment<0x6f>(t6f);
    }
    case 0x6e: {
        
        auto t6e = std::get<tbt_file6e>(t);
        
        return TtbtFileComment<0x6e>(t6e);
    }
    case 0x6b: {
        
        auto t6b = std::get<tbt_file6b>(t);
        
        return TtbtFileComment<0x6b>(t6b);
    }
    case 0x6a: {
        
        auto t6a = std::get<tbt_file6a>(t);
        
        return TtbtFileComment<0x6a>(t6a);
    }
    case 0x69: {
        
        auto t68 = std::get<tbt_file68>(t);
        
        return TtbtFileComment<0x69>(t68);
    }
    case 0x68: {
        
        auto t68 = std::get<tbt_file68>(t);
        
        return TtbtFileComment<0x68>(t68);
    }
    case 0x67: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TtbtFileComment<0x67>(t65);
    }
    case 0x66: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TtbtFileComment<0x66>(t65);
    }
    case 0x65: {
        
        auto t65 = std::get<tbt_file65>(t);
        
        return TtbtFileComment<0x65>(t65);
    }
    default:
        ABORT("invalid versionNumber: 0x%02x", versionNumber);
    }
}








