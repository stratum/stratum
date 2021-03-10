// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/utils.h"

#include <utility>

#include "absl/strings/strip.h"
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
absl::string_view StripLeadingZeroBytes(const std::string& str) {
  absl::string_view normalized(str);
  while (normalized.size() > 1 && absl::StartsWith(normalized, "\x00")) {
    normalized.remove_prefix(1);
  }
  return normalized;
}
}  // namespace

// For BFRT we explicitly insert the "don't care" range match as the
// [minimum, maximum] value range.
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Range& range,
                     int field_width) {
  // Ignore leading zero bytes.
  absl::string_view normalized_low(range.low());
  while (normalized_low.size() > 1 &&
         absl::StartsWith(normalized_low, "\x00")) {
    normalized_low.remove_prefix(1);
  }

  return normalized_low ==
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

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
