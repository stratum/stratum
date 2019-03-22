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

// The SwitchP4cBackend is a BackendExtensionInterface for Stratum switches.
// It manages the translation from the p4c Internal Representation (IR) to a
// HAL P4PipelineConfig.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_SWITCH_P4C_BACKEND_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_SWITCH_P4C_BACKEND_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/p4c_backends/common/backend_extension_interface.h"
#include "stratum/p4c_backends/common/p4c_front_mid_interface.h"
#include "stratum/p4c_backends/fpm/annotation_mapper.h"
#include "stratum/p4c_backends/fpm/field_decoder.h"
#include "stratum/p4c_backends/fpm/header_path_inspector.h"
#include "stratum/p4c_backends/fpm/p4_model_names.pb.h"
#include "stratum/p4c_backends/fpm/parser_decoder.h"
#include "stratum/p4c_backends/fpm/parser_field_mapper.h"
#include "stratum/p4c_backends/fpm/parser_map.pb.h"
#include "stratum/p4c_backends/fpm/parser_value_set_mapper.h"
#include "stratum/p4c_backends/fpm/sliced_field_map.pb.h"
#include "stratum/p4c_backends/fpm/switch_case_decoder.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/tunnel_optimizer_interface.h"
#include "external/com_github_p4lang_p4c/frontends/common/options.h"
#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/fromv1.0/v1model.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

// SwitchP4cBackend currently requires a v1/p4_16 model to be used.
class SwitchP4cBackend : public BackendExtensionInterface {
 public:
  // The constructor requires an injected table_mapper for pipeline config
  // output and a P4cFrontMidInterface to get information from previous passes.
  // The annotation_mapper is optional.
  SwitchP4cBackend(TableMapGenerator* table_mapper,
                   P4cFrontMidInterface* front_mid_interface,
                   AnnotationMapper* annotation_mapper,
                   TunnelOptimizerInterface* tunnel_optimizer);
  ~SwitchP4cBackend() override {};

  // The Compile method does all the work to translate the top_level IR program
  // into a P4PipelineConfig for runtime use on a Stratum fixed-function
  // switch.
  // TODO: Are there variations among platforms, e.g. Tomahawk vs.
  //                 Tomahawk 2/3 that will need to be differentiated by
  //                 command-line flag or even separate subclasses?
  void Compile(const IR::ToplevelBlock& top_level,
               const ::p4::v1::WriteRequest& static_table_entries,
               const ::p4::config::v1::P4Info& p4_info,
               P4::ReferenceMap* ref_map, P4::TypeMap* type_map) override;

  // SwitchP4cBackend is neither copyable nor movable.
  SwitchP4cBackend(const SwitchP4cBackend&) = delete;
  SwitchP4cBackend& operator=(const SwitchP4cBackend&) = delete;

 private:
  // Converts the P4 program's path expressions into path_to_header_type_map_
  // entries mapping fully-qualified header path names to header types.
  void ConvertHeaderPaths(const std::vector<const IR::PathExpression*>& paths);

  // Converts the actions represented by the IR inputs into action
  // descriptor entries in the P4PipelineConfig table map.  The action inputs
  // come from an initial IR pass by a ProgramInspector.
  void ConvertActions(
      const std::map<const IR::P4Action*, const IR::P4Control*>& ir_actions);

  // Processes the input IR parsers, to determine the mapping assignments for
  // header fields.
  void ConvertParser(const std::vector<const IR::P4Parser*>& parsers);

  // Converts the P4 tables represented by the IR inputs into table
  // descriptor entries in the P4PipelineConfig table map.  The table inputs
  // come from an initial IR pass by a ProgramInspector.
  void ConvertTables(const std::vector<const IR::P4Table*>& ir_tables);

  // Converts the P4Control nodes represented by the IR inputs into
  // P4PipelineConfig data.
  void ConvertControls(const std::vector<const IR::P4Control*>& controls,
                       hal::P4PipelineConfig* output_pipeline_cfg);

  // Processes P4 annotations as they pertain to the compiler output, leaving
  // an updated P4PipelineConfig in output_pipeline_cfg.
  bool ProcessAnnotations(const ::p4::config::v1::P4Info& p4_info,
                          hal::P4PipelineConfig* output_pipeline_cfg);

  // Determines names of the various common functions relative
  // to the P4 architecture model.
  void GetP4ModelNames(const IR::ToplevelBlock& top_level);

  // Strips any p4c-added prefixes from a P4 external object name to make
  // the name match what appears in the P4Info.
  static std::string StripNamePrefix(const cstring& external_name);

  // Accumulates mapped IR elements in the output table map, injected and owned
  // by the caller of the constructor.
  TableMapGenerator* table_mapper_;

  // Provides data from the front and mid end passes that preceded this
  // backend; injected and owned by the caller of the constructor.
  P4cFrontMidInterface* front_mid_interface_;

  // Applies annotations mapping on the post-IR table map before output occurs,
  // injected and owned by the caller of the constructor.
  AnnotationMapper* annotation_mapper_;

  // Does target-specific tunnel action optimizations; injected and owned
  // by the caller of the constructor.
  TunnelOptimizerInterface* tunnel_optimizer_;

  // Provides convenient access to P4Info for conversion methods that need it.
  std::unique_ptr<hal::P4InfoManager> p4_info_manager_;

  // Uses parser state and expressions to interpret header field types.
  // TODO: Evaluate injecting a mock ParserDecoder, although
  // intuitively it seems like ParserDecoder output will be complex enough that
  // it's not a good candidate for mocking.
  std::unique_ptr<ParserDecoder> parser_decoder_;

  // Uses IR StructLike, Header, and KeyElement types to derive field
  // descriptor entries in the P4PipelineConfig table map.
  // TODO: As above, is FieldDecoder mocking practical?
  std::unique_ptr<FieldDecoder> field_decoder_;

  // Provides a container to accumulate HeaderPathInspector output from
  // visiting the P4 program's IR::PathExpressions.
  HeaderPathInspector::PathToHeaderTypeMap path_to_header_type_map_;

  // Combines the ParserDecoder and FieldDecoder outputs to generate P4 field
  // type mappings in the output table map.
  std::unique_ptr<ParserFieldMapper> parser_field_mapper_;

  // Uses ParserDecoder output to classify UDFs based on value sets.
  std::unique_ptr<ParserValueSetMapper> parser_value_set_mapper_;

  // Generates additional action descriptor data for switch statements in
  // P4 control functions.
  std::unique_ptr<SwitchCaseDecoder> switch_case_decoder_;

  // The ref_map_ and type_map_ are provided by the Compile method caller,
  // and the caller retains ownership.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;

  // The v1model_ references p4c's global model instance.
  P4V1::V1Model& v1model_;

  // This protobuf member contains strings that record names of the P4
  // control functions, extern functions, and other references relative
  // to the active architecture model.
  P4ModelNames p4_model_names_;

  // The target_parser_map_ defines the target's parser behavior, which is
  // read from a command-line-specified text file.
  ParserMap target_parser_map_;

  // The sliced_field_map_ contains data to support slicing long header fields
  // into smaller subfields with unique field types.
  SlicedFieldMap sliced_field_map_;

  // Externally (in P4Info and P4PipelineConfig), p4c action names use
  // a <control-name>.<action-name> format.  Internally, the format is
  // <control-name>_<action-name>_<N>.  This map uses the internal format as
  // a key to lookup the external name.
  std::map<std::string, std::string> action_name_map_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_SWITCH_P4C_BACKEND_H_
