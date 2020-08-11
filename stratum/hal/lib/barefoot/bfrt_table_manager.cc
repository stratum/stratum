// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"

#include <set>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/synchronization/notification.h"
#include "bf_rt/bf_rt_table_operations.hpp"
#include "gflags/gflags.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/macros.h"
#include "stratum/hal/lib/barefoot/utils.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

::util::Status BfrtTableManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config, const bfrt::BfRtInfo* bfrt_info) {
  absl::WriterMutexLock l(&lock_);
  bfrt_info_ = bfrt_info;

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::BuildTableKey(
    const ::p4::v1::TableEntry& table_entry, bfrt::BfRtTableKey* table_key,
    const bfrt::BfRtTable* table) {
  for (const auto& mk : table_entry.match()) {
    bf_rt_id_t field_id = mk.field_id();
    switch (mk.field_match_type_case()) {
      case ::p4::v1::FieldMatch::kExact: {
        CHECK_RETURN_IF_FALSE(!IsDontCareMatch(mk.exact()));
        const size_t size = mk.exact().value().size();
        const uint8* val =
            reinterpret_cast<const uint8*>(mk.exact().value().data());
        RETURN_IF_BFRT_ERROR(table_key->setValue(field_id, val, size))
            << "Could not build table key from " << mk.ShortDebugString();
        break;
      }
      case ::p4::v1::FieldMatch::kTernary: {
        CHECK_RETURN_IF_FALSE(!IsDontCareMatch(mk.ternary()));
        CHECK_RETURN_IF_FALSE(table_entry.priority())
            << "Ternary field matches require a priority in table entry "
            << table_entry.ShortDebugString() << ".";
        const size_t size = mk.ternary().value().size();
        const uint8* val =
            reinterpret_cast<const uint8*>(mk.ternary().value().data());
        const uint8* mask =
            reinterpret_cast<const uint8*>(mk.ternary().mask().data());
        RETURN_IF_BFRT_ERROR(
            table_key->setValueandMask(field_id, val, mask, size))
            << "Could not build table key from " << mk.ShortDebugString();
        break;
      }
      case ::p4::v1::FieldMatch::kLpm: {
        CHECK_RETURN_IF_FALSE(!IsDontCareMatch(mk.lpm()));
        const size_t size = mk.lpm().value().size();
        const uint8* val =
            reinterpret_cast<const uint8*>(mk.lpm().value().data());
        const int32 prefix_len = mk.lpm().prefix_len();
        RETURN_IF_BFRT_ERROR(
            table_key->setValueLpm(field_id, val, prefix_len, size))
            << "Could not build table key from " << mk.ShortDebugString();
        break;
      }
      case ::p4::v1::FieldMatch::kRange: {
        size_t range_bitwidth;
        RETURN_IF_BFRT_ERROR(table->keyFieldSizeGet(field_id, &range_bitwidth));
        CHECK_RETURN_IF_FALSE(!IsDontCareMatch(mk.range(), range_bitwidth));
        CHECK_RETURN_IF_FALSE(table_entry.priority())
            << "Range field matches require a priority in table entry "
            << table_entry.ShortDebugString() << ".";
        const size_t size = mk.range().low().size();
        const uint8* start =
            reinterpret_cast<const uint8*>(mk.range().low().data());
        const uint8* end =
            reinterpret_cast<const uint8*>(mk.range().high().data());
        RETURN_IF_BFRT_ERROR(
            table_key->setValueRange(field_id, start, end, size))
            << "Could not build table key from " << mk.ShortDebugString();
        break;
      }
      case ::p4::v1::FieldMatch::kOptional:
        CHECK_RETURN_IF_FALSE(!IsDontCareMatch(mk.optional()));
        CHECK_RETURN_IF_FALSE(table_entry.priority())
            << "Optional field matches require a priority in table entry "
            << table_entry.ShortDebugString() << ".";
        ABSL_FALLTHROUGH_INTENDED;
      default:
        RETURN_ERROR(ERR_INVALID_PARAM)
            << "Invalid or unsupported match key: " << mk.ShortDebugString();
    }
  }

  // Priority
  if (table_entry.priority()) {
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
  for (const auto& param : action.params()) {
    const size_t size = param.value().size();
    const uint8* val = reinterpret_cast<const uint8*>(param.value().data());
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

::util::Status BfrtTableManager::BuildDirectCounterEntryData(
    const ::p4::v1::DirectCounterEntry& entry, const bfrt::BfRtTable* table,
    bfrt::BfRtTableData* table_data) {
  bf_rt_id_t action_id = 0;
  if (table->actionIdApplicable()) {
    RETURN_IF_BFRT_ERROR(table_data->actionIdGet(&action_id));
  }
  std::vector<bf_rt_id_t> ids;
  bf_rt_id_t field_id_bytes;
  bf_status_t has_bytes =
      table->dataFieldIdGet("$COUNTER_SPEC_BYTES", action_id, &field_id_bytes);
  if (has_bytes == BF_SUCCESS) {
    ids.push_back(field_id_bytes);
  }
  bf_rt_id_t field_id_packets;
  bf_status_t has_packets =
      table->dataFieldIdGet("$COUNTER_SPEC_PKTS", action_id, &field_id_packets);
  if (has_packets == BF_SUCCESS) {
    ids.push_back(field_id_packets);
  }
  if (action_id) {
    RETURN_IF_BFRT_ERROR(table->dataReset(ids, action_id, table_data));
  } else {
    RETURN_IF_BFRT_ERROR(table->dataReset(ids, table_data));
  }
  const auto& counter_data = entry.data();
  if (has_bytes == BF_SUCCESS) {
    RETURN_IF_BFRT_ERROR(table_data->setValue(
        field_id_bytes, static_cast<uint64>(counter_data.byte_count())));
  }
  if (has_packets == BF_SUCCESS) {
    RETURN_IF_BFRT_ERROR(table_data->setValue(
        field_id_packets, static_cast<uint64>(counter_data.packet_count())));
  }
  return ::util::OkStatus();
}

::util::Status BfrtTableManager::BuildTableData(
    const ::p4::v1::TableEntry& table_entry, const bfrt::BfRtTable* table,
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

  if (table_entry.has_counter_data()) {
    const auto& counter_data = table_entry.counter_data();
    bf_rt_id_t field_id;
    RETURN_IF_BFRT_ERROR(
        table->dataFieldIdGet("$COUNTER_SPEC_BYTES", &field_id));
    RETURN_IF_BFRT_ERROR(table_data->setValue(
        field_id, static_cast<uint64>(counter_data.byte_count())));
    RETURN_IF_BFRT_ERROR(
        table->dataFieldIdGet("$COUNTER_SPEC_PKTS", &field_id));
    RETURN_IF_BFRT_ERROR(table_data->setValue(
        field_id, static_cast<uint64>(counter_data.packet_count())));
  }
}

::util::Status BfrtTableManager::WriteTableEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::TableEntry& table_entry) {
  CHECK_RETURN_IF_FALSE(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;

  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(table_entry.table_id()));
  const bfrt::BfRtTable* table;
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  }

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get(), table));

  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  if (type == ::p4::v1::Update::INSERT || type == ::p4::v1::Update::MODIFY) {
    RETURN_IF_ERROR(BuildTableData(table_entry, table, table_data.get()));
  }

  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  if (table_entry.is_default_action()) {
    CHECK_RETURN_IF_FALSE(type == ::p4::v1::Update::MODIFY)
        << "The table default entry can only be modified.";
    CHECK_RETURN_IF_FALSE(table_entry.match_size() == 0)
        << "Default action must not contain match fields.";
    CHECK_RETURN_IF_FALSE(table_entry.priority() == 0)
        << "Default action must not contain a priority field.";
    RETURN_IF_BFRT_ERROR(
        table->tableDefaultEntrySet(*bfrt_session, bf_dev_tgt, *table_data))
        << "Failed to modify default table entry "
        << table_entry.ShortDebugString() << ".";
  } else {
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
  }

  return ::util::OkStatus();
}

