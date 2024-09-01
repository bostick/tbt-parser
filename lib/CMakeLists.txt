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

cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED COMMON_DIR)
  message(FATAL_ERROR "COMMON_DIR is not defined")
endif()

include(FetchContent)
FetchContent_Declare(zlib
    GIT_REPOSITORY
        https://github.com/madler/zlib.git
    GIT_TAG
        v1.3.1
)
set(ZLIB_USE_STATIC_LIBS ON)
set(ZLIB_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(zlib)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
target_compile_options(zlib PRIVATE
    #
    # disable warnings when building zlib in e.g. Xcode
    #
    -Wno-shorten-64-to-32
)
target_compile_options(zlibstatic PRIVATE
    #
    # disable warnings when building zlibstatic in e.g. Xcode
    #
    -Wno-shorten-64-to-32
)
endif()


set(CPP_LIB_SOURCES

    ${COMMON_DIR}/lib/file.cpp
    ${COMMON_DIR}/lib/logging.cpp

    midi.cpp
    rational.cpp
    tbt.cpp
    tbt-parser-util.cpp
    tablature.cpp
)

add_library(tbt-parser-static-lib STATIC
    ${CPP_LIB_SOURCES}
)

target_include_directories(tbt-parser-static-lib PRIVATE
    ../include/
    ${COMMON_DIR}/include/
)

target_link_libraries(tbt-parser-static-lib PRIVATE
    zlibstatic
)

set_target_properties(tbt-parser-static-lib
    PROPERTIES
        OUTPUT_NAME tbt-parser-static
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
)

#
# Setup warnings
#
# https://www.foonathan.net/2018/10/cmake-warnings/
#
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
target_compile_options(tbt-parser-static-lib PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
target_compile_options(tbt-parser-static-lib PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
target_compile_options(tbt-parser-static-lib PRIVATE
    -Wall -Wextra -pedantic -Werror -Wconversion -Wsign-conversion -Wno-unknown-pragmas
)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
target_compile_options(tbt-parser-static-lib PRIVATE
    #
    # Zc:preprocessor is needed for handling __VA_OPT__(,)
    #
    /Zc:preprocessor /WX /W4
)
else ()
message(FATAL_ERROR "Unrecognized compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()








