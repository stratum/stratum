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

// This file provides the AnnotationMapper implementation.

#include "stratum/p4c_backends/fpm/annotation_mapper.h"

#include <string>
#include <vector>
#include "gflags/gflags.h"
#include "stratum/hal/lib/p4/common_flow_entry.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "absl/strings/str_split.h"

DEFINE_string(p4c_annotation_map_files, "",
              "Specifies a comma-separated list of files for annotation "
              "lookup and post-processing of the P4PipelineConfig output.");

namespace stratum {
namespace p4c_backends {

AnnotationMapper::AnnotationMapper() : initialized_(false) {
  for (int stage = P4Annotation::PipelineStage_MIN;  // NOLINT
       stage <= P4Annotation::PipelineStage_MAX; ++stage) {
    if (P4Annotation::PipelineStage_IsValid(stage)) {
      pipeline_stage_usage_[static_cast<P4Annotation::PipelineStage>(stage)] =
          false;
    }
  }
  pipeline_stage_usage_[P4Annotation::DEFAULT_STAGE] = true;
}

bool AnnotationMapper::Init() {
  if (initialized_) {
    LOG(ERROR) << "AnnotationMapper is already initialized";
    return false;
  }

  // The annotations mapping data can be split across multiple files.
  // The code below merges each file's data into a single P4AnnotationMap.
  bool files_ok = true;
  if (!FLAGS_p4c_annotation_map_files.empty()) {
    std::vector<std::string> files =
        absl::StrSplit(FLAGS_p4c_annotation_map_files, ',');
    for (const auto& file : files) {
      P4AnnotationMap temp_map;
      if (ReadProtoFromTextFile(file, &temp_map).ok()) {
        annotation_map_.MergeFrom(temp_map);
      } else {
        LOG(ERROR) << "Failed parsing annotation map file: " << file;
        files_ok = false;
      }
    }
  }

  return files_ok && InitInternal();
}

bool AnnotationMapper::InitFromP4AnnotationMap(
    const P4AnnotationMap& annotation_map) {
  if (initialized_) {
    LOG(ERROR) << "AnnotationMapper is already initialized";
    return false;
  }
  annotation_map_ = annotation_map;
  return InitInternal();
}

bool AnnotationMapper::ProcessAnnotations(
    const hal::P4InfoManager& p4_info_manager,
    hal::P4PipelineConfig* p4_pipeline_config) {
  if (!initialized_) {
    LOG(ERROR) << "Attempt to map P4 annotations without initializing";
    return false;
  }

  bool success = true;
  for (auto& table_map_iter : *p4_pipeline_config->mutable_table_map()) {
    switch (table_map_iter.second.descriptor_case()) {
      case hal::P4TableMapValue::kTableDescriptor:
        if (!HandleTableAnnotations(
                table_map_iter.first, p4_info_manager,
                table_map_iter.second.mutable_table_descriptor()))
          success = false;
        break;
      case hal::P4TableMapValue::kFieldDescriptor:
        if (!HandleFieldAnnotations(
            table_map_iter.first,
            table_map_iter.second.mutable_field_descriptor()))
          success = false;
        break;
      case hal::P4TableMapValue::kActionDescriptor:
        if (!HandleActionAnnotations(
            table_map_iter.first, p4_info_manager,
            table_map_iter.second.mutable_action_descriptor()))
          success = false;
      case hal::P4TableMapValue::kHeaderDescriptor:
        break;
      case hal::P4TableMapValue::kInternalAction:
        // Internal actions will never be annotated.
        break;
      case hal::P4TableMapValue::DESCRIPTOR_NOT_SET:
        LOG(WARNING) << "P4PipelineConfig::table_map entry with key "
                     << table_map_iter.first << " has no valid descriptor data";
        break;
    }
  }

  // The pipeline_stage_usage_ map now reflects the outcome of processing
  // all @switchstack annotations, so it can be used to set idle_pipeline_stages
  // in the P4PipelineConfig.
  if (!success) return false;
  for (const auto iter : pipeline_stage_usage_) {
    if (!iter.second) p4_pipeline_config->add_idle_pipeline_stages(iter.first);
  }

  return success;
}

