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

// This file implements the p4c backend's HiddenStaticMapper.

#include "stratum/p4c_backends/fpm/hidden_static_mapper.h"

#include <memory>

#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/p4/p4_match_key.h"
#include "stratum/p4c_backends/fpm/internal_action.h"
#include "stratum/p4c_backends/fpm/utils.h"

namespace stratum {
namespace p4c_backends {

HiddenStaticMapper::HiddenStaticMapper(
    const hal::P4InfoManager& p4_info_manager,
    TunnelOptimizerInterface* tunnel_optimizer)
    : p4_info_manager_(p4_info_manager),
      tunnel_optimizer_(ABSL_DIE_IF_NULL(tunnel_optimizer)) {}

void HiddenStaticMapper::ProcessStaticEntries(
    const HiddenTableMapper::ActionRedirectMap& action_redirect_map,
    hal::P4PipelineConfig* p4_pipeline_cfg) {
  if (action_redirect_map.empty()) return;
  BuildHiddenActionMap(p4_pipeline_cfg->static_table_entries());

  for (const auto& iter : action_redirect_map) {
    const hal::P4ActionDescriptor& redirecting_action = iter.second;
    bool link_internal_action = false;
    bool valid_internal_action = true;
    InternalAction internal_action(iter.first, redirecting_action,
                                   *p4_pipeline_cfg, tunnel_optimizer_);

    for (const auto& redirect : redirecting_action.action_redirects()) {
      for (const auto& internal_link : redirect.internal_links()) {
        // The applied_tables qualifier should not be present in action
        // redirects to hidden tables.  (They are only relevant when merging
        // P4 control logic into actions.)
        if (internal_link.applied_tables_size() > 0) {
          ::error(
              "Backend: Unexpected applied_tables constraint in action "
              "%s when mapping hidden table static entries: %s",
              iter.first.c_str(), internal_link.ShortDebugString().c_str());
          valid_internal_action = false;
          continue;
        }
        const std::string hidden_table_action = FindActionInStaticEntry(
            internal_link.hidden_table_name(), redirect.key_value());

        // Once the redirecting action matches a hidden table action, the latter
        // merges into the new InternalAction.  The link_internal_action flag
        // is also set to update the redirecting action's descriptor after
        // all possible hidden actions are merged.
        if (!hidden_table_action.empty()) {
          internal_action.MergeAction(hidden_table_action);
          link_internal_action = true;
        }
      }
    }

    // The redirecting action's descriptor is replaced if the loop above
    // linked it to a new InternalAction.
    if (link_internal_action && valid_internal_action) {
      internal_action.Optimize();
      internal_action.WriteToP4PipelineConfig(p4_pipeline_cfg);
      hal::P4ActionDescriptor::P4ActionRedirect new_redirect;
      *new_redirect.mutable_input_redirects() =
          redirecting_action.action_redirects();
      new_redirect.add_internal_links()->set_internal_action_name(
          internal_action.internal_name());
      hal::P4TableMapValue new_linked_action;
      *new_linked_action.mutable_action_descriptor() = redirecting_action;
      auto new_descriptor = new_linked_action.mutable_action_descriptor();
      new_descriptor->clear_action_redirects();
      *new_descriptor->add_action_redirects() = new_redirect;
      (*p4_pipeline_cfg->mutable_table_map())[iter.first] = new_linked_action;
    }
  }
}

// To avoid a search through the P4PipelineConfig's static entries while
// processing every ActionRedirectMap entry, this method makes one pass
// through the static entries and populates hidden_action_id_map_ with
// data for later lookup.  It only creates hidden_action_id_map_ entries
// for static table entries that are of potential interest for handling
// ActionRedirectMap entries.
void HiddenStaticMapper::BuildHiddenActionMap(
    const ::p4::v1::WriteRequest& static_entries) {
  for (const auto& static_entry : static_entries.updates()) {
    // Only static entries that insert table entries are relevant for subsequent
    // ActionRedirectMap processing.
    if (static_entry.type() != ::p4::v1::Update::INSERT) continue;
    if (!static_entry.entity().has_table_entry()) continue;

    // Action redirects only support match keys with one exact match field.
    const ::p4::v1::TableEntry& table_entry =
        static_entry.entity().table_entry();
    if (table_entry.match_size() != 1) continue;
    if (!table_entry.match(0).has_exact()) continue;
    const auto match_bytes = table_entry.match(0).exact().value().size();
    if (match_bytes > sizeof(uint64)) continue;

    // Actions with parameters are not eligible.  Stratum P4 programs do not
    // currently use parameters for hidden table actions.  If parameters
    // become necessary, one possible implementation would be to take the
    // parameter value from the table_entry and convert it into a constant
    // assignment for the InternalAction that this HiddenStaticMapper forms
    // later.
    if (!table_entry.action().has_action()) continue;
    if (table_entry.action().action().params_size() != 0) continue;

    // The match_key value must have a translation to a 64-bit integer.
    // To do this translation, the match value from the static entry with
    // native P4Runtime bit width must be padded to a 64-bit key for
    // internal use.
    ::p4::v1::FieldMatch match_pad_64 = table_entry.match(0);
    match_pad_64.mutable_exact()->set_value(
        std::string(sizeof(uint64) - match_bytes, 0) +
        table_entry.match(0).exact().value());
    std::unique_ptr<hal::P4MatchKey> match_key =
        hal::P4MatchKey::CreateInstance(match_pad_64);
    auto key_status = match_key->ConvertExactToUint64();
    if (!key_status.ok()) continue;

    // This static table entry meets the criteria for an action redirect,
    // so a hidden_action_id_map_ entry is created to refer to its P4 action ID.
    hidden_action_id_map_.emplace(
        std::make_pair(table_entry.table_id(), key_status.ValueOrDie()),
        table_entry.action().action().action_id());
  }
}

const std::string HiddenStaticMapper::FindActionInStaticEntry(
    const std::string& hidden_table_name, uint64 key_field_value) {
  auto table_status = p4_info_manager_.FindTableByName(hidden_table_name);
  DCHECK(table_status.ok());
  auto p4_info_table = table_status.ValueOrDie();

  HiddenActionKey hidden_action_key =
      std::make_pair(p4_info_table.preamble().id(), key_field_value);
  uint32* hidden_action_id =
      gtl::FindOrNull(hidden_action_id_map_, hidden_action_key);
  if (hidden_action_id == nullptr) {
    LOG(WARNING) << "Missing action ID in hidden_action_id_map_ for table "
                 << hidden_table_name << " and match key " << key_field_value;
    return "";
  }

  auto action_status = p4_info_manager_.FindActionByID(*hidden_action_id);
  DCHECK(action_status.ok());
  auto p4_info_action = action_status.ValueOrDie();
  return p4_info_action.preamble().name();
}

}  // namespace p4c_backends
}  // namespace stratum
