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

// This file contains the HiddenTableMapper implementation.

#include "stratum/p4c_backends/fpm/hidden_table_mapper.h"

#include <memory>
#include <set>

#include "base/logging.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "absl/memory/memory.h"
#include "sandblaze/p4lang/p4/v1/p4runtime.host.pb.h"
#include "util/gtl/map_util.h"
#include "util/gtl/stl_util.h"

namespace stratum {
namespace p4c_backends {

HiddenTableMapper::~HiddenTableMapper() {
  gtl::STLDeleteValues(&meta_key_map_);
}

void HiddenTableMapper::ProcessTables(
    const hal::P4InfoManager& p4_info_manager,
    hal::P4PipelineConfig* p4_pipeline_cfg) {
  // If p4_pipeline_cfg has no static table entries, there is no reason
  // to continue.
  if (p4_pipeline_cfg->static_table_entries().updates_size() == 0) {
    VLOG(1) << "Skipping hidden table processing - no static tables "
            << "in P4PipelineConfig";
    return;
  }

  // MetaKeyMapper has already identified metadata fields that act as a
  // match key for some table.  This loop evaluates the tables referenced
  // by these fields for initial qualification as IndirectActionKeys.
  for (const auto& iter : p4_pipeline_cfg->table_map()) {
    if (!iter.second.has_field_descriptor()) continue;
    const auto& field_descriptor = iter.second.field_descriptor();
    for (const auto& metadata_key : field_descriptor.metadata_keys()) {
      auto p4_table_status =
          p4_info_manager.FindTableByName(metadata_key.table_name());
      CHECK(p4_table_status.status().ok())
          << "Unexpected failure of P4Info lookup for table "
          << metadata_key.table_name();
      CheckTableForIndirectActionKey(p4_table_status.ValueOrDie(),
                                     *p4_pipeline_cfg);
    }
  }

  // This loop processes all actions in p4_pipeline_cfg to find assignments
  // to metadata fields that act as indirect action keys.
  for (const auto& iter : meta_key_map_) {
    iter.second->FindActions(*p4_pipeline_cfg);
  }

  // TODO(teverman): Add more checks for disqualifying uses:
  //  - Decide how to behave if a key has a mix of uses in qualified and
  //    disqualified tables.

  // When this loop runs, meta_key_map_ contains all qualifying instances
  // of IndirectActionKey.  Field descriptor data needs to be updated for
  // each entry.
  for (const auto& iter : meta_key_map_) {
    if (iter.second->disqualified()) continue;
    if (iter.second->qualified_tables().empty()) continue;
    hal::P4FieldDescriptor* field_descriptor =
        FindMutableFieldDescriptorOrNull(iter.first, p4_pipeline_cfg);
    DCHECK(field_descriptor != nullptr)
        << "Missing field descriptor for " << iter.first;
    *field_descriptor = iter.second->new_field_descriptor();
    VLOG(5) << field_descriptor->DebugString();
  }
}

void HiddenTableMapper::CheckTableForIndirectActionKey(
    const ::p4::config::v1::Table& p4_table,
    const hal::P4PipelineConfig& p4_pipeline_cfg) {
  // The current implementation considers only tables with one local
  // metadata match field.  This limitation works for all tables in current
  // P4 programs.  With some additional complexity, this technique could
  // also be applied to tables with keys consisting of multiple local metadata
  // match fields, should the need arise.
  // TODO(teverman): What if the metadata field is used as a single key in
  // one table and in combination with other fields for another table's key?
  // It may be helpful to give a warning suggesting that the P4 program can
  // be optimized for Hercules switches by splitting the field in question.
  if (p4_table.match_fields_size() != 1) {
    return;
  }

  // Additional constraints on the input p4_table:
  // 1) It must be in a hidden pipeline stage.
  // 2) It must be programmed with static entries.
  const hal::P4TableDescriptor& table_descriptor =
      FindTableDescriptorOrDie(p4_table.preamble().name(), p4_pipeline_cfg);
  if (table_descriptor.pipeline_stage() != P4Annotation::HIDDEN) {
    return;
  }
  if (!table_descriptor.has_static_entries()) {
    return;
  }
  auto& match_field = p4_table.match_fields(0);
  CreateOrUpdateQualifiedKey(match_field, p4_table, p4_pipeline_cfg);
}

void HiddenTableMapper::CreateOrUpdateQualifiedKey(
    const ::p4::config::v1::MatchField& match_field,
    const ::p4::config::v1::Table& p4_table,
    const hal::P4PipelineConfig& p4_pipeline_cfg) {
  // A new provisional IndirectActionKey represents any field that makes it
  // this far.  If the field passes further qualification, meta_key_map_ is
  // updated with this new key entry, or it is merged into an existing entry
  // for the same field.
  auto new_meta_key = absl::make_unique<IndirectActionKey>(
      match_field.name(), &action_redirects_);
  if (!new_meta_key->QualifyKey(match_field, p4_table, p4_pipeline_cfg)) {
    return;
  }
  auto emplace_val =
      meta_key_map_.emplace(match_field.name(), new_meta_key.get());
  if (emplace_val.second) {
    new_meta_key.release();
    VLOG(5) << "New meta_key_map_ key " << match_field.name();
  } else {
    VLOG(5) << "Existing meta_key_map_ key " << match_field.name();
    auto meta_key_entry = emplace_val.first->second;
    meta_key_entry->Merge(*new_meta_key);
  }
}

// HiddenTableMapper::IndirectActionKey implementation starts here.
bool HiddenTableMapper::IndirectActionKey::QualifyKey(
    const ::p4::config::v1::MatchField& match_field,
    const ::p4::config::v1::Table& p4_table,
    const hal::P4PipelineConfig& p4_pipeline_cfg) {
  const hal::P4FieldDescriptor* field_descriptor =
      FindFieldDescriptorOrNull(field_name_, p4_pipeline_cfg);
  DCHECK(field_descriptor != nullptr)
      << "Missing field descriptor for " << field_name_;

  // Qualification 1: The field must be a local_metadata field.
  if (!field_descriptor->is_local_metadata()) {
    VLOG(5) << field_name_ << " is not part of local metadata";
    disqualified_ = true;
    return false;  // Disqualified for this table and all others.
  }

  // Qualification 2: The field type must thus far be unspecified.  Fields
  // with a known type for some other usage do not qualify.
  if (!IsFieldTypeUnspecified(*field_descriptor)) {
    VLOG(5) << field_name_ << " has a previously specified field type";
    disqualified_ = true;
    return false;  // Disqualified for this table and all others.
  }

  // Qualification 3: The field must be used as an EXACT match.  If not
  // EXACT for this table, this field may still qualify in other tables,
  // so the result is true, but the disqualified_tables_ insert is done
  // for this table.
  new_field_descriptor_ = *field_descriptor;
  new_field_descriptor_.set_type(P4_FIELD_TYPE_METADATA_MATCH);
  if (match_field.match_type() == ::p4::config::v1::MatchField::EXACT) {
    qualified_tables_.insert(p4_table.preamble().name());
  } else {
    VLOG(5) << field_name_ << " is not an EXACT match field";
  }

  return true;
}

void HiddenTableMapper::IndirectActionKey::FindActions(
    const hal::P4PipelineConfig& p4_pipeline_cfg) {
  // This could return immediately if already disqualified_, but continuing
  // may find additional errors and should do no harm.
  for (const auto& entry : p4_pipeline_cfg.table_map()) {
    if (entry.second.has_action_descriptor()) {
      const auto& action_descriptor = entry.second.action_descriptor();
      std::vector<int> assignment_indexes;
      FindAssignmentsToKey(entry.first, action_descriptor, &assignment_indexes);
      for (int index : assignment_indexes) {
        HandleKeyAssignment(
            entry.first, action_descriptor,
            action_descriptor.assignments(index).assigned_value());
      }
    }
  }
  RemoveAssignmentsToKey();
}

void HiddenTableMapper::IndirectActionKey::Merge(
    const IndirectActionKey& source_key) {
  qualified_tables_.insert(source_key.qualified_tables_.begin(),
                           source_key.qualified_tables_.end());
}

void HiddenTableMapper::IndirectActionKey::HandleKeyAssignment(
    const std::string& action_name,
    const hal::P4ActionDescriptor& old_action_descriptor,
    const P4AssignSourceValue& source_value) {
  int64 assigned_value = -1;
  if (source_value.source_value_case() == P4AssignSourceValue::kConstantParam) {
    assigned_value = source_value.constant_param();
  } else {
    // Even if disqualified here, the actions_assigns_ map emplace still occurs
    // below.  This enables detection of duplicate assignments to this key by
    // the same action.
    disqualified_ = true;
    LOG(ERROR) << "Action " << action_name << " sets indirect action key "
               << field_name_ << " to a non-constant value: "
               << source_value.ShortDebugString();
  }

  auto emplace_val = actions_assigns_.emplace(action_name, assigned_value);
  if (!emplace_val.second) {
    // If the action makes multiple assignments to this key, it is impossible
    // to tell at compile time which key value sticks as the action output
    // for the hidden table lookup.
    LOG(ERROR) << "Action " << action_name << " sets indirect action key "
               << field_name_ << " multiple times";
    disqualified_ = true;
  }

  // The action descriptor needs an action_redirects entry to represent this
  // IndirectActionKey.  In the rare case that this action has more than
  // one IndirectActionKey, a partially updated descriptor may already exist
  // in the action_redirects_ map.  In both cases, the most recent descriptor
  // goes into the action_redirects_ map.
  hal::P4ActionDescriptor new_action_descriptor;
  const hal::P4ActionDescriptor* updated_descriptor =
      gtl::FindOrNull(*action_redirects_, action_name);
  if (updated_descriptor != nullptr)
    new_action_descriptor = *updated_descriptor;
  else
    new_action_descriptor = old_action_descriptor;

  // The descriptor in action_redirects_ is not updated if either:
  // - this field is fully disqualified for any reason prior to the update.
  // - this field was not fully qualified for at least one table.
  // In either case, all previous actions that may have referenced this key
  // are removed from action_redirects_.
  if (disqualified_ || qualified_tables_.empty()) {
    for (const auto& erase_iter : actions_assigns_) {
      action_redirects_->erase(erase_iter.first);
    }
    return;
  }

  auto action_redirect = new_action_descriptor.add_action_redirects();
  action_redirect->set_key_field_name(field_name_);
  action_redirect->set_key_value(assigned_value);
  for (const auto& table : qualified_tables_) {
    action_redirect->add_internal_links()->set_hidden_table_name(table);
  }
  (*action_redirects_)[action_name] = new_action_descriptor;
}

void HiddenTableMapper::IndirectActionKey::FindAssignmentsToKey(
    const std::string& action_name,
    const hal::P4ActionDescriptor& descriptor,
    std::vector<int>* assignment_indexes) {
  for (int index = 0; index < descriptor.assignments_size(); ++index) {
    const auto& assignment = descriptor.assignments(index);
    DCHECK(!assignment.destination_field_name().empty());
    if (assignment.destination_field_name() == field_name_) {
      assignment_indexes->push_back(index);
    }
  }
}

// Any existing action descriptor assignments to the IndirectActionKey
// need to be removed; they have been replaced by a new P4ActionRedirect field.
void HiddenTableMapper::IndirectActionKey::RemoveAssignmentsToKey() {
  if (disqualified_) return;
  for (const auto& action_iter : actions_assigns_) {
    hal::P4ActionDescriptor* descriptor = gtl::FindOrNull(
        *action_redirects_, action_iter.first);
    if (descriptor == nullptr) continue;
    std::vector<int> assignment_indexes;
    FindAssignmentsToKey(action_iter.first, *descriptor, &assignment_indexes);
    DeleteRepeatedFields(assignment_indexes, descriptor->mutable_assignments());
  }
}

}  // namespace p4c_backends
}  // namespace stratum
