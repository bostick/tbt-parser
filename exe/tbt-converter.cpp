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

#include "common/common.h"

#include <string>


int main(int argc, const char *argv[]) {

	LOGI("tbt converter v1.0.0");
	LOGI("Copyright (C) 2023 by Brenton Bostick");

	if (argc == 1) {
		LOGI("usage: tbt-converter --input-file XXX [--output-file YYY]");
		exit(0);
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
		exit(1);
	}

	if (outputFile.empty()) {
		outputFile = "out.mid";
	}

	LOGI("input file: %s", inputFile.c_str());
	LOGI("output file: %s", outputFile.c_str());


	tbt_file t;

	LOGI("parsing...");

	Status ret = parseTbtFile(inputFile.c_str(), &t);

	if (ret != OK) {
		return ret;
	}

	LOGI("tbt file version: 0x%x", t.header.versionNumber);

    if (0x6d <= t.header.versionNumber) {

        LOGI("title: %s", fromPascal2String(t.metadata.title).data());

        LOGI("artist: %s", fromPascal2String(t.metadata.artist).data());

        LOGI("album: %s", fromPascal2String(t.metadata.album).data());

        LOGI("transcribed by: %s", fromPascal2String(t.metadata.transcribedBy).data());

        // LOGI("%s", fromPascal2String(t.metadata.comment).data());

    } else {

        LOGI("title: %s", fromPascal2String(t.metadata.title).data());

        LOGI("artist: %s", fromPascal2String(t.metadata.artist).data());

        // LOGI("%s", fromPascal2String(t.metadata.comment).data());
    }

    LOGI("exporting...");

	ret = exportMidiFile(t, outputFile.c_str());

	if (ret != OK) {
		return ret;
	}

	LOGI("finished!");

    return 0;
}







