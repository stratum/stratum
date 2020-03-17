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

#include "stratum/p4c_backends/fpm/utils.h"

#include <string>
#include <vector>

#include "stratum/glue/logging.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/target_info.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "absl/strings/substitute.h"
#include "external/com_github_p4lang_p4c/lib/error.h"
#include "stratum/glue/gtl/map_util.h"

namespace stratum {
namespace p4c_backends {

// This unnamed namespace surrounds a local function to find and verify a
// specific type of annotation.  GetValidAnnotations queries an IR::Node
// for any annotations with the given id_name, such as "switchstack" or
// "controller_header".  When one or more instances of the requested annotation
// ID exists in node, it returns a vector of annotation value strings from all
// valid matching annotations.  If no valid annotation exists, the returned
// vector is empty.  GetValidAnnotations requires valid annotations to contain
// a single expression of type IR::StringLiteral.
namespace {

std::vector<std::string> GetValidAnnotations(
    const IR::Node& node, const std::string& id_name) {
  std::vector<std::string> values;  // Initially empty.

  // IR nodes with annotations should be an IR::IAnnotated subclass.
  if (!node.is<IR::IAnnotated>()) {
    LOG(ERROR) << node.node_type_name() << " is not IR::IAnnotated";
    return values;
  }

  // This loop iterates over all of the node's annotations and adds those
  // matching id_name to the returned vector.
  for (auto p4c_annotation :
       node.to<IR::IAnnotated>()->getAnnotations()->annotations) {
    if (p4c_annotation->name != id_name)
      continue;
    if (p4c_annotation->needsParsing) {
      if (p4c_annotation->expr.size() != 0) {
        LOG(ERROR) << "Expected to find zero expressions";
        continue;
      }
      if (p4c_annotation->body.size() != 1) {
        LOG(ERROR) << "Expected to find exactly one body element";
        continue;
      }
      if (!p4c_annotation->body[0]->is<IR::AnnotationToken>()) {
        LOG(ERROR) << "Expected to find an IR::AnnotationToken";
        continue;
      }
      values.push_back(std::string(p4c_annotation->body[0]->text));
    } else {
      if (p4c_annotation->expr.size() != 1) {
        LOG(ERROR) << "Expected to find exactly one expression";
        continue;
      }
      if (!p4c_annotation->expr[0]->is<IR::StringLiteral>()) {
        LOG(ERROR) << "Expected to find an IR::StringLiteral";
        continue;
      }
      values.push_back(
        std::string(p4c_annotation->expr[0]->to<IR::StringLiteral>()->value));
    }
  }

  return values;
}

}  // namespace

bool GetSwitchStackAnnotation(const IR::Node& node,
                              P4Annotation* switch_stack_annotation) {
  switch_stack_annotation->Clear();

  // If the input node has "switchstack" annotations, each value string is
  // parsed and merged into the overall output.
  std::vector<std::string> values = GetValidAnnotations(node, "switchstack");
  if (values.empty())
    return false;
  for (const auto& value : values) {
    P4Annotation temp_annotation;
    if (ParseProtoFromString(value, &temp_annotation).ok()) {
      switch_stack_annotation->MergeFrom(temp_annotation);
    } else {
      switch_stack_annotation->Clear();
      LOG(ERROR) << "Unable to parse switchstack annotation " << value
                 << " in " << node.node_type_name();
      return false;
    }
  }

  return true;
}

P4Annotation::PipelineStage GetAnnotatedPipelineStage(const IR::Node& node) {
  P4Annotation annotation;
  if (!GetSwitchStackAnnotation(node, &annotation))
    return P4Annotation::DEFAULT_STAGE;
  return annotation.pipeline_stage();
}

P4Annotation::PipelineStage GetAnnotatedPipelineStageOrP4Error(
    const IR::P4Table& table) {
  P4Annotation::PipelineStage stage = GetAnnotatedPipelineStage(table);
  if (stage == P4Annotation::DEFAULT_STAGE) {
    ::error("IR node %s is missing a pipeline stage annotation", table);
  }
  return stage;
}

std::string GetControllerHeaderAnnotation(const IR::Node& node) {
  std::vector<std::string> values =
      GetValidAnnotations(node, "controller_header");

  // If the "controller_header" annotation is present for the input node, it
  // should have a single string value.
  if (values.empty())
    return "";
  if (values.size() > 1) {
    LOG(ERROR) << node.node_type_name()
               << " has multiple controller_header annotations";
    return "";
  }

  return values[0];
}

void FillTableRefByName(const std::string& table_name,
                        const hal::P4InfoManager& p4_info_manager,
                        hal::P4ControlTableRef* table_ref) {
  table_ref->set_table_name(table_name);
  auto status = p4_info_manager.FindTableByName(table_name);
  const ::p4::config::v1::Table& p4_table = status.ValueOrDie();
  table_ref->set_table_id(p4_table.preamble().id());
}

void FillTableRefFromIR(const IR::P4Table& ir_table,
                        const hal::P4InfoManager& p4_info_manager,
                        hal::P4ControlTableRef* table_ref) {
  FillTableRefByName(std::string(ir_table.controlPlaneName()), p4_info_manager,
                     table_ref);
  table_ref->set_pipeline_stage(GetAnnotatedPipelineStage(ir_table));
}

bool IsPipelineStageFixed(P4Annotation::PipelineStage stage) {
  return TargetInfo::GetSingleton()->IsPipelineStageFixed(stage);
}

bool IsTableApplyInstance(const P4::MethodInstance& instance,
                          P4Annotation::PipelineStage* applied_stage) {
  *applied_stage = P4Annotation::DEFAULT_STAGE;
  if (!instance.isApply())
    return false;
  auto apply_method = instance.to<P4::ApplyMethod>();
  if (!apply_method->isTableApply())
    return false;
  auto table = apply_method->object->to<IR::P4Table>();
  P4Annotation::PipelineStage stage =
      GetAnnotatedPipelineStageOrP4Error(*table);
  *applied_stage = stage;

  return true;
}

void FindLocalMetadataType(const std::vector<const IR::P4Control*>& controls,
                           P4ModelNames* p4_model_names) {
  p4_model_names->clear_local_metadata_type_name();
  std::string local_meta_type_name;
  for (auto control : controls) {
    if (control->externalName() != p4_model_names->ingress_control_name() &&
        control->externalName() != p4_model_names->egress_control_name()) {
      continue;
    }

    // In the V1 architecture model, the local metadata parameter is the
    // second of three parameters in both the ingress and egress controls.
    // All of the errors below should generally be detected earlier by the
    // frontend and midend passes, but they could show up when adding support
    // for a new architecture model.
    const int kIngressEgressParamSize = 3;
    const int kLocalMetaParamIndex = 1;
    if (control->type->applyParams->size() != kIngressEgressParamSize) {
      ::error("Expected ingress and egress controls to have %d parameters",
              kIngressEgressParamSize);
      return;
    }
    auto param_type = control->type->applyParams->
        parameters[kLocalMetaParamIndex]->type->to<IR::Type_Name>();
    if (param_type == nullptr) {
      ::error("Expected %s parameter to be a type name",
              control->externalName());
      return;
    }
    if (local_meta_type_name.empty()) {
      local_meta_type_name = param_type->path->name.name;
    } else if (local_meta_type_name != param_type->path->name.name) {
      ::error("Ingress and egress controls have different "
              "local metadata types");
      return;
    }
  }

  VLOG(1) << "Local metadata type: " << local_meta_type_name;
  p4_model_names->set_local_metadata_type_name(local_meta_type_name);
}

// P4_FIELD_TYPE_ANNOTATED means that it may be possible to find a type in
// the field's P4 program annotations, so from p4c's perspective, the type
// has not yet been specified.
bool IsFieldTypeUnspecified(const hal::P4FieldDescriptor& descriptor) {
  return descriptor.type() == P4_FIELD_TYPE_UNKNOWN ||
      descriptor.type() == P4_FIELD_TYPE_ANNOTATED;
}

// This unnamed namespace hides the p4c backend's global P4ModelNames instance.
namespace {

P4ModelNames* GlobalP4ModelNames() {
  static auto global_p4_model_names = new P4ModelNames;
  return global_p4_model_names;
}

}  // namespace

void SetP4ModelNames(const P4ModelNames& p4_model_names) {
  *GlobalP4ModelNames() = p4_model_names;
}

const P4ModelNames& GetP4ModelNames() {
  return *GlobalP4ModelNames();
}

void SetUpTestP4ModelNames() {
  // The "ingress" and "egress" names don't match some test files, but it
  // should not matter for most tests.
  P4ModelNames p4_model_names;
  p4_model_names.set_ingress_control_name("ingress");
  p4_model_names.set_egress_control_name("egress");
  p4_model_names.set_drop_extern_name("mark_to_drop");
  p4_model_names.set_clone_extern_name("clone");
  p4_model_names.set_clone3_extern_name("clone3");
  p4_model_names.set_counter_extern_name("counter");
  p4_model_names.set_meter_extern_name("meter");
  p4_model_names.set_direct_counter_extern_name("direct_counter");
  p4_model_names.set_direct_meter_extern_name("direct_meter");
  p4_model_names.set_counter_count_method_name("count");
  p4_model_names.set_direct_counter_count_method_name("count");
  p4_model_names.set_meter_execute_method_name("execute_meter");
  p4_model_names.set_direct_meter_read_method_name("read");
  p4_model_names.set_color_enum_type("meter_color_t");
  p4_model_names.set_color_enum_green("COLOR_GREEN");
  p4_model_names.set_color_enum_yellow("COLOR_YELLOW");
  p4_model_names.set_color_enum_red("COLOR_RED");
  p4_model_names.set_clone_type_ingress_to_egress("I2E");
  p4_model_names.set_clone_type_egress_to_egress("E2E");
  p4_model_names.set_no_action("NoAction");
  p4_model_names.set_exact_match("exact");
  p4_model_names.set_lpm_match("lpm");
  p4_model_names.set_ternary_match("ternary");
  p4_model_names.set_range_match("range");
  p4_model_names.set_selector_match("selector");
  SetP4ModelNames(p4_model_names);
}

std::string AddHeaderArrayIndex(const std::string& header_name, int64 index) {
  DCHECK_LE(0, index) << "AddHeaderArrayIndex with negative index " << index;
  return absl::Substitute("$0[$1]", header_name.c_str(), index);
}

std::string AddHeaderArrayLast(const std::string& header_name) {
  return absl::Substitute(
      "$0.$1", header_name.c_str(), IR::Type_Stack::last.c_str());
}

bool IsParserEndState(const ParserState& state) {
  if (state.transition().next_state() == IR::ParserState::accept)
    return true;
  if (state.transition().next_state() == IR::ParserState::reject)
    return true;
  return false;
}

const hal::P4TableDescriptor& FindTableDescriptorOrDie(
    const std::string& table_name,
    const hal::P4PipelineConfig& p4_pipeline_config) {
  const hal::P4TableMapValue& table_map_value =
      gtl::FindOrDie(p4_pipeline_config.table_map(), table_name);
  CHECK(table_map_value.has_table_descriptor())
      << "Table map value with key " << table_name << " is not a table "
      << "descriptor: " << table_map_value.ShortDebugString();
  return table_map_value.table_descriptor();
}

hal::P4TableDescriptor* FindMutableTableDescriptorOrDie(
    const std::string& table_name, hal::P4PipelineConfig* p4_pipeline_config) {
  hal::P4TableMapValue& table_map_value =
      gtl::FindOrDie(*p4_pipeline_config->mutable_table_map(), table_name);
  CHECK(table_map_value.has_table_descriptor())
      << "Table map value with key " << table_name << " is not a table "
      << "descriptor: " << table_map_value.ShortDebugString();
  return table_map_value.mutable_table_descriptor();
}

const hal::P4ActionDescriptor& FindActionDescriptorOrDie(
    const std::string& action_name,
    const hal::P4PipelineConfig& p4_pipeline_config) {
  const hal::P4TableMapValue& table_map_value =
      gtl::FindOrDie(p4_pipeline_config.table_map(), action_name);
  CHECK(table_map_value.has_action_descriptor())
      << "Table map value with key " << action_name << " is not an action "
      << "descriptor: " << table_map_value.ShortDebugString();
  return table_map_value.action_descriptor();
}

hal::P4ActionDescriptor* FindMutableActionDescriptorOrDie(
    const std::string& action_name, hal::P4PipelineConfig* p4_pipeline_config) {
  hal::P4TableMapValue& table_map_value =
      gtl::FindOrDie(*p4_pipeline_config->mutable_table_map(), action_name);
  CHECK(table_map_value.has_action_descriptor())
      << "Table map value with key " << action_name << " is not an action "
      << "descriptor: " << table_map_value.ShortDebugString();
  return table_map_value.mutable_action_descriptor();
}

const hal::P4HeaderDescriptor& FindHeaderDescriptorOrDie(
    const std::string& header_name,
    const hal::P4PipelineConfig& p4_pipeline_config) {
  const hal::P4TableMapValue& table_map_value =
      gtl::FindOrDie(p4_pipeline_config.table_map(), header_name);
  CHECK(table_map_value.has_header_descriptor())
      << "Table map value with key " << header_name << " is not a header "
      << "descriptor: " << table_map_value.ShortDebugString();
  return table_map_value.header_descriptor();
}

// This function is useful for finding a field's header descriptor when the
// field name and its header type are known, but the header name is unknown.
// The typical use case is finding a header descriptor that corresponds to
// a field descriptor's header_type value.
const hal::P4HeaderDescriptor& FindHeaderDescriptorForFieldOrDie(
    const std::string& field_name,
    P4HeaderType header_type,
    const hal::P4PipelineConfig& p4_pipeline_config) {
  const hal::P4HeaderDescriptor* header_descriptor = nullptr;
  for (const auto& table_map_iter : p4_pipeline_config.table_map()) {
    if (!table_map_iter.second.has_header_descriptor())
      continue;
    if (table_map_iter.second.header_descriptor().type() != header_type)
      continue;
    if (field_name.find(table_map_iter.first) != 0)
      continue;
    header_descriptor = &table_map_iter.second.header_descriptor();
    break;
  }

  CHECK(header_descriptor != nullptr)
      << "No header descriptor with type " << P4HeaderType_Name(header_type)
      << " matches field " << field_name;
  return *header_descriptor;
}

const hal::P4FieldDescriptor* FindFieldDescriptorOrNull(
    const std::string& field_name,
    const hal::P4PipelineConfig& p4_pipeline_config) {
  const hal::P4TableMapValue* table_map_value =
      gtl::FindOrNull(p4_pipeline_config.table_map(), field_name);
  if (table_map_value == nullptr) return nullptr;
  if (!table_map_value->has_field_descriptor()) return nullptr;
  return &table_map_value->field_descriptor();
}

hal::P4FieldDescriptor* FindMutableFieldDescriptorOrNull(
    const std::string& field_name, hal::P4PipelineConfig* p4_pipeline_config) {
  hal::P4TableMapValue* table_map_value =
      gtl::FindOrNull(*p4_pipeline_config->mutable_table_map(), field_name);
  if (table_map_value == nullptr) return nullptr;
  if (!table_map_value->has_field_descriptor()) return nullptr;
  return table_map_value->mutable_field_descriptor();
}

}  // namespace p4c_backends
}  // namespace stratum
