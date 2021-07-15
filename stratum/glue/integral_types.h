// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
// Adopted from https://github.com/google/google-api-cpp-client
// --> src/googleapis/base/integral_types.h

// TODO(unknow) Move Stratum over to _t types and remove this file

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

#ifdef COMPILER_MSVC /* if Visual C++ */

// VC++ long long suffixes
#define GG_LONGLONG(x) x##I64
#define GG_ULONGLONG(x) x##UI64

// Length modifier in printf format string for int64's (e.g. within %d)
#define GG_LL_FORMAT "I64"  // As in printf("%I64d", ...)
#define GG_LL_FORMAT_W L"I64"

#else /* not Visual C++ */

#define GG_LONGLONG(x) x##LL
#define GG_ULONGLONG(x) x##ULL
#define GG_LL_FORMAT "ll"  // As in "%lld". Note that "q" is poor form also.
#define GG_LL_FORMAT_W L"ll"

#endif  // COMPILER_MSVC

static const uint8 kuint8max = ((uint8)0xFF);
static const uint16 kuint16max = static_cast<uint16>(0xFFFF);
static const uint32 kuint32max = static_cast<uint64>(0xFFFFFFFF);
static const uint64 kuint64max =
    static_cast<uint64>(GG_LONGLONG(0xFFFFFFFFFFFFFFFF));
static const int8 kint8min = ((int8)~0x7F);
static const int8 kint8max = ((int8)0x7F);
static const int16 kint16min = static_cast<int16>(~0x7FFF);
static const int16 kint16max = static_cast<int16>(0x7FFF);
static const int32 kint32min = static_cast<int32>(~0x7FFFFFFF);
static const int32 kint32max = static_cast<int32>(0x7FFFFFFF);
static const int64 kint64min =
    static_cast<int64>(GG_LONGLONG(~0x7FFFFFFFFFFFFFFF));
static const int64 kint64max =
    static_cast<int64>(GG_LONGLONG(0x7FFFFFFFFFFFFFFF));

}  // namespace stratum

#endif  // STRATUM_GLUE_INTEGRAL_TYPES_H_
