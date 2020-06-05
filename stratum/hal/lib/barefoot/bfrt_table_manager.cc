// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"

#include "absl/strings/match.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/macros.h"

namespace stratum {
namespace hal {
namespace barefoot {

::util::Status BfRtTableManager::BuildTableKey(
    const ::p4::v1::TableEntry& table_entry, bfrt::BfRtTableKey* table_key) {
  for (auto mk : table_entry.match()) {
    bf_rt_id_t field_id = mk.field_id();
    bf_status_t bf_status;
    switch (mk.field_match_type_case()) {
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

::util::Status BfRtTableManager::BuildTableActionData(
    const ::p4::v1::Action& action, const bfrt::BfRtTable* table,
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

::util::Status BfRtTableManager::BuildTableActionProfileMemberData(
    const uint32_t action_profile_member_id, const bfrt::BfRtTable* table,
    bfrt::BfRtTableData* table_data) {
  bf_rt_id_t forward_act_mbr_data_field_id;
  BFRT_RETURN_IF_ERROR(table->dataReset(table_data));
  BFRT_RETURN_IF_ERROR(table->dataFieldIdGet("$ACTION_MEMBER_ID",
                                             &forward_act_mbr_data_field_id));
  BFRT_RETURN_IF_ERROR(
      table_data->setValue(forward_act_mbr_data_field_id,
                           static_cast<uint64_t>(action_profile_member_id)));
  return ::util::OkStatus();
}

::util::Status BfRtTableManager::BuildTableActionProfileGroupData(
    const uint32_t action_profile_group_id, const bfrt::BfRtTable* table,
    bfrt::BfRtTableData* table_data) {
  bf_rt_id_t forward_sel_grp_data_field_id;
  BFRT_RETURN_IF_ERROR(table->dataReset(table_data));
  BFRT_RETURN_IF_ERROR(table->dataFieldIdGet("$SELECTOR_GROUP_ID",
                                             &forward_sel_grp_data_field_id));
  BFRT_RETURN_IF_ERROR(
      table_data->setValue(forward_sel_grp_data_field_id,
                           static_cast<uint64_t>(action_profile_group_id)));
  return ::util::OkStatus();
}

::util::Status BfRtTableManager::BuildTableData(
    const ::p4::v1::TableEntry table_entry, const bfrt::BfRtTable* table,
    bfrt::BfRtTableData* table_data) {
  switch (table_entry.action().type_case()) {
    case ::p4::v1::TableAction::kAction:
      return BuildTableActionData(table_entry.action().action(), table,
                                  table_data);
    case ::p4::v1::TableAction::kActionProfileMemberId:
      return BuildTableActionProfileMemberData(
          table_entry.action().action_profile_member_id(), table, table_data);
    case ::p4::v1::TableAction::kActionProfileGroupId:
      return BuildTableActionProfileGroupData(
          table_entry.action().action_profile_group_id(), table, table_data);
    case ::p4::v1::TableAction::kActionProfileActionSet:
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED)
          << "Unsupported action type: " << table_entry.action().type_case();
  }
}

::util::Status BfRtTableManager::WriteTableEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::TableEntry& table_entry) {
  const bfrt::BfRtTable* table;
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  CHECK_RETURN_IF_FALSE(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;

  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(table_entry.table_id()));
  BFRT_RETURN_IF_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  ASSIGN_OR_RETURN(auto bf_dev_tgt, bfrt_id_mapper_->GetDeviceTarget(table_id));

  table->keyReset(table_key.get());
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));

  if (type == ::p4::v1::Update::INSERT || type == ::p4::v1::Update::MODIFY) {
    RETURN_IF_ERROR(BuildTableData(table_entry, table, table_data.get()));
  }

  switch (type) {
    case ::p4::v1::Update::INSERT:
      BFRT_RETURN_IF_ERROR(table->tableEntryAdd(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data));
      break;
    case ::p4::v1::Update::MODIFY:
      BFRT_RETURN_IF_ERROR(table->tableEntryMod(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data));
      break;
    case ::p4::v1::Update::DELETE:
      BFRT_RETURN_IF_ERROR(
          table->tableEntryDel(*bfrt_session, bf_dev_tgt, *table_key));
      break;
    default:
      RETURN_ERROR() << "Unsupported update type: " << type;
  }
  return ::util::OkStatus();
}

::util::StatusOr<::p4::v1::TableEntry> BfRtTableManager::ReadTableEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::TableEntry& table_entry) {
  const bfrt::BfRtTable* table;
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(table_entry.table_id()));
  BFRT_RETURN_IF_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  BFRT_RETURN_IF_ERROR(table->keyReset(table_key.get()));
  BFRT_RETURN_IF_ERROR(table->dataReset(table_data.get()));
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));
  ASSIGN_OR_RETURN(auto bf_dev_tgt, bfrt_id_mapper_->GetDeviceTarget(table_id));
  BFRT_RETURN_IF_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));
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
      result.mutable_action()->set_action_profile_member_id(
          (uint32_t)act_prof_mem_id);
      continue;
    } else if (field_name.compare("$SELECTOR_GROUP_ID") == 0) {
      // Action profile group id
      uint64_t act_prof_grp_id;
      BFRT_RETURN_IF_ERROR(table_data->getValue(field_id, &act_prof_grp_id));
      result.mutable_action()->set_action_profile_group_id(
          (uint32_t)act_prof_grp_id);
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

std::unique_ptr<BfRtTableManager> BfRtTableManager::CreateInstance(
    int unit, const BfRtIdMapper* bfrt_id_mapper) {
  return absl::WrapUnique(new BfRtTableManager(unit, bfrt_id_mapper));
}

BfRtTableManager::BfRtTableManager(int unit, const BfRtIdMapper* bfrt_id_mapper)
    : unit_(unit), bfrt_id_mapper_(bfrt_id_mapper) {}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