// TODO(max): the need for the original request might go away when the table
// data is correctly initialized with only the fields we care about.
::util::StatusOr<::p4::v1::TableEntry> BfrtTableManager::BuildP4TableEntry(
    const ::p4::v1::TableEntry& request, const bfrt::BfRtTable* table,
    const bfrt::BfRtTableKey& table_key,
    const bfrt::BfRtTableData& table_data) {
  ::p4::v1::TableEntry result;

  // Table ID
  bf_rt_id_t bfrt_table_id;
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&bfrt_table_id));
  ASSIGN_OR_RETURN(auto p4rt_table_id,
                   bfrt_id_mapper_->GetP4InfoId(bfrt_table_id));
  result.set_table_id(p4rt_table_id);

  // Match key and priority
  std::vector<bf_rt_id_t> key_field_ids;
  RETURN_IF_BFRT_ERROR(table->keyFieldIdListGet(&key_field_ids));
  for (const auto key_field_id : key_field_ids) {
    std::string field_name;
    RETURN_IF_BFRT_ERROR(table->keyFieldNameGet(key_field_id, &field_name));
    // Handle reserved field keys first.
    if (field_name == "$MATCH_PRIORITY") {
      uint64 table_entry_priority = 0;
      RETURN_IF_BFRT_ERROR(
          table_key.getValue(key_field_id, &table_entry_priority));
      result.set_priority(table_entry_priority);
    } else {
      ::p4::v1::FieldMatch match;  // Added to the entry later.
      match.set_field_id(key_field_id);
      bfrt::KeyFieldType field_type = bfrt::KeyFieldType::INVALID;
      RETURN_IF_BFRT_ERROR(table->keyFieldTypeGet(key_field_id, &field_type));
      size_t field_size_bits;
      RETURN_IF_BFRT_ERROR(
          table->keyFieldSizeGet(key_field_id, &field_size_bits));
      int field_size = (field_size_bits + 7) / 8;
      uint8 key_field_value[field_size];
      uint8 key_field_mask[field_size];
      switch (field_type) {
        case bfrt::KeyFieldType::EXACT: {
          RETURN_IF_BFRT_ERROR(
              table_key.getValue(key_field_id, field_size, key_field_value));
          match.mutable_exact()->set_value(key_field_value, field_size);
          if (!IsDontCareMatch(match.exact())) {
            *result.add_match() = match;
          }
          break;
        }
        case bfrt::KeyFieldType::TERNARY: {
          RETURN_IF_BFRT_ERROR(table_key.getValueandMask(
              key_field_id, field_size, key_field_value, key_field_mask));
          match.mutable_ternary()->set_value(key_field_value, field_size);
          match.mutable_ternary()->set_mask(key_field_mask, field_size);
          if (!IsDontCareMatch(match.ternary())) {
            *result.add_match() = match;
          }
          break;
        }
        case bfrt::KeyFieldType::RANGE: {
          RETURN_IF_BFRT_ERROR(table_key.getValueRange(
              key_field_id, field_size, key_field_value, key_field_mask));
          match.mutable_range()->set_low(key_field_value, field_size);
          match.mutable_range()->set_high(key_field_mask, field_size);
          if (!IsDontCareMatch(match.range(), field_size_bits)) {
            *result.add_match() = match;
          }
          break;
        }
        case bfrt::KeyFieldType::LPM: {
          uint16 prefix_length;
          RETURN_IF_BFRT_ERROR(table_key.getValueLpm(
              key_field_id, field_size, key_field_value, &prefix_length));
          match.mutable_lpm()->set_value(key_field_value, field_size);
          match.mutable_lpm()->set_prefix_len(prefix_length);
          if (!IsDontCareMatch(match.lpm())) {
            *result.add_match() = match;
          }
          break;
        }
        default:
          return MAKE_ERROR(ERR_INTERNAL)
                 << "Unknown key field type: " << static_cast<int>(field_type)
                 << ".";
      }
    }
  }

  // Action and action data
  bf_rt_id_t action_id = 0;
  if (table->actionIdApplicable()) {
    RETURN_IF_BFRT_ERROR(table_data.actionIdGet(&action_id));
  }
  std::vector<bf_rt_id_t> field_id_list;
  if (action_id) {
    RETURN_IF_BFRT_ERROR(table->dataFieldIdListGet(action_id, &field_id_list));
  } else {
    RETURN_IF_BFRT_ERROR(table->dataFieldIdListGet(&field_id_list));
  }
  // FIXME: Enforce that the TableAction type is only set once.
  result.mutable_action()->mutable_action()->set_action_id(action_id);
  for (const auto& field_id : field_id_list) {
    std::string field_name;
    if (action_id) {
      RETURN_IF_BFRT_ERROR(
          table->dataFieldNameGet(field_id, action_id, &field_name));
    } else {
      RETURN_IF_BFRT_ERROR(table->dataFieldNameGet(field_id, &field_name));
    }
    if (field_name == "$ACTION_MEMBER_ID") {
      // Action profile member id
      uint64 act_prof_mem_id;
      RETURN_IF_BFRT_ERROR(table_data.getValue(field_id, &act_prof_mem_id));
      result.mutable_action()->set_action_profile_member_id(
          static_cast<uint32>(act_prof_mem_id));
    } else if (field_name == "$SELECTOR_GROUP_ID") {
      // Action profile group id
      uint64 act_prof_grp_id;
      RETURN_IF_BFRT_ERROR(table_data.getValue(field_id, &act_prof_grp_id));
      result.mutable_action()->set_action_profile_group_id(
          static_cast<uint32>(act_prof_grp_id));
    } else if (field_name == "$COUNTER_SPEC_BYTES") {
      if (request.has_counter_data()) {
        uint64 counter_val;
        RETURN_IF_BFRT_ERROR(table_data.getValue(field_id, &counter_val));
        result.mutable_counter_data()->set_byte_count(
            static_cast<int64>(counter_val));
      }
    } else if (field_name == "$COUNTER_SPEC_PKTS") {
      if (request.has_counter_data()) {
        uint64 counter_val;
        RETURN_IF_BFRT_ERROR(table_data.getValue(field_id, &counter_val));
        result.mutable_counter_data()->set_packet_count(
            static_cast<int64>(counter_val));
      }
    } else {
      size_t field_size;
      if (action_id) {
        RETURN_IF_BFRT_ERROR(
            table->dataFieldSizeGet(field_id, action_id, &field_size));
      } else {
        RETURN_IF_BFRT_ERROR(table->dataFieldSizeGet(field_id, &field_size));
      }
      // "field_size" describes how many "bits" is this field, need to convert
      // to bytes with padding.
      field_size = (field_size + 7) / 8;
      uint8 field_data[field_size];
      table_data.getValue(field_id, field_size, field_data);
      const void* param_val = reinterpret_cast<const void*>(field_data);

      auto* param = result.mutable_action()->mutable_action()->add_params();
      param->set_param_id(field_id);
      param->set_value(param_val, field_size);
    }
  }

  return result;
}

