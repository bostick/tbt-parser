# Copyright (C) 2023 by Brenton Bostick
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
# associated documentation files (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do
# so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all copies or substantial
# portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
# OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.5)

find_package(ZLIB REQUIRED)

set(CPP_LIB_SOURCES
    alternate-time-regions.cpp
    bars.cpp
    body.cpp
    metadata.cpp
    midi.cpp
    notes.cpp
    tbt-file.cpp
    tbt-parser-util.cpp
    track-effect-changes.cpp
)

add_library(tbt-parser-lib SHARED
    ${CPP_LIB_SOURCES}
)

target_include_directories(tbt-parser-lib PRIVATE
    ../include/
)

target_link_libraries(tbt-parser-lib PRIVATE
    ZLIB::ZLIB
)

set_target_properties(tbt-parser-lib
    PROPERTIES
        OUTPUT_NAME tbt-parser
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
)








