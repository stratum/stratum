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

#include "stratum/p4c_backends/fpm/switch_p4c_backend.h"

#include <map>
#include <memory>
#include <vector>

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/common/program_inspector.h"
#include "stratum/p4c_backends/fpm/action_decoder.h"
#include "stratum/p4c_backends/fpm/control_inspector.h"
#include "stratum/p4c_backends/fpm/field_cross_reference.h"
#include "stratum/p4c_backends/fpm/field_name_inspector.h"
#include "stratum/p4c_backends/fpm/header_valid_inspector.h"
#include "stratum/p4c_backends/fpm/hidden_static_mapper.h"
#include "stratum/p4c_backends/fpm/hidden_table_mapper.h"
#include "stratum/p4c_backends/fpm/hit_assign_mapper.h"
#include "stratum/p4c_backends/fpm/meta_key_mapper.h"
#include "stratum/p4c_backends/fpm/meter_color_mapper.h"
#include "stratum/p4c_backends/fpm/pipeline_optimizer.h"
#include "stratum/p4c_backends/fpm/slice_cross_reference.h"
#include "stratum/p4c_backends/fpm/table_hit_inspector.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/table_type_mapper.h"
#include "stratum/p4c_backends/fpm/tunnel_type_mapper.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "absl/debugging/leak_check.h"
#include "absl/memory/memory.h"
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/frontends/p4/methodInstance.h"

DEFINE_string(p4_pipeline_config_text_file, "",
              "Path to text file for P4PipelineConfig output");
DEFINE_string(p4_pipeline_config_binary_file, "",
              "Path to file for serialized P4PipelineConfig output");
DEFINE_string(slice_map_file,
              "stratum/p4c_backends/fpm/"
              "map_data/sliced_field_map.pb.txt",
              "Path to text file that defines sliced field mappings");
DEFINE_string(target_parser_map_file, "",
              "Path to text file that defines target parser extractions");