::util::Status BfrtTableManager::ReadSingleTableEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::TableEntry& table_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(table_entry.table_id()));
  const bfrt::BfRtTable* table;
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  }
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get(), table));
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();

  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()))
      << "Could not find table entry " << table_entry.ShortDebugString() << ".";

  ASSIGN_OR_RETURN(
      ::p4::v1::TableEntry result,
      BuildP4TableEntry(table_entry, table, *table_key, *table_data));
  ::p4::v1::ReadResponse resp;
  *resp.add_entities()->mutable_table_entry() = result;
  VLOG(1) << "ReadSingleTableEntry resp " << resp.DebugString();
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::ReadDefaultTableEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::TableEntry& table_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  CHECK_RETURN_IF_FALSE(table_entry.table_id())
      << "Missing table id on default action read "
      << table_entry.ShortDebugString() << ".";

  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(table_entry.table_id()));
  const bfrt::BfRtTable* table;
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  }
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  // Empty for now.
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  RETURN_IF_BFRT_ERROR(table->tableDefaultEntryGet(
      *bfrt_session, bf_dev_tgt, bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW,
      table_data.get()));

  // FIXME: BuildP4TableEntry is not suitable for default entries.
  ASSIGN_OR_RETURN(
      ::p4::v1::TableEntry result,
      BuildP4TableEntry(table_entry, table, *table_key, *table_data));
  result.set_is_default_action(true);
  result.clear_match();

  ::p4::v1::ReadResponse resp;
  *resp.add_entities()->mutable_table_entry() = result;
  VLOG(1) << "ReadDefaultTableEntry resp " << resp.DebugString();
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::ReadAllTableEntries(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::TableEntry& table_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  CHECK_RETURN_IF_FALSE(table_entry.match_size() == 0)
      << "Match filters on wildcard reads are not supported.";
  CHECK_RETURN_IF_FALSE(table_entry.priority() == 0)
      << "Priority filters on wildcard reads are not supported.";
  CHECK_RETURN_IF_FALSE(table_entry.has_action() == false)
      << "Action filters on wildcard reads are not supported.";
  CHECK_RETURN_IF_FALSE(table_entry.metadata() == "")
      << "Metadata filters on wildcard reads are not supported.";
  CHECK_RETURN_IF_FALSE(table_entry.is_default_action() == false)
      << "Default action filters on wildcard reads are not supported.";
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(table_entry.table_id()));
  const bfrt::BfRtTable* table;
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  }

  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  RETURN_IF_ERROR(
      GetAllEntries(bfrt_session, bf_dev_tgt, table, &keys, &datums));
  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    ASSIGN_OR_RETURN(auto result, BuildP4TableEntry(table_entry, table,
                                                    *table_key, *table_data));
    *resp.add_entities()->mutable_table_entry() = result;
  }

  VLOG(1) << "ReadAllTableEntries resp " << resp.DebugString();
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::SyncTableCounters(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::TableEntry& table_entry) {
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(table_entry.table_id()));
  const bfrt::BfRtTable* table;
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  }
  absl::Notification sync_notifier;
  std::set<bfrt::TableOperationsType> supported_ops;
  RETURN_IF_BFRT_ERROR(table->tableOperationsSupported(&supported_ops));
  // Controller tries to read counter, but the table doesn't support it.
  CHECK_RETURN_IF_FALSE(
      supported_ops.count(bfrt::TableOperationsType::COUNTER_SYNC))
      << "Counters are not supported by table " << table_id << ".";
  std::unique_ptr<bfrt::BfRtTableOperations> table_op;
  RETURN_IF_BFRT_ERROR(table->operationsAllocate(
      bfrt::TableOperationsType::COUNTER_SYNC, &table_op));
  RETURN_IF_BFRT_ERROR(table_op->counterSyncSet(
      *bfrt_session, bf_dev_tgt,
      [table_id, &sync_notifier](const bf_rt_target_t& dev_tgt, void* cookie) {
        VLOG(1) << "Table counter for table " << table_id << " synced.";
        sync_notifier.Notify();
      },
      nullptr));
  RETURN_IF_BFRT_ERROR(table->tableOperationsExecute(*table_op.get()));

  // Wait until sync done or timeout.
  if (!sync_notifier.WaitForNotificationWithTimeout(kDefaultSyncTimeout)) {
    return MAKE_ERROR(ERR_OPER_TIMEOUT)
           << "Timeout while syncing table counters of table " << table_id
           << ".";
  }

  return ::util::OkStatus();
}

