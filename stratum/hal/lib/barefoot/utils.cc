// Copyright 2020-present Open Networking Foundation
// Copyright 2021 Google LLC
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/utils.h"

#include <algorithm>
#include <utility>

#include "stratum/glue/gtl/stl_util.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/lib/macros.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {
namespace barefoot {

bool IsDontCareMatch(const ::p4::v1::FieldMatch::Exact& exact) { return false; }

bool IsDontCareMatch(const ::p4::v1::FieldMatch::LPM& lpm) {
  return lpm.prefix_len() == 0;
}

bool IsDontCareMatch(const ::p4::v1::FieldMatch::Ternary& ternary) {
  return std::all_of(ternary.mask().begin(), ternary.mask().end(),
                     [](const char c) { return c == '\x00'; });
}

namespace {
// Strip leading zeros from a string, but keep at least one byte.
std::string StripLeadingZeroBytes(std::string str) {
  str.erase(0, std::min(str.find_first_not_of('\x00'), str.size() - 1));
  return str;
}
}  // namespace

// For BFRT we explicitly insert the "don't care" range match as the
// [minimum, maximum] value range.
// TODO(max): why are we not stripping the high bytes too?
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Range& range,
                     int field_width) {
  return StripLeadingZeroBytes(range.low()) ==
             StripLeadingZeroBytes(RangeDefaultLow(field_width)) &&
         range.high() == RangeDefaultHigh(field_width);
}

bool IsDontCareMatch(const ::p4::v1::FieldMatch::Optional& optional) {
  return false;
}

std::string RangeDefaultLow(size_t bitwidth) {
  return std::string(NumBitsToNumBytes(bitwidth), '\x00');
}

std::string RangeDefaultHigh(size_t bitwidth) {
  const size_t nbytes = NumBitsToNumBytes(bitwidth);
  std::string high(nbytes, '\xff');
  size_t zero_nbits = (nbytes * 8) - bitwidth;
  char mask = 0xff >> zero_nbits;
  high[0] &= mask;
  return high;
}

::util::StatusOr<uint64> ConvertPriorityFromP4rtToBfrt(int32 priority) {
  CHECK_RETURN_IF_FALSE(priority >= 0);
  CHECK_RETURN_IF_FALSE(priority <= kMaxPriority);
  return kMaxPriority - priority;
}

::util::StatusOr<int32> ConvertPriorityFromBfrtToP4rt(uint64 priority) {
  CHECK_RETURN_IF_FALSE(priority <= kMaxPriority);
  return static_cast<int32>(kMaxPriority - priority);
}

int NumBitsToNumBytes(int num_bits) {
  return (num_bits + 7) / 8;  // ceil(num_bits/8)
}

::util::StatusOr<std::string> Uint32ToBytes(uint32 value, size_t bit_width) {
  CHECK_RETURN_IF_FALSE(0 < bit_width && bit_width <= 32);
  CHECK_RETURN_IF_FALSE(value <= (UINT32_MAX >> (32 - bit_width)));
  size_t bytes = NumBitsToNumBytes(bit_width);
  std::string ret(bytes, '\x00');
  uint8* byte_array = reinterpret_cast<uint8*>(gtl::string_as_array(&ret));
  for (int i = bytes - 1; i >= 0; i--) {
    byte_array[i] = static_cast<char>(value & 0xff);
    value >>= 8;
  }
  return ret;
}

// Note that this is a protobuf byte array which means the first byte is the
// most significent byte. For example: 0x511 in 4-byte is "\x00\x00\x01\xff"
// instead of
// "\xff\x01\x00\x00".
::util::StatusOr<uint32> BytesToUint32(std::string value) {
  CHECK_RETURN_IF_FALSE(value.size() > 0);
  uint8* byte_array = reinterpret_cast<uint8*>(gtl::string_as_array(&value));

  // Check if the number is smaller than 32-bit unsigned interger of the size of
  // byte array is bigger than 4.
  int bytes_must_be_zero = 0;
  if (value.size() > 4) {
    bytes_must_be_zero = value.size() - 4;
  }
  uint32 result = 0;
  for (size_t i = 0; i < value.size() - 1; i++) {
    result |= byte_array[i];
    result <<= 8;
    if (i < bytes_must_be_zero) {
      CHECK_RETURN_IF_FALSE(byte_array[i] == 0);
    }
  }
  result += byte_array[value.size() - 1];
  return result;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
