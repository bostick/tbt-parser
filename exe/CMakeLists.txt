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

set(CPP_EXE_CONVERTER_SOURCES
    tbt-converter.cpp
)

add_executable(tbt-converter-exe
    ${CPP_EXE_CONVERTER_SOURCES}
)

target_link_libraries(tbt-converter-exe
    PRIVATE
        tbt-parser-lib
)

target_include_directories(tbt-converter-exe
    PRIVATE
        ../include/
)

set_target_properties(tbt-converter-exe
    PROPERTIES
        OUTPUT_NAME tbt-converter
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
)