::util::StatusOr<std::vector<uint32>> BfrtTableManager::GetP4TableIds() {
  std::vector<uint32> ids;
  std::vector<const bfrt::BfRtTable*> bfrt_tables;
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtInfoGetTables(&bfrt_tables));
  }
  for (const auto* bfrt_table : bfrt_tables) {
    bf_rt_id_t bfrt_table_id;
    bfrt_table->tableIdGet(&bfrt_table_id);
    bfrt::BfRtTable::TableType table_type;
    RETURN_IF_BFRT_ERROR(bfrt_table->tableTypeGet(&table_type));
    switch (table_type) {
      case bfrt::BfRtTable::TableType::MATCH_DIRECT:
      case bfrt::BfRtTable::TableType::MATCH_INDIRECT:
      case bfrt::BfRtTable::TableType::MATCH_INDIRECT_SELECTOR: {
        ASSIGN_OR_RETURN(auto p4rt_table_id,
                         bfrt_id_mapper_->GetP4InfoId(bfrt_table_id));
        ids.push_back(p4rt_table_id);
        break;
      }
      default:
        continue;
    }
  }
  return ids;
}

::util::Status BfrtTableManager::ReadTableEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::TableEntry& table_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  CHECK_RETURN_IF_FALSE(writer) << "Null writer.";

  // We have four cases to handle:
  // 1. empty table entry: return all tables
  // 2. table id set, no match key: return all table entires of that table
  // 3. table id set, no match key, is_default_action set: return default action
  // 4. table id and match key: return single entry

  if (table_entry.match_size() == 0 && !table_entry.is_default_action()) {
    // 1. or 2.
    std::vector<::p4::v1::TableEntry> wanted_tables;
    if (ProtoEqual(table_entry, ::p4::v1::TableEntry::default_instance())) {
      // TODO: remove workaround by fixing the interfaces?
      ASSIGN_OR_RETURN(auto ids, GetP4TableIds());
      for (const auto& table_id : ids) {
        ::p4::v1::TableEntry te;
        te.set_table_id(table_id);
        wanted_tables.push_back(te);
      }
    } else {
      wanted_tables.push_back(table_entry);
    }
    for (const auto& table_entry : wanted_tables) {
      RETURN_IF_ERROR_WITH_APPEND(
          ReadAllTableEntries(bfrt_session, table_entry, writer))
              .with_logging()
          << "Failed to read all table entries for request "
          << table_entry.ShortDebugString() << ".";
    }
    return ::util::OkStatus();
  } else if (table_entry.match_size() == 0 && table_entry.is_default_action()) {
    // 3.
    return ReadDefaultTableEntry(bfrt_session, table_entry, writer);
  } else {
    // 4.
    if (table_entry.has_counter_data()) {
      RETURN_IF_ERROR(SyncTableCounters(bfrt_session, table_entry));
    }
    return ReadSingleTableEntry(bfrt_session, table_entry, writer);
  }

  CHECK(false) << "This should never happen.";
}

