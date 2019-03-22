// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file contains the FieldCrossReference implementation.

#include "stratum/p4c_backends/fpm/field_cross_reference.h"

#include <string>

#include "stratum/glue/logging.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/glue/gtl/map_util.h"

namespace stratum {
namespace p4c_backends {

void FieldCrossReference::ProcessAssignments(
    const std::vector<const IR::AssignmentStatement*>& assignments,
    hal::P4PipelineConfig* p4_pipeline_config) {
  VLOG(2) << "Cross referencing " << assignments.size()
          << " P4 program assignments";
  field_xref_map_.clear();

  // This loop creates cross reference map entries for the fields in
  // IR AssignmentStatement nodes.  If the left or right side expression
  // does not have a field descriptor in the P4 table map, then it is
  // probably a temporary variable, constant, enum, method call, or other
  // expression that has no relevant field type information.
  for (auto assign : assignments) {
    const std::string left_key(assign->left->toString());
    hal::P4TableMapValue* destination_field =
        gtl::FindOrNull(*p4_pipeline_config->mutable_table_map(), left_key);
    if (destination_field == nullptr ||
        destination_field->has_header_descriptor()) {
      continue;
    }
    const std::string right_key(assign->right->toString());
    hal::P4TableMapValue* source_field =
        gtl::FindOrNull(*p4_pipeline_config->mutable_table_map(), right_key);
    if (source_field == nullptr || source_field->has_header_descriptor()) {
      continue;
    }

    AddFieldXref(destination_field, source_field);
  }

  UpdateFieldTypes();
}

void FieldCrossReference::AddFieldXref(hal::P4TableMapValue* destination_field,
                                       hal::P4TableMapValue* source_field) {
  if (!destination_field->has_field_descriptor()) {
    LOG(WARNING) << "Expected assignment destination field to be a field "
                 << "descriptor " << destination_field->ShortDebugString();
    return;
  }
  if (!source_field->has_field_descriptor()) {
    LOG(WARNING) << "Expected assignment source field to be a field "
                 << "descriptor " << source_field->ShortDebugString();
    return;
  }

  field_xref_map_[destination_field->mutable_field_descriptor()].insert(
      source_field->mutable_field_descriptor());
}

void FieldCrossReference::UpdateFieldTypes() {
  bool reprocess = false;

  // This loop runs multiple passes over the cross reference map to handle
  // sequences like this:
  //  field1_unknown = field2_unknown;
  //  field2_unknown = field3_type_xyz;
  // This sequence takes two passes.  The first pass assigns field3_type_xyz's
  // type to field2_unknown, and the second pass propagates field2_unknown's
  // new type to field1_unknown.
  do {
    reprocess = false;
    for (auto& iter : field_xref_map_) {
      reprocess |= ProcessXrefEntry(iter.first, iter.second);
    }
  } while (reprocess);
}

bool FieldCrossReference::ProcessXrefEntry(
    hal::P4FieldDescriptor* destination_field,
    const SourceFieldSet& source_field_set) {
  bool type_updated = false;

  // This loop propagates types in either direction if one side of the
  // assignment has a known type.
  for (const auto source_field : source_field_set) {
    bool destination_unknown = IsFieldTypeUnspecified(*destination_field);
    bool source_unknown = IsFieldTypeUnspecified(*source_field);
    if (destination_unknown == source_unknown)
      continue;
    if (destination_unknown) {
      destination_field->set_type(source_field->type());
    } else {
      source_field->set_type(destination_field->type());
    }
    type_updated = true;
  }
  return type_updated;
}

}  // namespace p4c_backends
}  // namespace stratum
