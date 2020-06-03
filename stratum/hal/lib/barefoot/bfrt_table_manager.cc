// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"

#include "stratum/glue/status/status_macros.h"
#include "absl/strings/match.h"
#include "stratum/hal/lib/barefoot/macros.h"

namespace stratum {
namespace hal {
namespace barefoot {

::util::Status BFRuntimeTableManager::PushPipelineInfo(
    const p4::config::v1::P4Info& p4info, const bfrt::BfRtInfo* bfrt_info) {
  absl::WriterMutexLock l(&lock_);
  bfrt_info_ = bfrt_info;
  RETURN_IF_ERROR(BuildP4InfoAndBfrtInfoMapping(p4info, bfrt_info));
  return ::util::OkStatus();
}

::util::Status BFRuntimeTableManager::BuildMapping(uint32_t p4info_id,
                                                   std::string p4info_name,
                                                   const bfrt::BfRtInfo* bfrt_info) {
  const bfrt::BfRtTable* table;
  auto bf_status = bfrt_info->bfrtTableFromIdGet(p4info_id, &table);

  if (bf_status == BF_SUCCESS) {
    // Both p4info and bfrt json uses the same id for a specific
    // table/action selector/profile
    p4info_to_bfrt_id_[p4info_id] = p4info_id;
    bfrt_to_p4info_id_[p4info_id] = p4info_id;
    return ::util::OkStatus();
  }
  // Unable to find table by id, because bfrt uses a different id, we
  // can try to search it by name.
  bf_status = bfrt_info->bfrtTableFromNameGet(p4info_name, &table);
  if (bf_status == BF_SUCCESS) {
    // Table can be found with the given name, but they uses different IDs
    // We need to store mapping so we can map them later.
    bf_rt_id_t bfrt_table_id;
    table->tableIdGet(&bfrt_table_id);
    p4info_to_bfrt_id_[p4info_id] = bfrt_table_id;
    bfrt_to_p4info_id_[bfrt_table_id] = p4info_id;
    return ::util::OkStatus();
  }

  // Special case: bfrt includes pipeline name as prefix(e.g., "pipe."), but
  // p4info doesn't. We need to scan all tables to see if there is a table
  // called "[pipeline name].[P4 info table name]"
  std::vector<const bfrt::BfRtTable *> bfrt_tables;
  BFRT_RETURN_IF_ERROR(bfrt_info->bfrtInfoGetTables(&bfrt_tables));
  for (auto* bfrt_table : bfrt_tables) {
    bf_rt_id_t bfrt_table_id;
    std::string bfrt_table_name;
    bfrt_table->tableIdGet(&bfrt_table_id);
    bfrt_table->tableNameGet(&bfrt_table_name);
    if (absl::StrContains(bfrt_table_name, p4info_name)) {
      p4info_to_bfrt_id_[p4info_id] = bfrt_table_id;
      bfrt_to_p4info_id_[bfrt_table_id] = p4info_id;
      return ::util::OkStatus();
    }
  }
  RETURN_ERROR() << "Unable to find " << p4info_name << " from bfrt info.";
}

// Builds mapping between p4info and bfrt info
// In most case, such as table id, we don't really need to map
// from p4info ID to bfrt ID.
// However for some cases, like externs which does not exists
// in native P4 core headers, the frontend compiler will
// generate different IDs between p4info and bfrt info.
::util::Status BFRuntimeTableManager::BuildP4InfoAndBfrtInfoMapping(
    const p4::config::v1::P4Info& p4info, const bfrt::BfRtInfo* bfrt_info) {

  // Try to find P4 tables from BFRT info
  for (const auto& table : p4info.tables()) {
    RETURN_IF_ERROR(BuildMapping(table.preamble().id(),
                                 table.preamble().name(),
                                 bfrt_info));
  }

  // Action profiles
  for (const auto& action_profile : p4info.action_profiles()) {
    RETURN_IF_ERROR(BuildMapping(action_profile.preamble().id(),
                                 action_profile.preamble().name(),
                                 bfrt_info));
  }
  return ::util::OkStatus();
}

::util::Status BFRuntimeTableManager::BuildTableKey(const ::p4::v1::TableEntry& table_entry,
                                                    bfrt::BfRtTableKey *table_key) {
  for (auto mk : table_entry.match()) {
    bf_rt_id_t field_id = mk.field_id();
    bf_status_t bf_status;
    switch(mk.field_match_type_case()) {
      case ::p4::v1::FieldMatch::kExact: {
        bf_status = table_key->setValue(field_id, mk.exact().value());
        break;
      }
      case ::p4::v1::FieldMatch::kTernary: {
        const size_t size = mk.ternary().value().size();
        const uint8_t* val =
            reinterpret_cast<const uint8_t*>(mk.ternary().value().c_str());
        const uint8_t* mask =
            reinterpret_cast<const uint8_t*>(mk.ternary().mask().c_str());
        bf_status = table_key->setValueandMask(field_id, val, mask, size);
        break;
      }
      case ::p4::v1::FieldMatch::kLpm: {
        const size_t size = mk.lpm().value().size();
        const uint8_t* val =
            reinterpret_cast<const uint8_t*>(mk.lpm().value().c_str());
        const int32_t prefix_len = mk.lpm().prefix_len();
        bf_status = table_key->setValueLpm(field_id, val, prefix_len, size);
        break;
      }
      case ::p4::v1::FieldMatch::kRange: {
        const size_t size = mk.range().low().size();
        const uint8_t* start =
            reinterpret_cast<const uint8_t*>(mk.range().low().c_str());
        const uint8_t* end =
            reinterpret_cast<const uint8_t*>(mk.range().high().c_str());
        bf_status = table_key->setValueRange(field_id, start, end, size);
        break;
      }
      // case ::p4::v1::FieldMatch::kOptional:
      default:
        RETURN_ERROR() << "Invalid or unsupported match key: "
                       << mk.ShortDebugString();
    }
    CHECK_RETURN_IF_FALSE(bf_status == BF_SUCCESS) << bf_err_str(bf_status);
  }
  return ::util::OkStatus();
}

::util::Status BFRuntimeTableManager::BuildTableActionData(
    const ::p4::v1::Action& action,
    const bfrt::BfRtTable *table,
    bfrt::BfRtTableData* table_data) {
  BFRT_RETURN_IF_ERROR(table->dataReset(action.action_id(), table_data));
  for (auto param : action.params()) {
    const size_t size = param.value().size();
    const uint8_t* val =
        reinterpret_cast<const uint8_t*>(param.value().c_str());
    BFRT_RETURN_IF_ERROR(table_data->setValue(param.param_id(), val, size));
  }
  return ::util::OkStatus();
}

::util::Status BFRuntimeTableManager::BuildTableActionProfileMemberData(
  const uint32_t action_profile_member_id,
  const bfrt::BfRtTable *table,
  bfrt::BfRtTableData* table_data) {
  bf_rt_id_t forward_act_mbr_data_field_id;
  BFRT_RETURN_IF_ERROR(table->dataReset(table_data));
  BFRT_RETURN_IF_ERROR(table->dataFieldIdGet("$ACTION_MEMBER_ID",
                                    &forward_act_mbr_data_field_id));
  BFRT_RETURN_IF_ERROR(table_data->setValue(forward_act_mbr_data_field_id,
                       static_cast<uint64_t>(action_profile_member_id)));
  return ::util::OkStatus();
}

::util::Status BFRuntimeTableManager::BuildTableActionProfileGroupData(
  const uint32_t action_profile_group_id,
  const bfrt::BfRtTable *table,
  bfrt::BfRtTableData* table_data) {
  bf_rt_id_t forward_sel_grp_data_field_id;
  BFRT_RETURN_IF_ERROR(table->dataReset(table_data));
  BFRT_RETURN_IF_ERROR(table->dataFieldIdGet("$SELECTOR_GROUP_ID",
                                    &forward_sel_grp_data_field_id));
  BFRT_RETURN_IF_ERROR(table_data->setValue(forward_sel_grp_data_field_id,
                       static_cast<uint64_t>(action_profile_group_id)));
  return ::util::OkStatus();
}

::util::Status BFRuntimeTableManager::BuildTableData(
    const ::p4::v1::TableEntry table_entry,
    const bfrt::BfRtTable *table,
    bfrt::BfRtTableData* table_data) {
  switch(table_entry.action().type_case()) {
    case ::p4::v1::TableAction::kAction:
      return BuildTableActionData(table_entry.action().action(),
                                  table, table_data);
    case ::p4::v1::TableAction::kActionProfileMemberId:
      return BuildTableActionProfileMemberData(
          table_entry.action().action_profile_member_id(), table, table_data);
    case ::p4::v1::TableAction::kActionProfileGroupId:
      return BuildTableActionProfileGroupData(
          table_entry.action().action_profile_group_id(), table, table_data);
    case ::p4::v1::TableAction::kActionProfileActionSet:
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED) << "Unsupported action type: "
                                      << table_entry.action().type_case();
  }
}

::util::Status BFRuntimeTableManager::WriteTableEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::TableEntry& table_entry) {
  const bfrt::BfRtTable* table;
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  CHECK_RETURN_IF_FALSE(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;

  ASSIGN_OR_RETURN(bf_rt_id_t table_id, GetBfRtId(table_entry.table_id()));
  BFRT_RETURN_IF_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  ASSIGN_OR_RETURN(auto bf_dev_tgt, GetDeviceTarget(table_id));

  table->keyReset(table_key.get());
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));

