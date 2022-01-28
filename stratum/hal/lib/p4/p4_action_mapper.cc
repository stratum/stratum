// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// This file contains the P4ActionMapper implementation.

#include "stratum/hal/lib/p4/p4_action_mapper.h"

#include <memory>

#include "absl/memory/memory.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/gtl/stl_util.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {

P4ActionMapper::P4ActionMapper(const P4PipelineConfig& p4_pipeline_config)
    : p4_pipeline_config_(p4_pipeline_config) {}

P4ActionMapper::~P4ActionMapper() { gtl::STLDeleteValues(&action_map_); }

::util::Status P4ActionMapper::AddP4Actions(
    const P4InfoManager& p4_info_manager) {
  if (!action_map_.empty()) {
    return MAKE_ERROR(ERR_INTERNAL)
           << __PRETTY_FUNCTION__
           << " has already processed this P4PipelineConfig";
  }

  // This loop finds the p4_pipeline_config_ action descriptor for each
  // P4Info action, then creates an action_map_ to help find internal
  // actions for P4Runtime requests.
  ::util::Status action_add_status = ::util::OkStatus();
  for (const auto& action : p4_info_manager.p4_info().actions()) {
    auto action_status = GetTableMapValueWithDescriptorCase(
        p4_pipeline_config_, action.preamble().name(),
        P4TableMapValue::kActionDescriptor, "");
    APPEND_STATUS_IF_ERROR(action_add_status, action_status.status());
    if (!action_status.status().ok()) continue;
    const P4ActionDescriptor* original_action =
        &action_status.ValueOrDie()->action_descriptor();
    auto new_map_entry = absl::make_unique<ActionMapEntry>(original_action);

    // Each internal_link in the original action is recorded in new_map_entry.
    for (const auto& action_redirect : original_action->action_redirects()) {
      for (const auto& internal_link : action_redirect.internal_links()) {
        auto internal_status = GetTableMapValueWithDescriptorCase(
            p4_pipeline_config_, internal_link.internal_action_name(),
            P4TableMapValue::kInternalAction, action.preamble().name());
        APPEND_STATUS_IF_ERROR(action_add_status, internal_status.status());
        if (!internal_status.status().ok()) continue;
        const P4ActionDescriptor& internal_action =
            internal_status.ValueOrDie()->internal_action();
        if (internal_link.applied_tables_size() == 0) {
          auto status = AddAction(internal_action, new_map_entry.get());
          APPEND_STATUS_IF_ERROR(action_add_status, status);
        } else {
          auto status =
              AddAppliedTableAction(p4_info_manager, internal_link,
                                    internal_action, new_map_entry.get());
          APPEND_STATUS_IF_ERROR(action_add_status, status);
        }
      }
    }

    action_map_[action.preamble().id()] = new_map_entry.release();
  }

  return action_add_status;
}

::util::StatusOr<const P4ActionDescriptor*>
P4ActionMapper::MapActionIDAndTableID(uint32 action_id, uint32 table_id) const {
  return nullptr;  // TODO(teverman): Add implementation.
}

::util::StatusOr<const P4ActionDescriptor*> P4ActionMapper::MapActionID(
    uint32 action_id) const {
  return nullptr;  // TODO(teverman): Add implementation.
}

// Each map_entry only supports one unconditional, non-table specific link
// to an internal action.  The error should normally be caught during pipeline
// config verification.
::util::Status P4ActionMapper::AddAction(
    const P4ActionDescriptor& internal_action, ActionMapEntry* map_entry) {
  if (map_entry->internal_action != nullptr) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Unexpected multiple links to internal actions - discarding "
           << internal_action.ShortDebugString();
  }
  map_entry->internal_action = &internal_action;

  return ::util::OkStatus();
}

// For table-specific internal actions, the ActionMapEntry's
// qualified_tables_map needs to be updated to map the P4 table ID to
// the corresponding internal action.
::util::Status P4ActionMapper::AddAppliedTableAction(
    const P4InfoManager& p4_info_manager,
    const P4ActionDescriptor::P4InternalActionLink& internal_link,
    const P4ActionDescriptor& internal_action, ActionMapEntry* map_entry) {
  ::util::Status table_action_status = ::util::OkStatus();
  for (const auto& table_name : internal_link.applied_tables()) {
    auto table_status = p4_info_manager.FindTableByName(table_name);
    APPEND_STATUS_IF_ERROR(table_action_status, table_status.status());
    if (!table_status.status().ok()) continue;
    if (!gtl::InsertIfNotPresent(&map_entry->qualified_tables_map,
                                 table_status.ValueOrDie().preamble().id(),
                                 &internal_action)) {
      ::util::Status error = MAKE_ERROR(ERR_INTERNAL)
                             << "Unexpected duplicate appearance of table "
                             << table_name << " in internal action links";
      APPEND_STATUS_IF_ERROR(table_action_status, error);
    }
  }

  return table_action_status;
}

}  // namespace hal
}  // namespace stratum
