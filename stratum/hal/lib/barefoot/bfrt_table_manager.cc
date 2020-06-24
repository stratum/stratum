// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"

#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/macros.h"

namespace stratum {
namespace hal {
namespace barefoot {

::util::Status BfrtTableManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config, const bfrt::BfRtInfo* bfrt_info) {
  absl::WriterMutexLock l(&lock_);
  bfrt_info_ = bfrt_info;

  return ::util::OkStatus();
}

// TODO(max): Replace with P4TableMapper class
::util::Status BfrtTableManager::BuildTableKey(
    const ::p4::v1::TableEntry& table_entry, bfrt::BfRtTableKey* table_key,
    const bfrt::BfRtTable* table) {
  for (auto mk : table_entry.match()) {
    bf_rt_id_t field_id = mk.field_id();
    switch (mk.field_match_type_case()) {
      case ::p4::v1::FieldMatch::kExact: {
        const size_t size = mk.exact().value().size();
        const uint8* val =
            reinterpret_cast<const uint8*>(mk.exact().value().c_str());
        RETURN_IF_BFRT_ERROR(table_key->setValue(field_id, val, size))
            << "Could not build table key from " << mk.ShortDebugString();
        break;
      }
      case ::p4::v1::FieldMatch::kTernary: {
        const size_t size = mk.ternary().value().size();
        const uint8* val =
            reinterpret_cast<const uint8*>(mk.ternary().value().c_str());
        const uint8* mask =
            reinterpret_cast<const uint8*>(mk.ternary().mask().c_str());
        RETURN_IF_BFRT_ERROR(
            table_key->setValueandMask(field_id, val, mask, size))
            << "Could not build table key from " << mk.ShortDebugString();
        break;
      }
      case ::p4::v1::FieldMatch::kLpm: {
        const size_t size = mk.lpm().value().size();
        const uint8* val =
            reinterpret_cast<const uint8*>(mk.lpm().value().c_str());
        const int32 prefix_len = mk.lpm().prefix_len();
        RETURN_IF_BFRT_ERROR(
            table_key->setValueLpm(field_id, val, prefix_len, size))
            << "Could not build table key from " << mk.ShortDebugString();
        break;
      }
      case ::p4::v1::FieldMatch::kRange: {
        const size_t size = mk.range().low().size();
        const uint8* start =
            reinterpret_cast<const uint8*>(mk.range().low().c_str());
        const uint8* end =
            reinterpret_cast<const uint8*>(mk.range().high().c_str());
        RETURN_IF_BFRT_ERROR(
            table_key->setValueRange(field_id, start, end, size))
            << "Could not build table key from " << mk.ShortDebugString();
        break;
      }
      // case ::p4::v1::FieldMatch::kOptional:
      default:
        RETURN_ERROR() << "Invalid or unsupported match key: "
                       << mk.ShortDebugString();
    }
  }

  // Priority
  if (table_entry.priority() != 0) {
    bf_rt_id_t priority_field_id;
    RETURN_IF_BFRT_ERROR(
        table->keyFieldIdGet("$MATCH_PRIORITY", &priority_field_id))
        << "table " << table_entry.table_id()
        << " doesn't support match priority.";
    RETURN_IF_BFRT_ERROR(table_key->setValue(
        priority_field_id, static_cast<uint64>(table_entry.priority())));
  }
  return ::util::OkStatus();
}

::util::Status BfrtTableManager::BuildTableActionData(
    const ::p4::v1::Action& action, const bfrt::BfRtTable* table,
    bfrt::BfRtTableData* table_data) {
  RETURN_IF_BFRT_ERROR(table->dataReset(action.action_id(), table_data));
  for (auto param : action.params()) {
    const size_t size = param.value().size();
    const uint8* val = reinterpret_cast<const uint8*>(param.value().c_str());
    RETURN_IF_BFRT_ERROR(table_data->setValue(param.param_id(), val, size));
  }
  return ::util::OkStatus();
}

::util::Status BfrtTableManager::BuildTableActionProfileMemberData(
    const uint32 action_profile_member_id, const bfrt::BfRtTable* table,
    bfrt::BfRtTableData* table_data) {
  bf_rt_id_t forward_act_mbr_data_field_id;
  RETURN_IF_BFRT_ERROR(table->dataFieldIdGet("$ACTION_MEMBER_ID",
                                             &forward_act_mbr_data_field_id));
  RETURN_IF_BFRT_ERROR(
      table_data->setValue(forward_act_mbr_data_field_id,
                           static_cast<uint64>(action_profile_member_id)));
  return ::util::OkStatus();
}

::util::Status BfrtTableManager::BuildTableActionProfileGroupData(
    const uint32 action_profile_group_id, const bfrt::BfRtTable* table,
    bfrt::BfRtTableData* table_data) {
  bf_rt_id_t forward_sel_grp_data_field_id;
  RETURN_IF_BFRT_ERROR(table->dataFieldIdGet("$SELECTOR_GROUP_ID",
                                             &forward_sel_grp_data_field_id));
  RETURN_IF_BFRT_ERROR(
      table_data->setValue(forward_sel_grp_data_field_id,
                           static_cast<uint64>(action_profile_group_id)));
  return ::util::OkStatus();
}

::util::Status BfrtTableManager::BuildTableData(
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

::util::Status BfrtTableManager::WriteTableEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::TableEntry& table_entry) {
  absl::WriterMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;

  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(table_entry.table_id()));
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get(), table));

  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  if (type == ::p4::v1::Update::INSERT || type == ::p4::v1::Update::MODIFY) {
    RETURN_IF_ERROR(BuildTableData(table_entry, table, table_data.get()));
  }

  ASSIGN_OR_RETURN(auto bf_dev_tgt, bfrt_id_mapper_->GetDeviceTarget(table_id));
  switch (type) {
    case ::p4::v1::Update::INSERT:
      RETURN_IF_BFRT_ERROR(table->tableEntryAdd(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data))
          << "Failed to insert table entry " << table_entry.ShortDebugString()
          << ".";
      break;
    case ::p4::v1::Update::MODIFY:
      RETURN_IF_BFRT_ERROR(table->tableEntryMod(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data))
          << "Failed to modify table entry " << table_entry.ShortDebugString()
          << ".";
      break;
    case ::p4::v1::Update::DELETE:
      RETURN_IF_BFRT_ERROR(
          table->tableEntryDel(*bfrt_session, bf_dev_tgt, *table_key))
          << "Failed to delete table entry " << table_entry.ShortDebugString()
          << ".";
      break;
    default:
      RETURN_ERROR(ERR_INTERNAL)
          << "Unsupported update type: " << type << " in table entry "
          << table_entry.ShortDebugString() << ".";
  }
  return ::util::OkStatus();
}