  if (type == ::p4::v1::Update::INSERT || type ==::p4::v1::Update::MODIFY) {
    RETURN_IF_ERROR(BuildTableData(table_entry, table, table_data.get()));
  }

  switch(type) {
    case ::p4::v1::Update::INSERT:
      BFRT_RETURN_IF_ERROR(table->tableEntryAdd(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data));
      break;
    case ::p4::v1::Update::MODIFY:
      BFRT_RETURN_IF_ERROR(table->tableEntryMod(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data));
      break;
    case ::p4::v1::Update::DELETE:
      BFRT_RETURN_IF_ERROR(table->tableEntryDel(*bfrt_session, bf_dev_tgt, *table_key));
      break;
    default:
      RETURN_ERROR() << "Unsupported update type: " << type;
  }
  return ::util::OkStatus();
}

::util::StatusOr<::p4::v1::TableEntry> BFRuntimeTableManager::ReadTableEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::TableEntry& table_entry) {
  const bfrt::BfRtTable* table;
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  ASSIGN_OR_RETURN(bf_rt_id_t table_id, GetBfRtId(table_entry.table_id()));
  BFRT_RETURN_IF_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  BFRT_RETURN_IF_ERROR(table->keyReset(table_key.get()));
  BFRT_RETURN_IF_ERROR(table->dataReset(table_data.get()));
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));
  ASSIGN_OR_RETURN(auto bf_dev_tgt, GetDeviceTarget(table_id));
  BFRT_RETURN_IF_ERROR(table->tableEntryGet(*bfrt_session, bf_dev_tgt,
                                            *table_key,
                                            bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW,
                                            table_data.get()));
  // Build result
  ::p4::v1::TableEntry result;
  result.CopyFrom(table_entry);
  bf_rt_id_t action_id;
  BFRT_RETURN_IF_ERROR(table_data->actionIdGet(&action_id));
  std::vector<bf_rt_id_t> field_id_list;
  BFRT_RETURN_IF_ERROR(table->dataFieldIdListGet(action_id, &field_id_list));
  for (auto field_id : field_id_list) {
    std::string field_name;
    BFRT_RETURN_IF_ERROR(
        table->dataFieldNameGet(field_id, action_id, &field_name));
    if (field_name.compare("$ACTION_MEMBER_ID") == 0) {
      // Action profile member id
      uint64_t act_prof_mem_id;
      BFRT_RETURN_IF_ERROR(table_data->getValue(field_id, &act_prof_mem_id));
      result.mutable_action()->set_action_profile_member_id((uint32_t)act_prof_mem_id);
      continue;
    } else if (field_name.compare("$SELECTOR_GROUP_ID") == 0) {
      // Action profile group id
      uint64_t act_prof_grp_id;
      BFRT_RETURN_IF_ERROR(table_data->getValue(field_id, &act_prof_grp_id));
      result.mutable_action()->set_action_profile_group_id((uint32_t)act_prof_grp_id);
      continue;
    }
    result.mutable_action()->mutable_action()->set_action_id(action_id);
    size_t field_size;
    BFRT_RETURN_IF_ERROR(
      table->dataFieldSizeGet(field_id, action_id, &field_size));
    // "field_size" describes how many "bits" is this field, need to convert
    // to bytes with padding.
    field_size = (field_size / 8) + (field_size % 8 == 0 ? 0 : 1);
    uint8_t field_data[field_size];
    table_data->getValue(field_id, field_size, field_data);
    const void* param_val = reinterpret_cast<const void*>(field_data);

    auto* param = result.mutable_action()->mutable_action()->add_params();
    param->set_param_id(field_id);
    param->set_value(param_val, field_size);
  }
  return result;
}

