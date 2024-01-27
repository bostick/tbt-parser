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

#include "tbt-parser/tbt-parser-util.h"

#include "common/assert.h"
#include "common/logging.h"

#include <string>
#include <cstring>
#include <cstdlib>


int main(int argc, const char *argv[]) {

	LOGI("tbt converter v1.0.1");
	LOGI("Copyright (C) 2023 by Brenton Bostick");

	if (argc == 1) {
		LOGI("usage: tbt-converter --input-file XXX [--output-file YYY]");
		return EXIT_SUCCESS;
	}

	std::string inputFile;
	std::string outputFile;

	for (int i = 0; i < argc; i++) {

		if (strcmp(argv[i], "--input-file") == 0) {

			i++;

			inputFile = argv[i];

		} else if (strcmp(argv[i], "--output-file") == 0) {

			i++;

			outputFile = argv[i];
		}
	}

	if (inputFile.empty()) {
		LOGE("input file is missing");
		return EXIT_FAILURE;
	}

	if (outputFile.empty()) {
		outputFile = "out.mid";
	}

	LOGI("input file: %s", inputFile.c_str());
	LOGI("output file: %s", outputFile.c_str());


	tbt_file t;

	LOGI("parsing...");

	Status ret = parseTbtFile(inputFile.c_str(), t);

	if (ret != OK) {
		return ret;
	}

	auto header = tbtFileHeader(t);

	std::string versionString;
    if (header[6] == 3) {
        versionString += static_cast<char>(header[7]);
        versionString += static_cast<char>(header[8]);
        versionString += static_cast<char>(header[9]);
    } else {
        ASSERT(header[6] == 4);
        versionString += static_cast<char>(header[7]);
        versionString += static_cast<char>(header[8]);
        versionString += static_cast<char>(header[9]);
        versionString += static_cast<char>(header[10]);
    }

    LOGI("tbt file version: %s (0x%02x)", versionString.c_str(), header[3]);

    switch (header[3]) {
    case 0x72:
    case 0x71: {

        auto t71 = *reinterpret_cast<tbt_file71 *>(t);
        
        LOGI("title: %s", fromPascal2String(t71.metadata.title).data());

        LOGI("artist: %s", fromPascal2String(t71.metadata.artist).data());

        LOGI("album: %s", fromPascal2String(t71.metadata.album).data());

        LOGI("transcribed by: %s", fromPascal2String(t71.metadata.transcribedBy).data());

        // LOGI("%s", fromPascal2String(t71.metadata.comment).data());
        
        break;
    }
    case 0x70: {
        
        auto t70 = *reinterpret_cast<tbt_file70 *>(t);
        
        LOGI("title: %s", fromPascal2String(t70.metadata.title).data());

        LOGI("artist: %s", fromPascal2String(t70.metadata.artist).data());

        LOGI("album: %s", fromPascal2String(t70.metadata.album).data());

        LOGI("transcribed by: %s", fromPascal2String(t70.metadata.transcribedBy).data());

        // LOGI("%s", fromPascal2String(t70.metadata.comment).data());
        
        break;
    }
    case 0x6f:
    case 0x6e: {
        
        auto t6e = *reinterpret_cast<tbt_file6e *>(t);
        
        LOGI("title: %s", fromPascal2String(t6e.metadata.title).data());

        LOGI("artist: %s", fromPascal2String(t6e.metadata.artist).data());

        LOGI("album: %s", fromPascal2String(t6e.metadata.album).data());

        LOGI("transcribed by: %s", fromPascal2String(t6e.metadata.transcribedBy).data());

        // LOGI("%s", fromPascal2String(t6e.metadata.comment).data());
        
        break;
    }
    case 0x6b: {
        
        auto t6b = *reinterpret_cast<tbt_file6b *>(t);
        
        LOGI("title: %s", fromPascal1String(t6b.metadata.title).data());

        LOGI("artist: %s", fromPascal1String(t6b.metadata.artist).data());

        // LOGI("%s", fromPascal1String(t6b.metadata.comment).data());
        
        break;
    }
    case 0x6a: {
        
        auto t6a = *reinterpret_cast<tbt_file6a *>(t);
        
        LOGI("title: %s", fromPascal1String(t6a.metadata.title).data());

        LOGI("artist: %s", fromPascal1String(t6a.metadata.artist).data());

        // LOGI("%s", fromPascal1String(t6a.metadata.comment).data());
        
        break;
    }
    case 0x69:
    case 0x68:
    case 0x67:
    case 0x66:
    case 0x65: {
        
        auto t65 = *reinterpret_cast<tbt_file65 *>(t);
        
        LOGI("title: %s", fromPascal1String(t65.metadata.title).data());

        LOGI("artist: %s", fromPascal1String(t65.metadata.artist).data());

        // LOGI("%s", fromPascal1String(t65.metadata.comment).data());
        
        break;
    }
    default:
        ASSERT(false);
        break;
    }

    LOGI("exporting...");

	ret = exportMidiFile(t, outputFile.c_str());

	if (ret != OK) {
		return ret;
	}

	LOGI("finished!");

	releaseTbtFile(t);

    return EXIT_SUCCESS;
}







