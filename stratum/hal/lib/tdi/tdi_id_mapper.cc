// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/tdi/tdi_id_mapper.h"

#include <vector>

#include "absl/strings/match.h"
#include "nlohmann/json.hpp"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/hal/lib/tdi/macros.h"
#include "stratum/hal/lib/tdi/tdi_constants.h"

#ifdef DPDK_TARGET
#include "tdi_rt/tdi_rt_defs.h"
#elif TOFINO_TARGET
#include "tdi_tofino/tdi_tofino_defs.h"
#else
#error "Unknown backend"
#endif

namespace stratum {
namespace hal {
namespace tdi {

TdiIdMapper::TdiIdMapper()
    : tdi_to_p4info_id_(),
      p4info_to_tdi_id_(),
      act_profile_to_selector_mapping_(),
      act_selector_to_profile_mapping_() {}

std::unique_ptr<TdiIdMapper> TdiIdMapper::CreateInstance() {
  return absl::WrapUnique(new TdiIdMapper());
}

::util::Status TdiIdMapper::PushForwardingPipelineConfig(
    const TdiDeviceConfig& config, const ::tdi::TdiInfo* tdi_info) {
  absl::WriterMutexLock l(&lock_);

  // Builds mapping between p4info and TDI info
  // In most cases, such as table id, we don't really need to map
  // from p4info ID to TDI ID.
  // However for some cases, like externs which does not exists
  // in native P4 core headers, the frontend compiler will
  // generate different IDs between p4info and TDI info.
  for (const auto& program : config.programs()) {
    // Try to find P4 tables from TDI info
    for (const auto& table : program.p4info().tables()) {
      RETURN_IF_ERROR(BuildMapping(table.preamble().id(),
                                   table.preamble().name(), tdi_info));
    }

    // Action profiles
    for (const auto& action_profile : program.p4info().action_profiles()) {
      RETURN_IF_ERROR(BuildMapping(action_profile.preamble().id(),
                                   action_profile.preamble().name(), tdi_info));
    }
    // FIXME(Yi): We need to scan all context.json to build correct mapping for
    // ActionProfiles and ActionSelectors. We may remove this workaround in the
    // future.
    for (const auto& pipeline : program.pipelines()) {
      RETURN_IF_ERROR(BuildActionProfileMapping(program.p4info(), tdi_info,
                                                pipeline.context()));
    }

    // Externs
    for (const auto& p4extern : program.p4info().externs()) {
      // TODO(Yi): Now we only support ActionProfile and ActionSelector
      // Things like DirectCounter are not listed as a table in tdi.json
      if (p4extern.extern_type_id() != kTnaExternActionProfileId &&
          p4extern.extern_type_id() != kTnaExternActionSelectorId) {
        continue;
      }
      for (const auto& extern_instance : p4extern.instances()) {
        RETURN_IF_ERROR(BuildMapping(extern_instance.preamble().id(),
                                     extern_instance.preamble().name(),
                                     tdi_info));
      }
    }

    // Indirect counters
    for (const auto& counter : program.p4info().counters()) {
      RETURN_IF_ERROR(BuildMapping(counter.preamble().id(),
                                   counter.preamble().name(), tdi_info));
    }

    // Registers
    for (const auto& register_entry : program.p4info().registers()) {
      RETURN_IF_ERROR(BuildMapping(register_entry.preamble().id(),
                                   register_entry.preamble().name(), tdi_info));
    }

    // Meters
    for (const auto& meter_entry : program.p4info().meters()) {
      RETURN_IF_ERROR(BuildMapping(meter_entry.preamble().id(),
                                   meter_entry.preamble().name(), tdi_info));
    }
  }

  return ::util::OkStatus();
}

::util::Status TdiIdMapper::BuildMapping(uint32 p4info_id,
                                         std::string p4info_name,
                                         const ::tdi::TdiInfo* tdi_info) {
  const ::tdi::Table* table;
  auto tdi_status = tdi_info->tableFromIdGet(p4info_id, &table);

  if (tdi_status == TDI_SUCCESS) {
    // Both p4info and TDI json use the same id for a specific
    // table/action selector/profile
    p4info_to_tdi_id_[p4info_id] = p4info_id;
    tdi_to_p4info_id_[p4info_id] = p4info_id;
    return ::util::OkStatus();
  }

  // Unable to find table by id because TDI uses a different id; we
  // can try to search it by name.
  tdi_status = tdi_info->tableFromNameGet(p4info_name, &table);
  if (tdi_status == TDI_SUCCESS) {
    // Table can be found with the given name, but they uses different IDs
    // We need to store mapping so we can map them later.
    tdi_id_t table_id = table->tableInfoGet()->idGet();
    p4info_to_tdi_id_[p4info_id] = table_id;
    tdi_to_p4info_id_[table_id] = p4info_id;
    return ::util::OkStatus();
  }

  // Special case: TDI includes pipeline name as prefix (e.g., "pipe."), but
  // p4info doesn't. We need to scan all tables to see if there is a table
  // called "[pipeline name].[P4 info table name]"
  std::vector<const ::tdi::Table*> tdi_tables;
  RETURN_IF_TDI_ERROR(tdi_info->tablesGet(&tdi_tables));
  for (const auto* table : tdi_tables) {
    tdi_id_t table_id;
    std::string table_name;
    table_id = table->tableInfoGet()->idGet();
    table_name = table->tableInfoGet()->nameGet();
    if (absl::StrContains(table_name, p4info_name)) {
      p4info_to_tdi_id_[p4info_id] = table_id;
      tdi_to_p4info_id_[table_id] = p4info_id;
      return ::util::OkStatus();
    }
  }
  return MAKE_ERROR(ERR_INTERNAL)
         << "Unable to find TDI ID for P4Info entity " << p4info_name
         << " with ID " << p4info_id << ".";
}

::util::Status TdiIdMapper::BuildActionProfileMapping(
    const p4::config::v1::P4Info& p4info, const ::tdi::TdiInfo* tdi_info,
    const std::string& context_json_content) {
  absl::flat_hash_map<std::string, std::string> prof_to_sel;
  try {
    nlohmann::json context_json =
        nlohmann::json::parse(context_json_content, nullptr, false);
    RET_CHECK(!context_json.is_discarded()) << "Failed to parse context.json";

    // Builds mappings for ActionProfile and ActionSelector.
    for (const auto& table : context_json["tables"]) {
      if (!table.contains("action_profile")) {
        continue;
      }
      const auto& action_profile_name = table["action_profile"];
      if (action_profile_name.empty()) {
        // Skip the table if there is no ActionProfile supported.
        continue;
      }

      if (!table.contains("selection_table_refs")) {
        continue;
      }
      const auto& selection_table_refs = table["selection_table_refs"];
      if (selection_table_refs.empty()) {
        // Skip the table if it supports ActionProfile only, since we don't need
        // to create the mapping for this table.
        continue;
      }

      const auto& action_selector_name = selection_table_refs[0]["name"];
      RET_CHECK(!action_selector_name.empty())
          << "ActionSelector for ActionProfile " << action_profile_name
          << " name is empty, this should not happened";
      RET_CHECK(gtl::InsertIfNotPresent(&prof_to_sel, action_profile_name,
                                        action_selector_name))
          << "Action profile with name " << action_profile_name
          << " already exists.";
    }
  } catch (nlohmann::json::exception& e) {
    return MAKE_ERROR(ERR_INTERNAL) << e.what();
  }

  // Searching all action profile and selector tables from tdi.json
  absl::flat_hash_map<std::string, tdi_id_t> act_prof_tdi_ids;
  absl::flat_hash_map<std::string, tdi_id_t> selector_tdi_ids;
  std::vector<const ::tdi::Table*> tdi_tables;
  RETURN_IF_TDI_ERROR(tdi_info->tablesGet(&tdi_tables));
  for (const auto* table : tdi_tables) {
    auto table_id = table->tableInfoGet()->idGet();
    auto table_name = table->tableInfoGet()->nameGet();
#ifdef DPDK_TARGET
    auto table_type =
        static_cast<tdi_rt_table_type_e>(table->tableInfoGet()->tableTypeGet());
    bool isActionProfileTable = table_type == TDI_RT_TABLE_TYPE_ACTION_PROFILE;
    bool isActionSelectorTable = table_type == TDI_RT_TABLE_TYPE_SELECTOR;
#elif TOFINO_TARGET
    auto table_type = static_cast<tdi_tofino_table_type_e>(
        table->tableInfoGet()->tableTypeGet());
    bool isActionProfileTable =
        table_type == TDI_TOFINO_TABLE_TYPE_ACTION_PROFILE;
    bool isActionSelectorTable = table_type == TDI_TOFINO_TABLE_TYPE_SELECTOR;
#else
#error "Unsupported backend"
#endif
    if (isActionProfileTable) {
      RET_CHECK(
          gtl::InsertIfNotPresent(&act_prof_tdi_ids, table_name, table_id))
          << "Action profile with name " << table_name << " already exists.";
    } else if (isActionSelectorTable) {
      RET_CHECK(
          gtl::InsertIfNotPresent(&selector_tdi_ids, table_name, table_id))
          << "Action selector with name " << table_name << " already exists.";
    }
  }

  // Use the prof_to_sel name mapping to build the ID mapping.
  // Note that the context.json may not include the pipe name as prefix
  // of the table name. So we need to do linear search to find IDs.
  for (const auto& name_pair : prof_to_sel) {
    const auto& prof = name_pair.first;
    const auto& sel = name_pair.second;
    tdi_id_t prof_id = 0;
    tdi_id_t sel_id = 0;
    for (const auto& act_prof_name_id_pair : act_prof_tdi_ids) {
      const auto& act_prof_name = act_prof_name_id_pair.first;
      const auto& act_prof_id = act_prof_name_id_pair.second;
      if (absl::StrContains(act_prof_name, prof)) {
        prof_id = act_prof_id;
        break;
      }
    }
    for (const auto& sel_name_id_pair : selector_tdi_ids) {
      const auto& act_sel_name = sel_name_id_pair.first;
      const auto& act_sel_id = sel_name_id_pair.second;
      if (absl::StrContains(act_sel_name, sel)) {
        sel_id = act_sel_id;
        break;
      }
    }
    RET_CHECK(prof_id != 0) << "Unable to find ID for action profile " << prof;
    RET_CHECK(sel_id != 0) << "Unable to find ID for action selector " << sel;

    act_profile_to_selector_mapping_[prof_id] = sel_id;
    act_selector_to_profile_mapping_[sel_id] = prof_id;
  }
  return ::util::OkStatus();
}

::util::StatusOr<uint32> TdiIdMapper::GetTdiRtId(uint32 p4info_id) const {
  absl::ReaderMutexLock l(&lock_);
  RET_CHECK(gtl::ContainsKey(p4info_to_tdi_id_, p4info_id))
      << "Unable to find TDI id from p4info id: " << p4info_id;
  return gtl::FindOrDie(p4info_to_tdi_id_, p4info_id);
}

::util::StatusOr<uint32> TdiIdMapper::GetP4InfoId(tdi_id_t tdi_id) const {
  absl::ReaderMutexLock l(&lock_);
  RET_CHECK(gtl::ContainsKey(tdi_to_p4info_id_, tdi_id))
      << "Unable to find p4info id from TDI id: " << tdi_id;
  return gtl::FindOrDie(tdi_to_p4info_id_, tdi_id);
}

::util::StatusOr<tdi_id_t> TdiIdMapper::GetActionSelectorTdiRtId(
    tdi_id_t action_profile_id) const {
  absl::ReaderMutexLock l(&lock_);
  RET_CHECK(
      gtl::ContainsKey(act_profile_to_selector_mapping_, action_profile_id))
      << "Unable to find action selector of an action profile: "
      << action_profile_id;
  return gtl::FindOrDie(act_profile_to_selector_mapping_, action_profile_id);
}

::util::StatusOr<tdi_id_t> TdiIdMapper::GetActionProfileTdiRtId(
    tdi_id_t action_selector_id) const {
  absl::ReaderMutexLock l(&lock_);
  RET_CHECK(
      gtl::ContainsKey(act_selector_to_profile_mapping_, action_selector_id))
      << "Unable to find action profile of an action selector: "
      << action_selector_id;
  return gtl::FindOrDie(act_selector_to_profile_mapping_, action_selector_id);
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
