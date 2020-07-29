// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// This file declares some utility functions for P4 objects.

#ifndef STRATUM_HAL_LIB_P4_UTILS_H_
#define STRATUM_HAL_LIB_P4_UTILS_H_

#include <string>

#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"

namespace stratum {
namespace hal {

// Decodes a P4 object ID into a human-readable form.
std::string PrintP4ObjectID(int object_id);

// Attempts to find a P4TableMapValue in p4_pipeline_config with the given
// table_map_key.  If an entry for the key is present, the entry's oneof
// descriptor is compared with descriptor_case.  The return status is a
// P4TableMapValue pointer if an entry with table_map_key exists and the
// entry matches the descriptor_case.  Otherwise, the return status is non-OK.
// The log_p4_object is a string that GetTableMapValueWithDescriptorCase
// optionally inserts into the error status message when non-empty.  For
// example, if the caller is looking for a match field's field descriptor,
// then the caller can provide the table name associated with the match
// field in log_p4_object.
::util::StatusOr<const P4TableMapValue*> GetTableMapValueWithDescriptorCase(
    const P4PipelineConfig& p4_pipeline_config,
    const std::string& table_map_key,
    P4TableMapValue::DescriptorCase descriptor_case,
    const std::string& log_p4_object);

// A set of helper functions to determine whether a P4 match object contains a
// "don't care" match.
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Exact& exact);
bool IsDontCareMatch(const ::p4::v1::FieldMatch::LPM& lpm);
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Ternary& ternary);
// The field width is only taken as a upper bound, byte strings longer than that
// are not checked.
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Range& range, int field_width);
// If the Optional match should be a wildcard, the FieldMatch must be omitted.
// Otherwise, this behaves like an exact match.
bool IsDontCareMatch(const ::p4::v1::FieldMatch::Optional& optional);

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_UTILS_H_
