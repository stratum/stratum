/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Adopted from https://github.com/google/google-api-cpp-client
// --> src/googleapis/base/integral_types.h

//TODO Move Stratum over to _t types and remove this file

#ifndef STRATUM_GLUE_INTEGRAL_TYPES_H_
#define STRATUM_GLUE_INTEGRAL_TYPES_H_

#include <stdint.h>

namespace stratum {
    typedef int8_t int8;
    typedef int16_t int16;
    typedef int32_t int32;
    typedef int64_t int64;

    typedef uint8_t uint8;
    typedef uint16_t uint16;
    typedef uint32_t uint32;
    typedef uint64_t uint64;

    // Prefer the stdint.h types instead of these:
//    typedef signed char int8;
//    typedef short int16;
//    typedef int int32;
//    typedef long long int64;
//
//    typedef unsigned char uint8;
//    typedef unsigned short uint16;
//    typedef unsigned int uint32;
//    typedef unsigned long long uint64;

    // long long macros to be used because gcc and vc++ use different suffixes,
    // and different size specifiers in format strings
    #undef GG_LONGLONG
    #undef GG_ULONGLONG
    #undef GG_LL_FORMAT

    #ifdef COMPILER_MSVC     /* if Visual C++ */

    // VC++ long long suffixes
    #define GG_LONGLONG(x) x##I64
    #define GG_ULONGLONG(x) x##UI64

    // Length modifier in printf format string for int64's (e.g. within %d)
    #define GG_LL_FORMAT "I64"  // As in printf("%I64d", ...)
    #define GG_LL_FORMAT_W L"I64"

    #else   /* not Visual C++ */

    #define GG_LONGLONG(x) x##LL
    #define GG_ULONGLONG(x) x##ULL
    #define GG_LL_FORMAT "ll"  // As in "%lld". Note that "q" is poor form also.
    #define GG_LL_FORMAT_W L"ll"

    #endif  // COMPILER_MSVC

    static const uint8  kuint8max  = (( uint8) 0xFF);
    static const uint16 kuint16max = ((uint16) 0xFFFF);
    static const uint32 kuint32max = ((uint32) 0xFFFFFFFF);
    static const uint64 kuint64max = ((uint64) GG_LONGLONG(0xFFFFFFFFFFFFFFFF));
    static const  int8  kint8min   = ((  int8) ~0x7F);
    static const  int8  kint8max   = ((  int8) 0x7F);
    static const  int16 kint16min  = (( int16) ~0x7FFF);
    static const  int16 kint16max  = (( int16) 0x7FFF);
    static const  int32 kint32min  = (( int32) ~0x7FFFFFFF);
    static const  int32 kint32max  = (( int32) 0x7FFFFFFF);
    static const  int64 kint64min  = (( int64) GG_LONGLONG(~0x7FFFFFFFFFFFFFFF));
    static const  int64 kint64max  = (( int64) GG_LONGLONG(0x7FFFFFFFFFFFFFFF));

}

#endif  // STRATUM_GLUE_INTEGRAL_TYPES_H_