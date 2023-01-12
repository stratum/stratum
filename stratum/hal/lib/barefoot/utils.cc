// Copyright 2020-present Open Networking Foundation
// Copyright 2021 Google LLC
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/utils.h"

#include <algorithm>
#include <utility>

#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/p4/utils.h"
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

// For BFRT we explicitly insert the "don't care" range match as the
// [minimum, maximum] value range.
// TODO(max): why are we not stripping the high bytes too?
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Range& range,
                     int field_width) {
  return ByteStringToP4RuntimeByteString(range.low()) ==
             ByteStringToP4RuntimeByteString(RangeDefaultLow(field_width)) &&
         range.high() == RangeDefaultHigh(field_width);
}

bool IsDontCareMatch(const ::p4::v1::FieldMatch::Optional& optional) {
  return false;
}

std::string RangeDefaultLow(size_t bitwidth) {
  return std::string(NumBitsToNumBytes(bitwidth), '\x00');
}

std::string RangeDefaultHigh(size_t bitwidth) {
  return AllOnesByteString(bitwidth);
}

::util::StatusOr<uint64> ConvertPriorityFromP4rtToBfrt(int32 priority) {
  RET_CHECK(priority >= 0);
  RET_CHECK(priority <= kMaxPriority);
  return kMaxPriority - priority;
}

::util::StatusOr<int32> ConvertPriorityFromBfrtToP4rt(uint64 priority) {
  RET_CHECK(priority <= kMaxPriority);
  return static_cast<int32>(kMaxPriority - priority);
}

int NumBitsToNumBytes(int num_bits) {
  return (num_bits + 7) / 8;  // ceil(num_bits/8)
}

std::string AllOnesByteString(size_t bitwidth) {
  const size_t nbytes = NumBitsToNumBytes(bitwidth);
  std::string value(nbytes, '\xff');
  size_t zero_nbits = (nbytes * 8) - bitwidth;
  char mask = 0xff >> zero_nbits;
  value[0] &= mask;
  return value;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
