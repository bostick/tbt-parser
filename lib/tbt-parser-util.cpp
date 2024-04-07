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

#include "tbt-parser/tbt-parser-util.h"

#include "common/assert.h"
#include "common/check.h"
#include "common/logging.h"

#include "zlib.h"

#include "partitioninto.inl"
#include "splitat.inl"


#define TAG "tbt-parser-util"


uint16_t parseLE2(std::vector<uint8_t>::const_iterator &it) {
    return static_cast<uint16_t>((*it++ << 0) | (*it++ << 8));
}


uint16_t parseLE2(const uint8_t *data) {
    return static_cast<uint16_t>((data[0] << 0) | (data[1] << 8));
}


uint16_t parseLE2(uint8_t b0, uint8_t b1) {
    return static_cast<uint16_t>((b0 << 0) | (b1 << 8));
}


uint32_t parseLE4(std::vector<uint8_t>::const_iterator &it) {
    return static_cast<uint32_t>((*it++ << 0) | (*it++ << 8) | (*it++ << 16) | (*it++ << 24));
}


uint32_t parseLE4(const uint8_t *data) {
    return static_cast<uint32_t>((data[0] << 0) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
}


uint32_t parseLE4(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    return static_cast<uint32_t>((b0 << 0) | (b1 << 8) | (b2 << 16) | (b3 << 24));
}


Status
readPascal2String(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    std::vector<char> &out) {

    (void)end;

    auto begin = it;

    CHECK(it + 2 <= end, "out of data");

    auto len = parseLE2(*it++, *it++);

    CHECK(it + len <= end, "out of data");

    out = std::vector<char>(begin, it + len);
    it += len;

    return OK;
}


Status
parseDeltaListChunk(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    std::vector<uint8_t> &out) {

    CHECK(it + 2 <= end, "out of data");

    auto count = parseLE2(*it++, *it++);

    CHECK(count <= 0x1000, "out of data");

    CHECK(it + 2 <= end, "out of data");

    out = std::vector<uint8_t>(it, it + 2 * count);
    it += 2 * count;

    return OK;
}


Status
parseChunk4(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    std::vector<uint8_t> &out) {

    CHECK(it + 4 <= end, "out of data");

    auto count = parseLE4(*it++, *it++, *it++, *it++);

    static const int MAX_SIGNED_INT32 = 0x7fffffff;

    CHECK(count <= MAX_SIGNED_INT32, "unhandled");

    CHECK(it + static_cast<int32_t>(count) <= end, "unhandled");

    out = std::vector<uint8_t>(it, it + static_cast<int32_t>(count));
    it += static_cast<int32_t>(count);

    return OK;
}


// clang-format off
uint32_t table[256] = {
    0,          0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};
// clang-format on


uint32_t crc32_checksum(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end) {

    uint32_t acc = 0xffffffff;

    while (it < end) {
        acc = table[(acc & 0xff) ^ *it++] ^ (acc >> 8);
    }

    return acc ^ 0xffffffff;
}


/* Version history:
   1.0  30 Oct 2004  First version
   1.1   8 Nov 2004  Add void casting for unused return values
                     Use switch statement for inflate() return values
   1.2   9 Nov 2004  Add assertions to document zlib guarantees
   1.3   6 Apr 2005  Remove incorrect assertion in inf()
   1.4  11 Dec 2005  Add hack to avoid MSDOS end-of-line conversions
                     Avoid some compiler warnings for input and output buffers
 */

#define CHUNK 16384

void zerr(int ret);

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
Status
zlib_inflate(
    std::vector<uint8_t>::const_iterator &it,
    const std::vector<uint8_t>::const_iterator end,
    std::vector<uint8_t> &acc) {

    int ret;
    unsigned have;
    z_stream strm;

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK) {
        zerr(ret);
        return ERR;
    }

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = static_cast<uInt>(end - it);
        strm.next_in = const_cast<uint8_t *>(&*it);

        /* run inflate() on input until output buffer not full */

        uint8_t out[CHUNK];

        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR) {
                LOGE("ret == Z_STREAM_ERROR");
                return ERR;
            }
            switch (ret) {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR;
                    /* fall through */
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    inflateEnd(&strm);
                    zerr(ret);
                    return ERR;
                default:
                    break;
            }

            have = CHUNK - strm.avail_out;
            acc.insert(acc.end(), out, out + have);

        } while (strm.avail_out == 0);

    /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    inflateEnd(&strm);

    return OK;
}


/* report a zlib or i/o error */
void zerr(int ret) {
    switch (ret) {
    case Z_ERRNO:
        LOGE("zlib_inflate: Z_ERRNO");
        break;
    case Z_STREAM_ERROR:
        LOGE("zlib_inflate: Z_STREAM_ERROR");
        break;
    case Z_DATA_ERROR:
        LOGE("zlib_inflate: Z_DATA_ERROR");
        break;
    case Z_MEM_ERROR:
        LOGE("zlib_inflate: Z_MEM_ERROR");
        break;
    case Z_VERSION_ERROR:
        LOGE("zlib_inflate: Z_VERSION_ERROR");
        break;
    default:
        LOGE("zlib_inflate: unhandled err: %d", ret);
        break;
    }
}


Status
computeDeltaListCount(
    const std::vector<uint8_t> &deltaList,
    uint32_t *acc) {

    std::vector<std::array<uint8_t, 2> > parts;

    Status ret = partitionInto<2>(deltaList, parts);

    if (ret != OK) {
        return ret;
    }

    std::vector<std::vector<std::array<uint8_t, 2> > > split;

    //
    // split into lists where first is {0, ...}
    //
    splitAt(parts.cbegin(), parts.cend(), split,
        [](std::array<uint8_t, 2> x) {
            return x[0] == 0;
        }
    );

    for (const auto &s : split) {

        if (s[0][0] == 0) {

            CHECK(s.size() == 2, "unhandled");

            //
            // parses s[0][1] and s[1][0] as a single short
            //
            *acc += parseLE2(&s[0][1]);

        } else {

            ASSERT(s.size() == 1);

            *acc += s[0][0];
        }
    }

    return OK;
}


std::array<uint8_t, 2> toDigitsBE(uint16_t value) {

    std::array<uint8_t, 2> arr{};

    arr[1] = value & 0xffu;
    value >>= 8;
    arr[0] = value & 0xffu;

    return arr;
}


std::array<uint8_t, 4> toDigitsBE(uint32_t value) {

    std::array<uint8_t, 4> arr{};

    arr[3] = value & 0xffu;
    value >>= 8;
    arr[2] = value & 0xffu;
    value >>= 8;
    arr[1] = value & 0xffu;
    value >>= 8;
    arr[0] = value & 0xffu;

    return arr;
}


std::string fromPascal1String(const char *data) {

    uint8_t len = static_cast<uint8_t>(*(data + 0));

    std::vector<char> cstrData(data + 1, data + 1 + len + 1);

    cstrData[len] = '\0';

    return { cstrData.data() };
}

std::string fromPascal2String(const char *data) {

    uint16_t len = parseLE2(reinterpret_cast<const uint8_t *>(data));

    std::vector<char> cstrData(data + 2, data + 2 + len + 1);

    cstrData[len] = '\0';

    return { cstrData.data() };
}
















