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

#pragma once

#if __ANDROID__
#include <android/log.h>
#endif

#define ABORT(msg, ...) \
   do { \
      LOGE(msg __VA_OPT__(,) __VA_ARGS__); \
      *((volatile char*)0) = 'a'; \
   } while (0)

#define ASSERT(cond) \
   do { \
      if (!(cond)) { \
         ABORT("ASSERTION FAILED: %s %s:%d", #cond, __FILE__, __LINE__); \
      } \
   } while (0)

#if __ANDROID__

#define LOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, TAG, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN, TAG, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, TAG, fmt __VA_OPT__(,) __VA_ARGS__)

#ifdef NDEBUG
#define LOGD(fmt, ...)
#define LOGV(fmt, ...)
#else
#define LOGD(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, TAG, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOGV(fmt, ...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, fmt __VA_OPT__(,) __VA_ARGS__)
#endif

#else // __ANDROID__

#define LOGE(fmt, ...) fprintf(stderr, fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#define LOGW(fmt, ...) fprintf(stderr, fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#define LOGI(fmt, ...) fprintf(stderr, fmt "\n" __VA_OPT__(,) __VA_ARGS__)

#ifdef NDEBUG
#define LOGD(fmt, ...)
#define LOGV(fmt, ...)
#else
#define LOGD(fmt, ...) fprintf(stderr, fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#define LOGV(fmt, ...) fprintf(stderr, fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#endif

#endif // __ANDROID__







