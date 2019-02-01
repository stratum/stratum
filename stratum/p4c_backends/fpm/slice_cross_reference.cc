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

// This file contains the SliceCrossReference implementation.

#include "stratum/p4c_backends/fpm/slice_cross_reference.h"

#include <string>

#include "base/logging.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "absl/memory/memory.h"
#include "util/gtl/map_util.h"

namespace stratum {
namespace p4c_backends {

SliceCrossReference::SliceCrossReference(const SlicedFieldMap& sliced_field_map,
                                         P4::ReferenceMap* ref_map,
                                         P4::TypeMap* type_map)
    : sliced_field_map_(sliced_field_map) {
  slice_decoder_ = absl::make_unique<ExpressionInspector>(ref_map, type_map);
}

void SliceCrossReference::ProcessAssignments(
    const std::vector<const IR::AssignmentStatement*>& assignments,
    hal::P4PipelineConfig* p4_pipeline_config) {
  VLOG(2) << "Cross referencing slices in " << assignments.size()
          << " P4 program assignments";

  // This loop looks for sliced fields on the right side of assignments.  Upon
  // finding a slice with a known destination field type, it updates the
  // overall type of the sliced field to P4_FIELD_TYPE_SLICED.
  for (auto assign : assignments) {
    if (!assign->right->is<IR::Slice>()) continue;
    if (!slice_decoder_->Inspect(*assign->right)) continue;
    if (slice_decoder_->value().source_value_case() !=
        P4AssignSourceValue::kSourceFieldName) continue;
    const std::string& source_key = slice_decoder_->value().source_field_name();
    hal::P4TableMapValue* source_field =
        gtl::FindOrNull(*p4_pipeline_config->mutable_table_map(), source_key);
    if (source_field == nullptr || source_field->has_header_descriptor()) {
      continue;
    }

    const std::string left_key(assign->left->toString());
    hal::P4TableMapValue* destination_field =
        gtl::FindOrNull(*p4_pipeline_config->mutable_table_map(), left_key);
    if (destination_field == nullptr ||
        destination_field->has_header_descriptor()) {
      continue;
    }

    bool dest_unknown =
        IsFieldTypeUnspecified(destination_field->field_descriptor());
    bool source_unknown =
        IsFieldTypeUnspecified(source_field->field_descriptor());
    if (!dest_unknown && source_unknown) {
      HandleUnknownSourceType(source_field->mutable_field_descriptor());
    } else if (dest_unknown && !source_unknown) {
      if (!HandleUnknownDestType(
              source_field->field_descriptor(),
              destination_field->mutable_field_descriptor())) {
        ::error(
            "Backend: Unable to process sliced assignment from %s - "
            "check for missing slice map file entry",
            assign->right->to<IR::Slice>());
      }
    }
  }
}

// Unknown source field slices aren't particularly interesting to the
// Stratum switch stack, so they get the generic P4_FIELD_TYPE_SLICED
// to distinguish them from completely unknown fields.
void SliceCrossReference::HandleUnknownSourceType(
    hal::P4FieldDescriptor* source_field) {
  source_field->set_type(P4_FIELD_TYPE_SLICED);
}

// Unknown destination fields assigned from a slice of a known field type
// need to be updated with more useful information from the slice.
bool SliceCrossReference::HandleUnknownDestType(
    const hal::P4FieldDescriptor& source_field,
    hal::P4FieldDescriptor* dest_field) {
  const SliceMapValue* slice_map_value =
      gtl::FindOrNull(sliced_field_map_.sliced_field_map(),
                      P4FieldType_Name(source_field.type()));
  if (slice_map_value == nullptr) return false;

  // For valid slices, the sliced_field_map_ should have a match for this
  // slice's offset and width.
  bool slice_found = false;
  int slice_bit_offset =
      source_field.bit_width() - (slice_decoder_->value().high_bit() + 1);
  for (const auto& slice_properties : slice_map_value->slice_properties()) {
    if (slice_properties.slice_bit_offset() == slice_bit_offset &&
        slice_properties.slice_bit_width() == dest_field->bit_width()) {
      dest_field->set_type(slice_properties.sliced_field_type());
      dest_field->set_header_type(source_field.header_type());
      dest_field->set_bit_offset(slice_bit_offset + source_field.bit_offset());
      slice_found = true;
      break;
    }
  }

  return slice_found;
}

}  // namespace p4c_backends
}  // namespace stratum
