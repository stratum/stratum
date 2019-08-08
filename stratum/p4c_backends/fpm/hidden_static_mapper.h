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

// The HiddenStaticMapper combines the HiddenTableMapper's ActionRedirectMap
// output with the static table entries in the P4PipelineConfig.  Each redirect
// map entry specifies a key value for a local metadata field that acts as
// a hidden table match key.  A corresponding static entry for the hidden
// table with the same match key value indicates the action ID for the
// related hidden action.  The HiddenStaticMapper's role is form a new
// InternalAction that merges the redirecting action with the hidden action.
// The InternalAction becomes a single action for a Stratum switch physical
// table that combines the functionality of multiple P4 logical tables.

#ifndef STRATUM_P4C_BACKENDS_FPM_HIDDEN_STATIC_MAPPER_H_
#define STRATUM_P4C_BACKENDS_FPM_HIDDEN_STATIC_MAPPER_H_

#include <map>
#include <string>
#include <utility>

#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/p4c_backends/fpm/hidden_table_mapper.h"
#include "stratum/p4c_backends/fpm/tunnel_optimizer_interface.h"

namespace stratum {
namespace p4c_backends {

// The p4c backend invokes the HiddenStaticMapper after the HiddenTableMapper
// produces its ActionRedirectMap and the P4PipelineConfig is fully populated
// with action descriptors and static table entries.
class HiddenStaticMapper {
 public:
  // The constructor requires a P4InfoManager so HiddenStaticMapper can
  // translate between P4 object names and IDs.  The tunnel_optimizer does
  // target-specific work for tunnel actions in static entries.
  HiddenStaticMapper(const hal::P4InfoManager& p4_info_manager,
                     TunnelOptimizerInterface* tunnel_optimizer);
  virtual ~HiddenStaticMapper() {}

  // ProcessStaticEntries combines the data from action_redirect_map with
  // the static table entries in p4_pipeline_cfg.  Where applicable, it updates
  // the p4_pipeline_cfg with a new InternalAction that combines the behavior
  // of actions referenced by the action_redirect_map with actions referenced
  // by static table entries.  ProcessStaticEntries indicates errors through
  // p4c's ErrorReporter.
  void ProcessStaticEntries(
      const HiddenTableMapper::ActionRedirectMap& action_redirect_map,
      hal::P4PipelineConfig* p4_pipeline_cfg);

  // HiddenStaticMapper is neither copyable nor movable.
  HiddenStaticMapper(const HiddenStaticMapper&) = delete;
  HiddenStaticMapper& operator=(const HiddenStaticMapper&) = delete;

 private:
  // This type defines the lookup key for the private hidden_action_id_map_.
  // The first pair member is a P4 table ID, and the second pair member is
  // an exact match field value.
  typedef std::pair<uint32, uint64> HiddenActionKey;

  // Processes the static_entries from the P4PipelineConfig, building a map
  // to lookup potential hidden actions.  It makes a single pass through
  // static_entries to create the map, which contains essential data to
  // translate ActionRedirectMap entries into the action identified by the
  // static table entry.
  void BuildHiddenActionMap(const ::p4::v1::WriteRequest& static_entries);

  // Searches hidden_action_id_map_ for an action in the table identified
  // by hidden_table_name.  The key_field_value is the value of the local
  // metadata field that acts as the hidden table's match field.
  const std::string FindActionInStaticEntry(
      const std::string& hidden_table_name, uint64 key_field_value);

  // These members are injected via the constructor - not owned by this class.
  const hal::P4InfoManager& p4_info_manager_;
  TunnelOptimizerInterface* tunnel_optimizer_;

  // This map contains extracted data from the P4PipelineConfig's static table
  // entries.  It maps a HiddenActionKey to a P4 action ID.  It facilitates
  // searches for hidden table actions that correspond to ActionRedirectMap
  // attributes.
  std::map<HiddenActionKey, uint32> hidden_action_id_map_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_HIDDEN_STATIC_MAPPER_H_
