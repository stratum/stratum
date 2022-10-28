// Copyright 2020-present Open Networking Foundation
// Copyright 2021 Google LLC
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/tdi/utils.h"

#include <algorithm>
#include <utility>

#include "stratum/hal/lib/tdi/tdi_constants.h"
#include "stratum/lib/macros.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {
namespace tdi {

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

::util::StatusOr<uint64> ConvertPriorityFromP4rtToTdi(int32 priority) {
  CHECK_RETURN_IF_FALSE(priority >= 0);
  CHECK_RETURN_IF_FALSE(priority <= kMaxPriority);
  return kMaxPriority - priority;
}

::util::StatusOr<int32> ConvertPriorityFromTdiToP4rt(uint64 priority) {
  CHECK_RETURN_IF_FALSE(priority <= kMaxPriority);
  return static_cast<int32>(kMaxPriority - priority);
}

int NumBitsToNumBytes(int num_bits) {
  return (num_bits + 7) / 8;  // ceil(num_bits/8)
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
