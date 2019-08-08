// Copyright 2018 Google LLC
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


// This file contains the implementation of P4StaticEntryMapper.

#include "stratum/hal/lib/p4/p4_static_entry_mapper.h"

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/p4/p4_table_mapper.h"
#include "stratum/hal/lib/p4/p4_write_request_differ.h"

// Stratum P4 programs contain some "hidden" tables with const entries.  These
// tables have no direct mapping to physical tables on the target switch.  They
// are often related to tables that occur earlier in the forwarding pipeline.
// For example, P4 encap/decap is spread across two tables, where the first
// table decides the type of encap/decap, and a later table does the actual
// header pushes or pops.  In Stratum, the second table is hidden, and when
// FLAGS_remap_hidden_table_const_entries is true, the switch stack combines
// actions from its const entries into the actions for the first table.
// When the flag is false, the switch stack treats const entries in hidden
// tables like any other const entry.
DEFINE_bool(remap_hidden_table_const_entries, true, "Enables/disables "
            "remapping of hidden table const entries.  When enabled, the "
            "hidden const entry actions are integrated into the actions of "
            "related physical tables.");

namespace stratum {
namespace hal {

P4StaticEntryMapper::P4StaticEntryMapper(P4TableMapper* p4_table_mapper)
    : p4_table_mapper_(ABSL_DIE_IF_NULL(p4_table_mapper)) {}

P4StaticEntryMapper::P4StaticEntryMapper() : p4_table_mapper_(nullptr) {}

::util::Status P4StaticEntryMapper::HandlePrePushChanges(
    const ::p4::v1::WriteRequest& new_static_config,
    ::p4::v1::WriteRequest* out_request) {
  out_request->Clear();
  ::p4::v1::WriteRequest physical_request;
  ::p4::v1::WriteRequest hidden_request;
  RETURN_IF_ERROR(
      SplitRequest(new_static_config, &physical_request, &hidden_request));

  // Physical static entries that have been deleted relative to the current
  // pipeline config are identified here.  Static entry additions and
  // modifications are not applicable during the pre-push step.
  ::p4::v1::WriteRequest physical_deletes;
  ::p4::v1::WriteRequest physical_unchanged;
  P4WriteRequestDiffer physical_differ(
      physical_static_entries_, physical_request);
  RETURN_IF_ERROR(physical_differ.Compare(
      &physical_deletes, nullptr, nullptr, &physical_unchanged));

  // Hidden static entries that have been deleted relative to the current
  // pipeline config are identified here.  Static entry additions and
  // modifications are not applicable during the pre-push step.
  ::p4::v1::WriteRequest hidden_deletes;
  ::p4::v1::WriteRequest hidden_unchanged;
  P4WriteRequestDiffer hidden_differ(hidden_static_entries_, hidden_request);
  RETURN_IF_ERROR(hidden_differ.Compare(
      &hidden_deletes, nullptr, nullptr, &hidden_unchanged));

  // TODO(unknown): Finish implementation - hidden_deletes need to be saved
  // with internal hidden tables.
  physical_static_entries_ = physical_unchanged;
  hidden_static_entries_ = hidden_unchanged;
  *out_request = physical_deletes;

  return ::util::OkStatus();
}

::util::Status P4StaticEntryMapper::HandlePostPushChanges(
    const ::p4::v1::WriteRequest& new_static_config,
    ::p4::v1::WriteRequest* out_request) {
  out_request->Clear();
  ::p4::v1::WriteRequest physical_request;
  ::p4::v1::WriteRequest hidden_request;
  RETURN_IF_ERROR(
      SplitRequest(new_static_config, &physical_request, &hidden_request));

  // Physical static entries that have been added or modified in the new
  // pipeline config are identified here.  Static entry deletions should have
  // already been handled by HandlePrePushChanges.
  ::p4::v1::WriteRequest physical_deletes;
  ::p4::v1::WriteRequest physical_adds;
  ::p4::v1::WriteRequest physical_mods;
  P4WriteRequestDiffer physical_differ(
      physical_static_entries_, physical_request);
  RETURN_IF_ERROR(physical_differ.Compare(
      &physical_deletes, &physical_adds, &physical_mods, nullptr));
  CHECK_RETURN_IF_FALSE(physical_deletes.updates_size() == 0)
      << "Unexpected physical static table entry deletions - possible "
      << "P4StaticEntryMapper API misuse: "
      << physical_deletes.ShortDebugString();

  // Hidden static entries that have been added or modified in the new
  // pipeline config are identified here.  Static entry deletions should have
  // already been handled by HandlePrePushChanges.
  ::p4::v1::WriteRequest hidden_deletes;
  ::p4::v1::WriteRequest hidden_adds;
  ::p4::v1::WriteRequest hidden_mods;
  P4WriteRequestDiffer hidden_differ(hidden_static_entries_, hidden_request);
  RETURN_IF_ERROR(hidden_differ.Compare(
      &hidden_deletes, &hidden_adds, &hidden_mods, nullptr));
  CHECK_RETURN_IF_FALSE(hidden_deletes.updates_size() == 0)
      << "Unexpected hidden static table entry deletions - possible "
      << "P4StaticEntryMapper API misuse: "
      << hidden_deletes.ShortDebugString();

  // TODO(unknown): Finish implementation - hidden_adds need to be saved with
  // internal hidden tables; This class also needs more sophistication
  // to handle the modified output from P4WriteRequestDiffer.
  physical_static_entries_ = physical_request;
  hidden_static_entries_ = hidden_request;
  *out_request = physical_adds;

  return ::util::OkStatus();
}

::util::Status P4StaticEntryMapper::SplitRequest(
    const ::p4::v1::WriteRequest& new_request,
    ::p4::v1::WriteRequest* physical_request,
    ::p4::v1::WriteRequest* hidden_request) {
  for (const auto& update : new_request.updates()) {
    CHECK_RETURN_IF_FALSE(update.entity().has_table_entry())
        << "Static update in P4 WriteRequest has no table_entry: "
        << update.ShortDebugString();
    int table_id = update.entity().table_entry().table_id();

    auto hidden_status = p4_table_mapper_->IsTableStageHidden(table_id);
    ::p4::v1::Update* out_update = nullptr;
    if (hidden_status == TRI_STATE_UNKNOWN) {
      // TRI_STATE_UNKNOWN is not an error when called from pre-push because
      // the table_id may represent a new table in the pipeline config that
      // is being pushed.
      // TODO(unknown): TRI_STATE_UNKNOWN should probably be an error in the
      // post-push for adding new entries.
      continue;
    } else if (hidden_status == TRI_STATE_TRUE &&
        FLAGS_remap_hidden_table_const_entries) {
      out_update = hidden_request->add_updates();
    } else {
      out_update = physical_request->add_updates();
    }
    *out_update = update;
  }
  return ::util::OkStatus();
}

}  // namespace hal
}  // namespace stratum