::util::StatusOr<::p4::v1::TableEntry> BfrtTableManager::ReadTableEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::TableEntry& table_entry) {
  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(table_entry.table_id()));
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get(), table));
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  ASSIGN_OR_RETURN(auto bf_dev_tgt, bfrt_id_mapper_->GetDeviceTarget(table_id));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));
  // Build result
  ::p4::v1::TableEntry result = table_entry;
  bf_rt_id_t action_id;
  RETURN_IF_BFRT_ERROR(table_data->actionIdGet(&action_id));
  std::vector<bf_rt_id_t> field_id_list;
  RETURN_IF_BFRT_ERROR(table->dataFieldIdListGet(action_id, &field_id_list));
  for (auto field_id : field_id_list) {
    std::string field_name;
    RETURN_IF_BFRT_ERROR(
        table->dataFieldNameGet(field_id, action_id, &field_name));
    if (field_name.compare("$ACTION_MEMBER_ID") == 0) {
      // Action profile member id
      uint64 act_prof_mem_id;
      RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &act_prof_mem_id));
      result.mutable_action()->set_action_profile_member_id(
          static_cast<uint32>(act_prof_mem_id));
      continue;
    } else if (field_name.compare("$SELECTOR_GROUP_ID") == 0) {
      // Action profile group id
      uint64 act_prof_grp_id;
      RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &act_prof_grp_id));
      result.mutable_action()->set_action_profile_group_id(
          static_cast<uint32>(act_prof_grp_id));
      continue;
    } else if (field_name.compare("$COUNTER_SPEC_BYTES") == 0 ||
               field_name.compare("$COUNTER_SPEC_PKTS") == 0) {
      // Skip counter data
      continue;
    }
    result.mutable_action()->mutable_action()->set_action_id(action_id);
    size_t field_size;
    RETURN_IF_BFRT_ERROR(
        table->dataFieldSizeGet(field_id, action_id, &field_size));
    // "field_size" describes how many "bits" is this field, need to convert
    // to bytes with padding.
    field_size = (field_size / 8) + (field_size % 8 == 0 ? 0 : 1);
    uint8 field_data[field_size];
    table_data->getValue(field_id, field_size, field_data);
    const void* param_val = reinterpret_cast<const void*>(field_data);

    auto* param = result.mutable_action()->mutable_action()->add_params();
    param->set_param_id(field_id);
    param->set_value(param_val, field_size);
  }

  // Priority
  bf_rt_id_t priority_field_id;
  bf_status_t status =
      table->keyFieldIdGet("$MATCH_PRIORITY", &priority_field_id);
  // Table may not support match priority
  if (status == BF_SUCCESS) {
    uint64 table_entry_priority = 0;
    table_key->getValue(priority_field_id, &table_entry_priority);
    result.set_priority(table_entry_priority);
  }

  return result;
}

std::unique_ptr<BfrtTableManager> BfrtTableManager::CreateInstance(
    const BfrtIdMapper* bfrt_id_mapper) {
  return absl::WrapUnique(new BfrtTableManager(bfrt_id_mapper));
}

BfrtTableManager::BfrtTableManager(const BfrtIdMapper* bfrt_id_mapper)
    : bfrt_id_mapper_(ABSL_DIE_IF_NULL(bfrt_id_mapper)) {}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