namespace stratum {
namespace p4c_backends {

namespace {

// Returns true if the input node has an @hidden annotation.
// TODO(unknown): Is this useful outside a private namespace in this file?
bool IsHidden(const IR::Node& node) {
  return node.getAnnotation("hidden") != nullptr;
}

}  // namespace

SwitchP4cBackend::SwitchP4cBackend(TableMapGenerator* table_mapper,
                                   P4cFrontMidInterface* front_mid_interface,
                                   AnnotationMapper* annotation_mapper,
                                   TunnelOptimizerInterface* tunnel_optimizer)
    : table_mapper_(ABSL_DIE_IF_NULL(table_mapper)),
      front_mid_interface_(ABSL_DIE_IF_NULL(front_mid_interface)),
      annotation_mapper_(annotation_mapper),  // NULL is allowed.
      tunnel_optimizer_(ABSL_DIE_IF_NULL(tunnel_optimizer)),
      parser_decoder_(new ParserDecoder),
      ref_map_(nullptr),
      type_map_(nullptr),
      v1model_(P4V1::V1Model::instance) {}

void SwitchP4cBackend::Compile(
    const IR::ToplevelBlock& top_level,
    const ::p4::v1::WriteRequest& static_table_entries,
    const ::p4::config::v1::P4Info& p4_info, P4::ReferenceMap* ref_map,
    P4::TypeMap* type_map) {
  // TODO(unknown): Should NULL inputs be treated as compiler bugs?
  ref_map_ = ref_map;
  if (ref_map_ == nullptr) {
    ::error("No reference map for input P4 program");
    return;
  }
  type_map_ = type_map;
  if (type_map_ == nullptr) {
    ::error("No type map for input P4 program");
    return;
  }
  {
    absl::LeakCheckDisabler disable_top_level_leak_checks;
    auto package = top_level.getMain();
    if (package == nullptr) {
      ::error("No output to generate for input P4 program");
      return;
    }
    if (package->type->name != v1model_.sw.name) {
      ::error(
          "This back-end requires the program to be compiled for the "
          "%1% model",
          v1model_.sw.name);
      return;
    }
  }

  // The p4_info_manager_ verifies that the p4_info provided by earlier
  // compiler passes is valid for eventual use on the Stratum switch.
  // It also provides P4 object data for some of the conversion methods
  // that execute below.
  p4_info_manager_ = absl::make_unique<hal::P4InfoManager>(p4_info);
  if (!p4_info_manager_->InitializeAndVerify().ok()) {
    ::error("Invalid P4Info for input P4 program");
    return;
  }

  GetP4ModelNames(top_level);

  // If the flags identifying the parser definition file for the target and/or
  // the sliced field map are available, read them here for future use.
  if (!FLAGS_target_parser_map_file.empty()) {
    if (!ReadProtoFromTextFile(
        FLAGS_target_parser_map_file, &target_parser_map_).ok()) {
      LOG(WARNING) << "Unable to read target parser spec from "
                   << FLAGS_target_parser_map_file;
    }
  }
  if (!FLAGS_slice_map_file.empty()) {
    if (!ReadProtoFromTextFile(FLAGS_slice_map_file, &sliced_field_map_).ok()) {
      LOG(WARNING) << "Unable to read slice map file from "
                   << FLAGS_slice_map_file;
    }
  }

  // The ProgramInspector looks through the IR for nodes that this backend
  // needs to create the P4PipelineConfig content.
  ProgramInspector program_inspector;
  {
    absl::LeakCheckDisabler disable_p4_program_leak_checks;
    top_level.getProgram()->apply(program_inspector);
  }

  // The standard metadata name is built into the P4 V1 model.  This also
  // applies to P4_16 programs based on the V1 model.
  VLOG(1) << "V1 std metadata ingress name "
          << v1model_.ingress.standardMetadataParam.name;
  VLOG(1) << "V1 drop name " << v1model_.drop.name;
  VLOG(1) << "V1 action profile name " << v1model_.action_profile.name;

  // For P4_14/V1 programs, the p4c IR inserts some prefixes in the IR names
  // that don't appear in the P4 info output.  The code below finds these
  // prefixes and adds them to the set that FieldNameInspector ignores later.
  std::string header_prefix(v1model_.parser.headersParam.name);
  VLOG(1) << "V1 headers name " << header_prefix;
  std::string user_metadata_prefix(v1model_.parser.metadataParam.name);
  VLOG(1) << "V1 parser user meta name " << user_metadata_prefix;
  if (front_mid_interface_->IsV1Program()) {
    (*p4_model_names_.mutable_strip_path_prefixes())[header_prefix] = 0;
    (*p4_model_names_.mutable_strip_path_prefixes())[user_metadata_prefix] = 0;
  }
  FindLocalMetadataType(program_inspector.controls(), &p4_model_names_);
  SetP4ModelNames(p4_model_names_);
  field_decoder_ = absl::make_unique<FieldDecoder>(table_mapper_);
  parser_field_mapper_ = absl::make_unique<ParserFieldMapper>(table_mapper_);

  // Preliminary stuff is done, the real work to convert the IR to a
  // P4PipelineConfig is below.
  // TODO(unknown): Add error checking and exit if any of the phases below
  // detect a bug or unsupported feature in the P4 program.
  ConvertHeaderPaths(program_inspector.struct_paths());
  field_decoder_->ConvertHeaderFields(
      program_inspector.p4_typedefs(), program_inspector.p4_enums(),
      program_inspector.struct_likes(), program_inspector.header_types(),
      path_to_header_type_map_);
  field_decoder_->ConvertMatchKeys(program_inspector.match_keys());
  ConvertParser(program_inspector.parsers());
  ConvertActions(program_inspector.actions());
  ConvertTables(program_inspector.tables());
  MetaKeyMapper meta_key_mapper;
  meta_key_mapper.FindMetaKeys(p4_info_manager_->p4_info().tables(),
                               table_mapper_);

  // ConvertControls writes P4Control entries into output_pipeline_cfg.  It
  // also uses table_mapper_ to update some action descriptors in the P4 table
  // map.  Both sets of data merge into output_pipeline_cfg when finished,
  // along with any static table entries from earlier p4c passes.
  hal::P4PipelineConfig output_pipeline_cfg;
  ConvertControls(program_inspector.controls(), &output_pipeline_cfg);
  output_pipeline_cfg.MergeFrom(table_mapper_->generated_map());
  *(output_pipeline_cfg.mutable_static_table_entries()) = static_table_entries;

  // Most table mapping from the IR is done.  The post-processing steps below
  // attempt to determine additional field type information from annotations
  // and from cross references among assignment statements.
  if (!ProcessAnnotations(p4_info, &output_pipeline_cfg)) {
    ::error("P4PipelineConfig annotation processing failed");
    return;
  }
  FieldCrossReference field_xref;
  field_xref.ProcessAssignments(program_inspector.assignments(),
                                &output_pipeline_cfg);
  SliceCrossReference slice_xref(sliced_field_map_, ref_map_, type_map_);
  slice_xref.ProcessAssignments(program_inspector.assignments(),
                                &output_pipeline_cfg);
  TunnelTypeMapper tunnel_type_mapper(&output_pipeline_cfg);
  tunnel_type_mapper.ProcessTunnels();
  TableTypeMapper table_type_mapper;
  table_type_mapper.ProcessTables(*p4_info_manager_, &output_pipeline_cfg);
  HiddenTableMapper hidden_table_mapper;
  hidden_table_mapper.ProcessTables(*p4_info_manager_, &output_pipeline_cfg);
  HiddenStaticMapper hidden_static_mapper(*p4_info_manager_, tunnel_optimizer_);
  hidden_static_mapper.ProcessStaticEntries(
      hidden_table_mapper.action_redirects(), &output_pipeline_cfg);

  // P4PipelineConfig output goes to the selected files, if any, after
  // all backend work completes error free.
  if (front_mid_interface_->GetErrorCount()) return;
  if (!FLAGS_p4_pipeline_config_binary_file.empty()) {
    if (!WriteProtoToBinFile(output_pipeline_cfg,
                             FLAGS_p4_pipeline_config_binary_file)
             .ok()) {
      LOG(ERROR) << "Failed to write P4PipelineConfig to "
                 << FLAGS_p4_pipeline_config_binary_file;
    }
  }
  if (!FLAGS_p4_pipeline_config_text_file.empty()) {
    if (!WriteProtoToTextFile(output_pipeline_cfg,
                              FLAGS_p4_pipeline_config_text_file)
             .ok()) {
      LOG(ERROR) << "Failed to write P4PipelineConfig to "
                 << FLAGS_p4_pipeline_config_text_file;
    }
  }
}

void SwitchP4cBackend::ConvertHeaderPaths(
    const std::vector<const IR::PathExpression*>& paths) {
  for (auto path : paths) {
    HeaderPathInspector path_inspector;
    path_inspector.Inspect(*path);
    path_to_header_type_map_.insert(
        path_inspector.path_to_header_type_map().begin(),
        path_inspector.path_to_header_type_map().end());
  }
}

void SwitchP4cBackend::ConvertActions(
    const std::map<const IR::P4Action*, const IR::P4Control*>& ir_actions) {
  std::unique_ptr<ActionDecoder> action_decoder(new ActionDecoder(
      table_mapper_, ref_map_, type_map_));
  for (auto map_iter : ir_actions) {
    auto action = map_iter.first;
    if (IsHidden(*action))
      continue;
    // TODO(unknown): The control node pointer in map_iter.second doesn't seem
    //                 to add much value.  Remove it from the program_inspector.
    std::string action_name = StripNamePrefix(action->externalName());
    VLOG(1) << "Processing action " << action_name;
    action_name_map_[std::string(action->name.name)] = action_name;
    action_decoder->ConvertActionBody(action_name, action->body->components);
    if (VLOG_IS_ON(2)) ::dump(action);
  }
}

void SwitchP4cBackend::ConvertParser(
    const std::vector<const IR::P4Parser*>& parsers) {
  // This backend expects exactly one parser to exist in the P4 program.
  if (parsers.size() != 1) {
    ::error("This back-end expects one P4 parser but found %1%",
            parsers.size());
    return;
  }

  parser_decoder_->DecodeParser(*parsers[0], ref_map_, type_map_);
  parser_field_mapper_->MapFields(parser_decoder_->parser_states(),
                                  field_decoder_->extracted_fields_per_type(),
                                  target_parser_map_);
  parser_value_set_mapper_ = absl::make_unique<ParserValueSetMapper>(
      parser_decoder_->parser_states(), *p4_info_manager_, table_mapper_);
  parser_value_set_mapper_->MapValueSets(*parsers[0]);
}

void SwitchP4cBackend::ConvertTables(
    const std::vector<const IR::P4Table*>& ir_tables) {
  for (auto table : ir_tables) {
    if (IsHidden(*table))
      continue;
    const std::string p4_table_name = std::string(table->controlPlaneName());
    VLOG(1) << "Processing table " << p4_table_name;
    table_mapper_->AddTable(p4_table_name);
    if (table->getEntries() != nullptr) {
      VLOG(2) << p4_table_name << " has static entries";
      table_mapper_->SetTableStaticEntriesFlag(p4_table_name);
    }
  }
}

void SwitchP4cBackend::ConvertControls(
    const std::vector<const IR::P4Control*>& controls,
    hal::P4PipelineConfig* output_pipeline_cfg) {
  switch_case_decoder_ = absl::make_unique<SwitchCaseDecoder>(
      action_name_map_, ref_map_, type_map_, table_mapper_);
  for (auto control : controls) {
    unsigned no_opt_error_count = front_mid_interface_->GetErrorCount();
    VLOG(1) << "Processing control " << control->externalName();

    // Control transforms and optimizations need to occur before
    // the ControlInspector runs.
    HitAssignMapper hit_assign_mapper(ref_map_, type_map_);
    auto hit_assigned_control = hit_assign_mapper.Apply(*control);
    MeterColorMapper color_mapper(ref_map_, type_map_, table_mapper_);
    auto color_mapped_control = color_mapper.Apply(*hit_assigned_control);
    TableHitInspector table_inspector(false, false, ref_map_, type_map_);
    table_inspector.Inspect(*color_mapped_control->body);
    PipelineOptimizer optimizer(ref_map_, type_map_);
    auto optimized_control = optimizer.Optimize(*color_mapped_control);
    if (no_opt_error_count != front_mid_interface_->GetErrorCount()) {
      LOG(WARNING)
          << "Skipping remaining processing of P4Control "
          << control->externalName()
          << " due to errors in preliminary optimization passes";
      continue;
    }

    ControlInspector control_inspector(
        p4_info_manager_.get(), ref_map_, type_map_,
        switch_case_decoder_.get(), table_mapper_);
    control_inspector.Inspect(*optimized_control);
    *output_pipeline_cfg->add_p4_controls() = control_inspector.control();

    HeaderValidInspector header_valid_inspector(ref_map_, type_map_);
    header_valid_inspector.Inspect(*optimized_control->body, table_mapper_);
  }
}

bool SwitchP4cBackend::ProcessAnnotations(
    const ::p4::config::v1::P4Info& p4_info,
    hal::P4PipelineConfig* output_pipeline_cfg) {
  if (annotation_mapper_ == nullptr) {
    LOG(WARNING) << "Skipping annotation mapping - no AnnotationMapper";
    return true;
  }
  if (!annotation_mapper_->Init())
    return false;
  return annotation_mapper_->ProcessAnnotations(*p4_info_manager_,
                                                output_pipeline_cfg);
}

// TODO(unknown): Generalize to non-V1 models.
void SwitchP4cBackend::GetP4ModelNames(const IR::ToplevelBlock& top_level) {
  const IR::PackageBlock* package = nullptr;
  {
    absl::LeakCheckDisabler disable_top_level_leak_checks;
    package = top_level.getMain();
  }
  auto ingress = package->getParameterValue(v1model_.sw.ingress.name);
  auto egress = package->getParameterValue(v1model_.sw.egress.name);
  if (ingress->is<IR::ControlBlock>()) {
    p4_model_names_.set_ingress_control_name(
        ingress->to<IR::ControlBlock>()->container->name.toString());
  } else {
    LOG(ERROR) << "V1 model ingress is not an IR::ControlBlock";
  }
  if (egress->is<IR::ControlBlock>()) {
    p4_model_names_.set_egress_control_name(
        egress->to<IR::ControlBlock>()->container->name.toString());
  } else {
    LOG(ERROR) << "V1 model egress is not an IR::ControlBlock";
  }

  p4_model_names_.set_drop_extern_name(v1model_.drop.name);
  p4_model_names_.set_clone_extern_name(v1model_.clone.name);
  p4_model_names_.set_clone3_extern_name(v1model_.clone.clone3.name);
  p4_model_names_.set_counter_extern_name(v1model_.counter.name);
  p4_model_names_.set_meter_extern_name(v1model_.meter.name);
  p4_model_names_.set_direct_counter_extern_name(v1model_.directCounter.name);
  p4_model_names_.set_direct_meter_extern_name(v1model_.directMeter.name);

  p4_model_names_.set_counter_count_method_name(
      v1model_.counter.increment.name);
  p4_model_names_.set_direct_counter_count_method_name(
      v1model_.directCounter.count.name);
  p4_model_names_.set_meter_execute_method_name(
      v1model_.meter.executeMeter.name);
  p4_model_names_.set_direct_meter_read_method_name(
      v1model_.directMeter.read.name);

  // TODO(unknown): PSA is expected to include a standard enum type for
  // color, which could then be the source of P4ModelNames data below.  Data
  // must currently be hard-coded for the V1 model.
  p4_model_names_.set_color_enum_type("MeterColor_t");
  p4_model_names_.set_color_enum_green("GREEN");
  p4_model_names_.set_color_enum_yellow("YELLOW");
  p4_model_names_.set_color_enum_red("RED");

  p4_model_names_.set_clone_type_ingress_to_egress(
      v1model_.clone.cloneType.i2e.name);
  p4_model_names_.set_clone_type_egress_to_egress(
      v1model_.clone.cloneType.e2e.name);

  const ::P4::P4CoreLibrary& core_lib = ::P4::P4CoreLibrary::instance;
  p4_model_names_.set_no_action(core_lib.noAction.name);
  p4_model_names_.set_exact_match(core_lib.exactMatch.name);
  p4_model_names_.set_lpm_match(core_lib.lpmMatch.name);
  p4_model_names_.set_ternary_match(core_lib.ternaryMatch.name);
  p4_model_names_.set_range_match(v1model_.rangeMatchType.name);
  p4_model_names_.set_selector_match(v1model_.selectorMatchType.name);
}

// The p4c IR prefixes some object names with a '.' to indicate they are at
// the top-level of the P4 object hierarchy.  The p4c P4Info serializer strips
// these prefixes, so this backend needs to do the same.
std::string SwitchP4cBackend::StripNamePrefix(const cstring& external_name) {
  std::string stripped_name(external_name);
  if (!stripped_name.empty() && stripped_name[0] == '.')
    stripped_name.erase(0, 1);
  return stripped_name;
}

}  // namespace p4c_backends
}  // namespace stratum
