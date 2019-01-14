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

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_UTILS_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_UTILS_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "stratum/hal/lib/p4/p4_control.host.pb.h"
#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.host.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.host.pb.h"
#include "stratum/p4c_backends/fpm/p4_model_names.host.pb.h"
#include "stratum/p4c_backends/fpm/parser_map.host.pb.h"
#include "stratum/public/proto/p4_annotation.host.pb.h"
#include "stratum/public/proto/p4_table_defs.host.pb.h"
#include "p4lang_p4c/frontends/p4/coreLibrary.h"
#include "p4lang_p4c/frontends/p4/methodInstance.h"

namespace stratum {
namespace p4c_backends {

// Parses @switchstack annotations from their p4c IR form into a
// P4Annotation message.  Returns true if successful, false if the input
// IR node is not annotated or at least one annotation can't be parsed.
// When the input node has multiple annotations, GetSwitchStackAnnotation
// merges the parsed values into the output message.  For example, a node with:
//   @switchstack("pipeline_stage: VLAN_ACL")
//   @switchstack("field_type: P4_FIELD_TYPE_VRF")
// yields an output message with values for both pipeline_stage and field_type.
bool GetSwitchStackAnnotation(const IR::Node& node,
                              P4Annotation* switch_stack_annotation);

// Parses a @switchstack annotation from its p4c IR form into a PipelineStage.
// Returns DEFAULT_STAGE if node has no @switchstack annotation or the
// annotation does not specify a pipeline_stage.
P4Annotation::PipelineStage GetAnnotatedPipelineStage(const IR::Node& node);

// Behaves like GetAnnotatedPipelineStage, but in addition uses p4c's
// error reporter to flag a P4 program error when the annotation does not exist.
// The input must be a P4Table subclass of IR::Node for proper p4c error
// reporting.
P4Annotation::PipelineStage GetAnnotatedPipelineStageOrP4Error(
    const IR::P4Table& table);

// Parses a @controller_header annotation from its p4c IR form into its
// string value, typically "packet_in" or "packet_out"; returns the
// annotation value if the input node has a single valid @controller_header
// annotation; returns an empty string when the input node does not have the
// annotation or if multiple @controller_header annotations are present.
std::string GetControllerHeaderAnnotation(const IR::Node& node);

// These two functions populate a P4ControlTableRef message from the input
// table data.  FillTableRefByName acts based on the table name string, whereas
// FillTableRefFromIR takes data from a P4Table node in p4c's IR.  Both
// functions refer to a P4InfoManager to assist in table name to ID mapping.
void FillTableRefByName(const std::string& table_name,
                        const hal::P4InfoManager& p4_info_manager,
                        hal::P4ControlTableRef* table_ref);
void FillTableRefFromIR(const IR::P4Table& ir_table,
                        const hal::P4InfoManager& p4_info_manager,
                        hal::P4ControlTableRef* table_ref);

// IsPipelineStageFixed evaluates the input pipeline stage and returns true
// if it matches a fixed-function stage of the forwarding pipeline hardware.
bool IsPipelineStageFixed(P4Annotation::PipelineStage stage);

// Determines whether the input MethodInstance represents a table apply.
// For valid applies, the result is true, and the applied_stage output contains
// either the table's annotated pipeline stage or P4Annotation::DEFAULT_STAGE
// if the table is not annotated.  If a table is not annotated, a P4 program
// bug is reported.
bool IsTableApplyInstance(const P4::MethodInstance& instance,
                          P4Annotation::PipelineStage* applied_stage);

// Examines the input controls to identify the type name for the P4 program's
// local metadata.  The p4_model_names parameter is an input and output.  On
// input, it contains architecture-dependent control method names.  When
// FindLocalMetadataType successfully locates the local metadata type, it
// stores the type name in p4_model_names before returning.
void FindLocalMetadataType(const std::vector<const IR::P4Control*>& controls,
                           P4ModelNames* p4_model_names);

// Checks the input field descriptor's type to determine whether the backend
// considers it to be an unspecified type.
bool IsFieldTypeUnspecified(const hal::P4FieldDescriptor& descriptor);

// These functions manage a global instance of P4ModelNames for use throughout
// the Hercules p4c backend.  SetP4ModelNames is called to set up the global
// instance.  Calling GetP4ModelNames returns a reference to the global
// instance.  If a call to GetP4ModelNames occurs before SetP4ModelNames,
// GetP4ModelNames returns an empty instance of P4ModelNames.
void SetP4ModelNames(const P4ModelNames& p4_model_names);
const P4ModelNames& GetP4ModelNames();

// Calls SetP4ModelNames with values that are suitable for many of the
// P4 spec files in the testdata subdirectory.
void SetUpTestP4ModelNames();

// Generates an output string appending an array index to the given
// header_name.  For example, if header_name is "hdr.name" and index is 2,
// the output is "hdr.name[2]".
std::string AddHeaderArrayIndex(const std::string& header_name, int64 index);

// Generates an output string appending the P4 parser "last" operator to the
// given header_name.  For example, if header_name is "hdr.name", the
// output is "hdr.name.last".
std::string AddHeaderArrayLast(const std::string& header_name);

// Returns true if the input ParserState specifies a transition to one of
// P4's built-in terminating states, i.e. "accept" or "reject".
bool IsParserEndState(const ParserState& state);

// Utilities to look up various descriptor types in the P4PipelineConfig.
// These should only be used when the descriptor is expected to exist, and
// a missing descriptor indicates a serious bug in the compiler.  The action
// descriptor functions only look for action descriptors defined by the P4
// program - they do not consider internal actions.
const hal::P4TableDescriptor& FindTableDescriptorOrDie(
    const std::string& table_name,
    const hal::P4PipelineConfig& p4_pipeline_config);
hal::P4TableDescriptor* FindMutableTableDescriptorOrDie(
    const std::string& table_name, hal::P4PipelineConfig* p4_pipeline_config);
const hal::P4ActionDescriptor& FindActionDescriptorOrDie(
    const std::string& action_name,
    const hal::P4PipelineConfig& p4_pipeline_config);
hal::P4ActionDescriptor* FindMutableActionDescriptorOrDie(
    const std::string& action_name, hal::P4PipelineConfig* p4_pipeline_config);
const hal::P4HeaderDescriptor& FindHeaderDescriptorOrDie(
    const std::string& header_name,
    const hal::P4PipelineConfig& p4_pipeline_config);
const hal::P4HeaderDescriptor& FindHeaderDescriptorForFieldOrDie(
    const std::string& field_name,
    P4HeaderType header_type,
    const hal::P4PipelineConfig& p4_pipeline_config);

// These utility functions provide common support for field descriptor lookup.
// Field descriptors aren't conducive to an "OrDie" lookup because many
// references to field names in the P4PipelineConfig could also be header
// descriptors.  When one of these functions returns nullptr, the caller must
// decide whether the context is also appropriate for a packet header.
const hal::P4FieldDescriptor* FindFieldDescriptorOrNull(
    const std::string& field_name,
    const hal::P4PipelineConfig& p4_pipeline_config);
hal::P4FieldDescriptor* FindMutableFieldDescriptorOrNull(
    const std::string& field_name, hal::P4PipelineConfig* p4_pipeline_config);

// During optimization steps, the backend often needs to delete a subset of
// the repeated fields in a protobuf.  As the code determines the fields to
// delete, it typically stores a container of field indexes in order to avoid
// invalidating an iterator.  These functions take a delete_indexes vector and
// remove the members of repeated_fields with the selected indexes.  The
// field indexes in delete_indexes must be in ascending order.
template <typename ELEMENT>
void DeleteRepeatedFields(
    const std::vector<int>& delete_indexes,
    ::google::protobuf::RepeatedPtrField<ELEMENT>* repeated_fields) {
  int prior_index = repeated_fields->size();
  for (auto r_iter = delete_indexes.rbegin();
       r_iter !=  delete_indexes.rend(); ++r_iter) {
    DCHECK(0 <= *r_iter && *r_iter < prior_index)
        << "Invalid deleted field index " << *r_iter
        << " must be non-negative in ascending order";
    repeated_fields->DeleteSubrange(*r_iter, 1);
    prior_index = *r_iter;
  }
}

// DeleteRepeatedNonPtrFields is like DeleteRepeatedFields, but it takes a
// RepeatedField input instead of a RepeatedPtrField input.
template <typename ELEMENT>
void DeleteRepeatedNonPtrFields(
    const std::vector<int>& delete_indexes,
    ::google::protobuf::RepeatedField<ELEMENT>* repeated_fields) {
  int prior_index = repeated_fields->size();
  for (auto rindex_iter = delete_indexes.rbegin();
       rindex_iter != delete_indexes.rend(); ++rindex_iter) {
    DCHECK(0 <= *rindex_iter && *rindex_iter < prior_index)
        << "Invalid deleted field index " << *rindex_iter
        << " must be non-negative in ascending order";
    auto field_iter = repeated_fields->begin() + *rindex_iter;
    repeated_fields->erase(field_iter);
    prior_index = *rindex_iter;
  }
}

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_UTILS_H_