// Modify the counter data of a table entry.
::util::Status BfrtTableManager::WriteDirectCounterEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::DirectCounterEntry& direct_counter_entry) {
  CHECK_RETURN_IF_FALSE(type == ::p4::v1::Update::MODIFY)
      << "Update type of DirectCounterEntry "
      << direct_counter_entry.ShortDebugString() << " must be MODIFY.";

  // Read table entry first.
  const auto& table_entry = direct_counter_entry.table_entry();
  CHECK_RETURN_IF_FALSE(table_entry.action().action().action_id() == 0)
      << "Found action on DirectCounterEntry "
      << direct_counter_entry.ShortDebugString();
  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(table_entry.table_id()));
  const bfrt::BfRtTable* table;
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  }

  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();

  // Table key
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get(), table));

  // Table data
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  // Fetch existing entry with action data.
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()))
      << "Could not find table entry for direct counter "
      << direct_counter_entry.ShortDebugString() << ".";

  // Rewrite the counter data and modify it.
  if (!direct_counter_entry.has_data()) {
    // Nothing to be updated.
    return ::util::OkStatus();
  }

  RETURN_IF_ERROR(BuildDirectCounterEntryData(direct_counter_entry, table,
                                              table_data.get()));

  RETURN_IF_BFRT_ERROR(
      table->tableEntryMod(*bfrt_session, bf_dev_tgt, *table_key, *table_data));

  return ::util::OkStatus();
}

