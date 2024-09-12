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

#include "common/assert.h"
#include "common/file.h"
#include "common/logging.h"

#include <string>
#include <cstring>
#include <cstdlib>


#define TAG "tbt-printer"


void printUsage();


int main(int argc, const char *argv[]) {

    LOGI("tbt printer v1.2.0");
    LOGI("Copyright (C) 2024 by Brenton Bostick");

    if (argc == 1) {
        printUsage();
        return EXIT_SUCCESS;
    }

    std::string inputFile;
    std::string outputFile;

    for (int i = 0; i < argc; i++) {

        if (strcmp(argv[i], "--input-file") == 0) {

            if (i == argc - 1) {
                printUsage();
                return EXIT_FAILURE;
            }

            i++;

            inputFile = argv[i];

        } else if (strcmp(argv[i], "--output-file") == 0) {

            if (i == argc - 1) {
                printUsage();
                return EXIT_FAILURE;
            }

            i++;

            outputFile = argv[i];
        }
    }

    if (inputFile.empty()) {
        LOGE("input file is missing (or --input-file is not specified)");
        return EXIT_FAILURE;
    }

    if (outputFile.empty()) {
        outputFile = "out.txt";
    }

    LOGI("input file: %s", inputFile.c_str());
    LOGI("output file: %s", outputFile.c_str());


    tbt_file t;

    LOGI("parsing...");

    Status ret = parseTbtFile(inputFile.c_str(), t);

    if (ret != OK) {
        return ret;
    }

    LOGI("printing...");

    auto tab = tbtFileTablature(t);

    auto buf = std::vector<uint8_t>(tab.begin(), tab.end());

    ret = saveFile(outputFile.c_str(), buf);

    if (ret != OK) {
        return ret;
    }

    LOGI("finished!");

    return EXIT_SUCCESS;
}


void printUsage() {
    LOGI("usage: tbt-printer --input-file XXX [--output-file YYY]");
    LOGI();
}