::util::StatusOr<uint32_t> BFRuntimeTableManager::GetBfRtId(
    uint32_t p4info_id) {
  auto it = p4info_to_bfrt_id_.find(p4info_id);
  CHECK_RETURN_IF_FALSE(it != p4info_to_bfrt_id_.end())
      << "Unable to find bfrt id form p4info id: " << p4info_id;
  return it->second;
}

::util::StatusOr<uint32_t> BFRuntimeTableManager::GetP4InfoId(
    bf_rt_id_t bfrt_id) {
  auto it = bfrt_to_p4info_id_.find(bfrt_id);
  CHECK_RETURN_IF_FALSE(it != bfrt_to_p4info_id_.end())
      << "Unable to find p4info id form bfrt id: " << bfrt_id;
  return it->second;
}

std::unique_ptr<BFRuntimeTableManager> BFRuntimeTableManager::CreateInstance(
    int unit) {
  return absl::WrapUnique(new BFRuntimeTableManager(unit));
}

::util::StatusOr<bf_rt_target_t> BFRuntimeTableManager::GetDeviceTarget(
    bf_rt_id_t bfrt_id) {
  bf_rt_target_t dev_tgt;
  dev_tgt.dev_id = unit_;
  dev_tgt.pipe_id = BF_DEV_PIPE_ALL;
  return dev_tgt;
}

BFRuntimeTableManager::BFRuntimeTableManager(int unit): unit_(unit) {}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