// Read the counter data of a table entry.
::util::StatusOr<::p4::v1::DirectCounterEntry>
BfrtTableManager::ReadDirectCounterEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::DirectCounterEntry& direct_counter_entry) {
  const auto& table_entry = direct_counter_entry.table_entry();
  CHECK_RETURN_IF_FALSE(table_entry.action().action().action_id() == 0)
      << "Found action on DirectCounterEntry "
      << direct_counter_entry.ShortDebugString();
  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(table_entry.table_id()));
  const bfrt::BfRtTable* table;
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  }
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();

  // Sync table counter
  RETURN_IF_ERROR(SyncTableCounters(bfrt_session, table_entry));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get(), table));
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()))
      << "Could not find table entry for direct counter "
      << direct_counter_entry.ShortDebugString() << ".";

  // TODO(max): build response entry from returned data
  ::p4::v1::DirectCounterEntry result = direct_counter_entry;
  bf_rt_id_t action_id = 0;
  if (table->actionIdApplicable()) {
    RETURN_IF_BFRT_ERROR(table_data->actionIdGet(&action_id));
  }
  bf_rt_id_t field_id;
  // Try to read byte counter
  bf_status_t bf_status;
  if (action_id) {
    bf_status =
        table->dataFieldIdGet("$COUNTER_SPEC_BYTES", action_id, &field_id);
  } else {
    bf_status = table->dataFieldIdGet("$COUNTER_SPEC_BYTES", &field_id);
  }
  if (bf_status == BF_SUCCESS) {
    uint64 counter_val;
    RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &counter_val));
    result.mutable_data()->set_byte_count(static_cast<int64>(counter_val));
  }

  // Try to read packet counter
  if (action_id) {
    bf_status =
        table->dataFieldIdGet("$COUNTER_SPEC_PKTS", action_id, &field_id);
  } else {
    bf_status = table->dataFieldIdGet("$COUNTER_SPEC_PKTS", &field_id);
  }
  if (bf_status == BF_SUCCESS) {
    uint64 counter_val;
    RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &counter_val));
    result.mutable_data()->set_packet_count(static_cast<int64>(counter_val));
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