  // ProcessAnnotations will use the lookup maps created below.
bool AnnotationMapper::InitInternal() {
  bool field_ok = field_lookup_.BuildMap(annotation_map_.field_addenda());
  bool table_ok = table_lookup_.BuildMap(annotation_map_.table_addenda());
  bool action_ok = action_lookup_.BuildMap(annotation_map_.action_addenda());
  initialized_ = field_ok && table_ok && action_ok;

  return initialized_;
}

bool AnnotationMapper::HandleFieldAnnotations(
    const std::string& field_name, hal::P4FieldDescriptor* field_descriptor) {
  // The name is always the first annotation lookup.
  bool name_ok = MapFieldAnnotation(field_name, field_descriptor);
  return name_ok;
}

bool AnnotationMapper::MapFieldAnnotation(
    const std::string& annotation, hal::P4FieldDescriptor* field_descriptor) {
  const auto& iter = annotation_map_.field_addenda_map().find(annotation);
  if (iter == annotation_map_.field_addenda_map().end())
    return true;  // It is OK not to have a matching annotation.

  const P4FieldAnnotationValue& map_value = iter->second;
  if (map_value.type() != P4_FIELD_TYPE_UNKNOWN)
    field_descriptor->set_type(map_value.type());

  for (const auto& addenda_name : map_value.addenda_names()) {
    auto field_addendum = field_lookup_.FindAddenda(addenda_name);
    if (field_addendum == nullptr) {
      LOG(ERROR) << "Unable to find field addenda named "
                 << addenda_name << " for annotation " << annotation;
      return false;
    }
    LOG(WARNING) << "P4FieldAddenda are not implemented for " << annotation;
  }

  return true;
}

bool AnnotationMapper::HandleTableAnnotations(
    const std::string& table_name, const hal::P4InfoManager& p4_info_manager,
    hal::P4TableDescriptor* table_descriptor) {
  // The name is always the first annotation lookup.
  bool name_ok = MapTableAnnotation(table_name, table_descriptor);

  // If present, @switchstack annotations directly specify table attributes
  // without the need to do an annotation map lookup.
  auto status = p4_info_manager.GetSwitchStackAnnotations(table_name);
  if (status.ok()) {
    const P4Annotation& annotation = status.ValueOrDie();
    if (annotation.pipeline_stage() != P4Annotation::DEFAULT_STAGE) {
      table_descriptor->set_pipeline_stage(annotation.pipeline_stage());
      pipeline_stage_usage_[annotation.pipeline_stage()] = true;
    }
  }
  return name_ok && status.ok();
}

bool AnnotationMapper::MapTableAnnotation(
    const std::string& annotation, hal::P4TableDescriptor* table_descriptor) {
  const auto& iter = annotation_map_.table_addenda_map().find(annotation);
  if (iter == annotation_map_.table_addenda_map().end())
    return true;  // It is OK not to have a matching annotation.

  const P4TableAnnotationValue& map_value = iter->second;
  if (map_value.type() != P4_TABLE_UNKNOWN)
    table_descriptor->set_type(map_value.type());

  for (const auto& addenda_name : map_value.addenda_names()) {
    auto table_addendum = table_lookup_.FindAddenda(addenda_name);
    if (table_addendum == nullptr) {
      LOG(ERROR) << "Unable to find table addenda named "
                 << addenda_name << " for annotation " << annotation;
      return false;
    }

    // Each device_data field and internal_match_fields entry from the
    // annotation map is appended to the table_descriptor.
    for (const auto& device_data : table_addendum->device_data()) {
      *table_descriptor->add_device_data() = device_data;
    }
    for (const auto& match : table_addendum->internal_match_fields()) {
      *table_descriptor->add_internal_match_fields() = match;
    }
  }

  return true;
}

bool AnnotationMapper::HandleActionAnnotations(const std::string& action_name,
                            const hal::P4InfoManager& p4_info_manager,
                            hal::P4ActionDescriptor* action_descriptor) {
  // The name is always the first annotation lookup.
  bool name_ok = MapActionAnnotation(action_name, action_descriptor);
  return name_ok;
}

bool AnnotationMapper::MapActionAnnotation(const std::string& annotation,
                          hal::P4ActionDescriptor* action_descriptor) {
  const auto& iter = annotation_map_.action_addenda_map().find(annotation);
  if (iter == annotation_map_.action_addenda_map().end())
    return true;  // It is OK not to have a matching annotation.

  const P4ActionAnnotationValue& map_value = iter->second;
  if (map_value.type() != P4_ACTION_TYPE_UNKNOWN)
    action_descriptor->set_type(map_value.type());

  for (const auto& addenda_name : map_value.addenda_names()) {
    auto action_addendum = action_lookup_.FindAddenda(addenda_name);
    if (action_addendum == nullptr) {
      LOG(ERROR) << "Unable to find action addenda named "
                 << addenda_name << " for annotation " << annotation;
      return false;
    }

    if (action_addendum->assignment_primitive_replace()) {
      action_descriptor->clear_assignments();
      action_descriptor->clear_primitive_ops();
    }

    // Each device_data field from the annotation map is appended
    // to the action_descriptor.
    for (const auto& device_data : action_addendum->device_data()) {
      *action_descriptor->add_device_data() = device_data;
    }

    if (action_addendum->has_assignments_addenda()) {
      *action_descriptor->add_assignments() = action_addendum->assignments_addenda();
    }
    if (action_addendum->primitive_ops_addenda()) {
      action_descriptor->add_primitive_ops(action_addendum->primitive_ops_addenda());
    }
  }

  return true;
}

}  // namespace p4c_backends
}  // namespace stratum
