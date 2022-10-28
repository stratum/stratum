// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_UTILS_H_
#define STRATUM_HAL_LIB_TDI_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/tdi/tdi.pb.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace tdi {

// A set of helper functions to determine whether a P4 match object constructed
// from a bfrt table key is a "don't care" match.
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Exact& exact);
bool IsDontCareMatch(const ::p4::v1::FieldMatch::LPM& lpm);
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Ternary& ternary);
// The field width is only taken as a upper bound, byte strings longer than that
// are not checked.
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Range& range, int field_width);
// If the Optional match should be a wildcard, the FieldMatch must be omitted.
// Otherwise, this behaves like an exact match.
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Optional& optional);

// Returns the "don't care" match values for a range type match field.
// Values are padded to the full bit width, as expected by the SDE.
std::string RangeDefaultLow(size_t bitwidth);
std::string RangeDefaultHigh(size_t bitwidth);

// Check and converts priority value from P4Rutime to Tdi, vice versa.
// In P4Runtime, a higher number indicates that the entry must
// be given higher priority, however, in Tdi the lower number means higher
// priority for table lookup.
::util::StatusOr<uint64> ConvertPriorityFromP4rtToTdi(int32 priority);
::util::StatusOr<int32> ConvertPriorityFromTdiToP4rt(uint64 priority);

// Returns the number of bytes needed to encode the given number of bits in a
// byte string.
int NumBitsToNumBytes(int num_bits);

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_UTILS_H_
