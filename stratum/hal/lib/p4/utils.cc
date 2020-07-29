// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// Implementation of p4_utils functions.

#include "stratum/hal/lib/p4/utils.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "absl/strings/substitute.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/lib/macros.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {

// Decodes a P4 object ID into a human-readable form.  The high-order byte of
// the 32-bit ID is a resource type, as specified by the P4Ids::Prefix enum.
std::string PrintP4ObjectID(int object_id) {
  int base_id = object_id & 0xffffff;
  ::p4::config::v1::P4Ids::Prefix resource_type =
      static_cast<::p4::config::v1::P4Ids::Prefix>((object_id >> 24) & 0xff);
  std::string resource_name =
      ::p4::config::v1::P4Ids::Prefix_Name(resource_type);
  if (resource_name.empty()) resource_name = "INVALID";
  return absl::StrFormat("%s/0x%x (0x%x)", resource_name.c_str(), base_id,
                         object_id);
}

// This unnamed namespace hides a function that forms a status string to refer
// to a P4 object.
namespace {

std::string AddP4ObjectReferenceString(const std::string& log_p4_object) {
  std::string referenced_object;
  if (!log_p4_object.empty()) {
    referenced_object =
        absl::Substitute(" referenced by P4 object $0", log_p4_object.c_str());
  }
  return referenced_object;
}

}  // namespace

::util::StatusOr<const P4TableMapValue*> GetTableMapValueWithDescriptorCase(
    const P4PipelineConfig& p4_pipeline_config,
    const std::string& table_map_key,
    P4TableMapValue::DescriptorCase descriptor_case,
    const std::string& log_p4_object) {
  auto* map_value =
      gtl::FindOrNull(p4_pipeline_config.table_map(), table_map_key);
  if (map_value != nullptr) {
    if (map_value->descriptor_case() != descriptor_case) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "P4PipelineConfig descriptor for " << table_map_key
             << AddP4ObjectReferenceString(log_p4_object)
             << " does not have the expected descriptor case: "
             << map_value->ShortDebugString();
    }
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "P4PipelineConfig table map has no descriptor for "
           << table_map_key << AddP4ObjectReferenceString(log_p4_object);
  }

  return map_value;
}

bool IsDontCareMatch(const ::p4::v1::FieldMatch::Exact& exact) { return false; }

bool IsDontCareMatch(const ::p4::v1::FieldMatch::LPM& lpm) {
  return lpm.prefix_len() == 0;
}

bool IsDontCareMatch(const ::p4::v1::FieldMatch::Ternary& ternary) {
  return std::all_of(ternary.mask().begin(), ternary.mask().end(),
                     [](const char c) { return c == '\x00'; });
}

bool IsDontCareMatch(const ::p4::v1::FieldMatch::Range& range,
                     int field_width) {
  if (range.high().size() * 8 < field_width) {
    return false;
  }
  auto it = range.high().rbegin();
  while (it != range.high().rend() && field_width > 0) {
    int cmp = (1u << std::min(field_width, 8)) - 1;
    int v = static_cast<uint8_t>(*it);
    if (v != cmp) {
      return false;
    }
    field_width -= 8;
    ++it;
  }

  return std::all_of(range.low().begin(), range.low().end(),
                     [](const char c) { return c == '\x00'; });
}

bool IsDontCareMatch(const ::p4::v1::FieldMatch::Optional& optional) {
  return false;
}

}  // namespace hal
}  // namespace stratum
