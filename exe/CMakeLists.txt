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

if(NOT DEFINED COMMON_DIR)
  message(FATAL_ERROR "COMMON_DIR is not defined")
endif()


add_executable(tbt-converter-exe
    tbt-converter.cpp
)

add_executable(midi-info-exe
    midi-info.cpp
)

add_executable(tbt-printer-exe
    tbt-printer.cpp
)

add_executable(tbt-info-exe
    tbt-info.cpp
)

target_link_libraries(tbt-converter-exe
    PRIVATE
        tbt-parser-static-lib
)

target_link_libraries(midi-info-exe
    PRIVATE
        tbt-parser-static-lib
)

target_link_libraries(tbt-printer-exe
    PRIVATE
        tbt-parser-static-lib
)

target_link_libraries(tbt-info-exe
    PRIVATE
        tbt-parser-static-lib
)

target_include_directories(tbt-converter-exe
    PRIVATE
        ../include/
        ${COMMON_DIR}/include/
)

target_include_directories(midi-info-exe
    PRIVATE
        ../include/
        ${COMMON_DIR}/include/
)

target_include_directories(tbt-printer-exe
    PRIVATE
        ../include/
        ${COMMON_DIR}/include/
)

target_include_directories(tbt-info-exe
    PRIVATE
        ../include/
        ${COMMON_DIR}/include/
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

set_target_properties(tbt-printer-exe
    PROPERTIES
        OUTPUT_NAME tbt-printer
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
)

set_target_properties(tbt-info-exe
    PROPERTIES
        OUTPUT_NAME tbt-info
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
target_compile_options(tbt-printer-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
target_compile_options(tbt-info-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
target_compile_options(tbt-converter-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
target_compile_options(midi-info-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
target_compile_options(tbt-printer-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
target_compile_options(tbt-info-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
target_compile_options(tbt-converter-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
target_compile_options(midi-info-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
target_compile_options(tbt-printer-exe PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
target_compile_options(tbt-info-exe PRIVATE
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
target_compile_options(tbt-printer-exe PRIVATE
    #
    # Zc:preprocessor is needed for handling __VA_OPT__(,)
    #
    /Zc:preprocessor /WX /W4
)
target_compile_options(tbt-info-exe PRIVATE
    #
    # Zc:preprocessor is needed for handling __VA_OPT__(,)
    #
    /Zc:preprocessor /WX /W4
)
else ()
message(FATAL_ERROR "Unrecognized compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()



add_test(
    NAME
        tbt-converter-exe-black-test
    COMMAND
        $<TARGET_FILE:tbt-converter-exe> --input-file ../../test/data/black.tbt
)


add_test(
    NAME
        midi-info-exe-black-test
    COMMAND
        $<TARGET_FILE:midi-info-exe> --input-file ../../test/data/black.mid
)


add_test(
    NAME
        tbt-printer-exe-black-test
    COMMAND
        $<TARGET_FILE:tbt-printer-exe> --input-file ../../test/data/black.tbt
)


add_test(
    NAME
        tbt-info-exe-black-test
    COMMAND
        $<TARGET_FILE:tbt-info-exe> --input-file ../../test/data/black.tbt
)
















