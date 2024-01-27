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

#define _CRT_SECURE_NO_DEPRECATE // disable warnings about fopen being insecure on MSVC

#include "common/file.h"

#include "common/logging.h"

#include <cstdio>


Status
openFile(
    const char *path,
    std::vector<uint8_t> &out) {
    
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

    out = std::vector<uint8_t>(len);

    size_t r = fread(out.data(), sizeof(uint8_t), len, file);

    if (r != len) {

        LOGE("fread failed");

        return ERR;
    }

    fclose(file);

    return OK;
}


Status
saveFile(
    const char *path,
    const std::vector<uint8_t> buf) {

    FILE *file = fopen(path, "wb");

    if (!file) {

        LOGE("cannot open %s\n", path);

        return ERR;
    }

    auto r = fwrite(buf.data(), sizeof(uint8_t), buf.size(), file);

    if (r != buf.size()) {

        LOGE("fwrite failed");

        return ERR;
    }

    fclose(file);

    return OK;
}







