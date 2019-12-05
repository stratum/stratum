/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// FieldCrossReference is a p4c backend class that deduces P4 header and
// metadata field types from assignment statements within the P4 program.
// Given an assignment:
//
//  meta_type.field_1 = hdr_type.field_2;
//
// FieldCrossReference looks for assignments where p4c has determined the
// type of one field but not the other.  For example, if p4c knows that
// hdr_type.field_2's type is P4_FIELD_TYPE_INGRESS_PORT and the type of
// meta_type.field_1 is unknown, the FieldCrossReference class can infer
// that meta_type.field_1 is also used as type P4_FIELD_TYPE_INGRESS_PORT.
//
// FieldCrossReference also applies this property transitively across
// multiple assignments.

#ifndef STRATUM_P4C_BACKENDS_FPM_FIELD_CROSS_REFERENCE_H_
#define STRATUM_P4C_BACKENDS_FPM_FIELD_CROSS_REFERENCE_H_

#include <map>
#include <set>
#include <vector>

#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"

namespace stratum {
namespace p4c_backends {

// Normal usage is to create a FieldCrossReference instance and then call
// ProcessAssignments with a vector of all the assignment statements in the
// P4 program.  FieldCrossReference expects to run once near the end of
// backend processing, after all other methods for determining field
// types have executed.
class FieldCrossReference {
 public:
  FieldCrossReference() {}
  virtual ~FieldCrossReference() {}

  // ProcessAssignments examines all of the input assignments, builds a
  // cross reference map, and then looks for cross references where the
  // type of one field implies the type of another field.  Upon finding any
  // such references, ProcessAssignments updates the related field descriptors
  // in p4_pipeline_config.  To be most effective, the input vector should
  // contain all the assignments in the P4 program, which is available from
  // the ProgramInspector's assignments() accessor.
  void ProcessAssignments(
      const std::vector<const IR::AssignmentStatement*>& assignments,
      hal::P4PipelineConfig* p4_pipeline_config);

  // FieldCrossReference is neither copyable nor movable.
  FieldCrossReference(const FieldCrossReference&) = delete;
  FieldCrossReference& operator=(const FieldCrossReference&) = delete;

 private:
  // This set contains P4FieldDescriptors for all fields that are the source
  // expression in assignments to a given field.  In this example:
  //  fieldA = fieldB;
  //  fieldA = fieldC;
  // fieldA's SourceFieldSet contains the descriptors for {fieldB, fieldC}.
  using SourceFieldSet = std::set<hal::P4FieldDescriptor*>;

  // Adds a field_xref_map_ for the two fields in an assignment statement,
  // where the inputs represent P4 table map values for the fields on each
  // side of the assignment operator.  The table map values may be modified
  // later if FieldCrossReference finds a field type inference.
  void AddFieldXref(hal::P4TableMapValue* destination_field,
                    hal::P4TableMapValue* source_field);

  // Once field_xref_map_ is fully populated, UpdateFieldTypes and
  // ProcessXrefEntry process the cross references for field type inferences.
  // UpdateFieldTypes coordinates multiple passes through the cross references,
  // calling ProcessXrefEntry to handle each entry.  ProcessXrefEntry returns
  // true when it makes a field type adjustment.
  void UpdateFieldTypes();
  bool ProcessXrefEntry(hal::P4FieldDescriptor* destination_field,
                        const SourceFieldSet& source_field_set);

  // This map records all cross references in P4 assignment statements.
  // The key is the destination field in an assignment, and the value is
  // the set of descriptors for all fields that are the sources of
  // assignments to the field.  For the example in the SourceFieldSet comments,
  // a field_xref_map_ entry would be created as:
  //  field_xref_map_[fieldA] = {fieldB, fieldC};
  std::map<hal::P4FieldDescriptor*, SourceFieldSet> field_xref_map_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_FIELD_CROSS_REFERENCE_H_
