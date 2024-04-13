# Copyright (C) 2024 by Brenton Bostick
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

set(CPP_EXE_TBT_CONVERTER_SOURCES
    tbt-converter.cpp
)

set(CPP_EXE_MIDI_INFO_SOURCES
    midi-info.cpp
)

add_executable(tbt-converter-exe
    ${CPP_EXE_TBT_CONVERTER_SOURCES}
)

add_executable(midi-info-exe
    ${CPP_EXE_MIDI_INFO_SOURCES}
)

target_link_libraries(tbt-converter-exe
    PRIVATE
        tbt-parser-static-lib
)

target_link_libraries(midi-info-exe
    PRIVATE
        tbt-parser-static-lib
)

target_include_directories(tbt-converter-exe
    PRIVATE
        ../include/
)

target_include_directories(midi-info-exe
    PRIVATE
        ../include/
)

set_target_properties(tbt-converter-exe
    PROPERTIES
        OUTPUT_NAME tbt-converter
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
)

set_target_properties(midi-info-exe
    PROPERTIES
        OUTPUT_NAME midi-info
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
)

#
# Setup warnings
#
# https://www.foonathan.net/2018/10/cmake-warnings/
#
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
target_compile_options(tbt-converter-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
target_compile_options(midi-info-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
target_compile_options(tbt-converter-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
target_compile_options(midi-info-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
target_compile_options(tbt-converter-exe PRIVATE
    #
    # Zc:preprocessor is needed for handling __VA_OPT__(,)
    #
    /Zc:preprocessor /WX /W4
)
target_compile_options(midi-info-exe PRIVATE
    #
    # Zc:preprocessor is needed for handling __VA_OPT__(,)
    #
    /Zc:preprocessor /WX /W4
)
endif()



add_test(
    NAME
        tbt-converter-exe-test
    COMMAND
        $<TARGET_FILE:tbt-converter-exe>
)













