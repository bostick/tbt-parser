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

cmake_minimum_required(VERSION 3.14)

cmake_policy(SET CMP0144 NEW)


find_package(GTest REQUIRED)


set(CPP_TEST_SOURCES
	TestMidi.cpp
	TestRational.cpp
    TestTbt.cpp
    TestUtil.cpp
)

add_executable(tbt-test-exe
	${CPP_TEST_SOURCES}
)

target_link_libraries(tbt-test-exe
	PRIVATE
		tbt-parser-static-lib
		${GTEST_BOTH_LIBRARIES}
)

target_include_directories(tbt-test-exe
	PRIVATE
		${GTEST_INCLUDE_DIRS}
		../include/
		../lib/
		${COMMON_DIR}/include
)

target_compile_options(tbt-test-exe PRIVATE
    -Wall -Wextra -pedantic -Werror
)

set_target_properties(tbt-test-exe
	PROPERTIES
		OUTPUT_NAME tbt-test
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
)








