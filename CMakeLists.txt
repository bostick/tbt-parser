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

cmake_minimum_required(VERSION 3.22.1)

project(tbt-parser
	LANGUAGES CXX
)


set(TBTPARSER_BUILD_EXE ON CACHE BOOL "Build exe")
set(TBTPARSER_BUILD_TESTS OFF CACHE BOOL "Build tests")

message(STATUS "TBTPARSER_BUILD_EXE: ${TBTPARSER_BUILD_EXE}")
message(STATUS "TBTPARSER_BUILD_TESTS: ${TBTPARSER_BUILD_TESTS}")


#
# exe adds some tests, so enable testing before adding exe
#
if(TBTPARSER_BUILD_TESTS)

enable_testing()

endif()


add_subdirectory(lib)


if(TBTPARSER_BUILD_EXE)

add_subdirectory(exe)

endif ()


if(TBTPARSER_BUILD_TESTS)

add_subdirectory(test)

endif()












