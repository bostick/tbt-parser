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

#include "common/check.h"
#include "common/logging.h"

#include <string>
#include <map>
#include <cstdlib>
#include <cstring> // for strcmp
#include <cinttypes>


#define TAG "midi-info"


void printUsage();


int main(int argc, const char *argv[]) {

    LOGI("midi info v1.3.0");
    LOGI("Copyright (C) 2024 by Brenton Bostick");

    if (argc == 1) {
        printUsage();
        return EXIT_SUCCESS;
    }

    std::string inputFile;

    for (int i = 0; i < argc; i++) {

        if (std::strcmp(argv[i], "--input-file") == 0) {

            if (i == argc - 1) {
                printUsage();
                return EXIT_FAILURE;
            }

            i++;

            inputFile = argv[i];
        }
    }

    if (inputFile.empty()) {
        LOGE("input file is missing (or --input-file is not specified)");
        return EXIT_FAILURE;
    }

    LOGI("input file: %s", inputFile.c_str());


    midi_file m;

    Status ret = parseMidiFile(inputFile.c_str(), m);

    if (ret != OK) {
        return ret;
    }

    auto info = midiFileInfo(m);

    LOGI("%s", info.c_str());
    
    return EXIT_SUCCESS;
}


void printUsage() {
    LOGI("usage: midi-info --input-file XXX");
    LOGI();
}






