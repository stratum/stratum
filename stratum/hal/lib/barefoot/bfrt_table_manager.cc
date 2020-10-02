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

DEFINE_uint32(
    bfrt_table_sync_timeout_ms,
    stratum::hal::barefoot::kDefaultSyncTimeout / absl::Milliseconds(1),
    "The timeout for table sync operation like counters and registers.");
DEFINE_bool(incompatible_enable_register_reset_annotations, false,
            "Enables handling of annotions to reset registers.");

namespace stratum {
namespace hal {
namespace barefoot {

BfrtTableManager::BfrtTableManager(OperationMode mode,
                                   const BfrtIdMapper* bfrt_id_mapper)
    : mode_(mode),
      register_timer_descriptors_(),
      bfrt_info_(nullptr),
      p4_info_manager_(nullptr),
      bfrt_id_mapper_(ABSL_DIE_IF_NULL(bfrt_id_mapper)) {}

namespace {
struct RegisterClearThreadData {
  std::vector<p4::config::v1::Register> registers;
  BfrtTableManager* mgr;

  RegisterClearThreadData(BfrtTableManager* _mgr) : registers(), mgr(_mgr) {}
};
}  // namespace

::util::Status BfrtTableManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config, const bfrt::BfRtInfo* bfrt_info) {
  absl::WriterMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(config.programs_size() == 1)
      << "Only one P4 program is supported.";
  register_timer_descriptors_.clear();
  bfrt_info_ = bfrt_info;
  const auto& program = config.programs(0);
  const auto& p4_info = program.p4info();
  std::unique_ptr<P4InfoManager> p4_info_manager =
      absl::make_unique<P4InfoManager>(p4_info);
  RETURN_IF_ERROR(p4_info_manager->InitializeAndVerify());
  p4_info_manager_ = std::move(p4_info_manager);
  RETURN_IF_ERROR(SetupRegisterReset(p4_info));

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::VerifyForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) const {
  // TODO(unknown): Implement if needed.
  return ::util::OkStatus();
}

::util::Status BfrtTableManager::SetupRegisterReset(
    const ::p4::config::v1::P4Info& p4_info) {
  if (!FLAGS_incompatible_enable_register_reset_annotations) {
    return ::util::OkStatus();
  }
  // Crude check to prevent consecutive pipeline pushes.
  static bool first_time = true;
  if (!first_time) {
    LOG(FATAL) << "Multiple pipeline pushes are not allowed when using "
                  "register reset annotations.";
  }
  first_time = false;
  if (mode_ == OPERATION_MODE_SIM) {
    LOG(WARNING)
        << "Register reset annotations are disabled in simulation mode.";
    return ::util::OkStatus();
  }

  // Validate consistent reset intervals.
  std::vector<uint64> intervals_ms;
  for (const auto& reg : p4_info.registers()) {
    ASSIGN_OR_RETURN(
        P4Annotation annotation,
        p4_info_manager_->GetSwitchStackAnnotations(reg.preamble().name()));
    if (annotation.register_reset_interval_ms()) {
      intervals_ms.push_back(annotation.register_reset_interval_ms());
    }
  }
  if (intervals_ms.empty()) {
    return ::util::OkStatus();
  }
  std::sort(intervals_ms.begin(), intervals_ms.end());
  auto last = std::unique(intervals_ms.begin(), intervals_ms.end());
  intervals_ms.erase(last, intervals_ms.end());
  if (intervals_ms.size() != 1) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Inconsistent register reset intervals are not supported.";
  }

  TimerDaemon::DescriptorPtr handle;
  RETURN_IF_ERROR(TimerDaemon::RequestPeriodicTimer(
      0, intervals_ms[0],
      [this, p4_info]() -> ::util::Status {
        auto t1 = absl::Now();
        auto session = bfrt::BfRtSession::sessionCreate();
        CHECK_RETURN_IF_FALSE(session != nullptr)
            << "Unable to create session.";
        RETURN_IF_BFRT_ERROR(session->beginBatch());
        ::util::Status status = ::util::OkStatus();
        for (const auto& reg : p4_info.registers()) {
          P4Annotation annotation;
          {
            absl::ReaderMutexLock l(&lock_);
            ASSIGN_OR_RETURN(annotation,
                             p4_info_manager_->GetSwitchStackAnnotations(
                                 reg.preamble().name()));
          }
          std::string clear_value =
              Uint64ToByteStream(annotation.register_reset_value());
          ::p4::v1::RegisterEntry register_entry;
          register_entry.set_register_id(reg.preamble().id());
          register_entry.mutable_data()->set_bitstring(clear_value);
          register_entry.clear_index();
          APPEND_STATUS_IF_ERROR(
              status, this->WriteRegisterEntry(
                          session, ::p4::v1::Update::MODIFY, register_entry));
          VLOG(1) << "Cleared register " << reg.preamble().name() << ".";
        }
        // We need to end the batch and destroy the session in every case.
        APPEND_STATUS_IF_BFRT_ERROR(status, session->endBatch(true));
        APPEND_STATUS_IF_BFRT_ERROR(status,
                                    session->sessionCompleteOperations());
        APPEND_STATUS_IF_BFRT_ERROR(status, session->sessionDestroy());
        auto t2 = absl::Now();
        VLOG(1) << "Reset all registers in "
                << (t2 - t1) / absl::Milliseconds(1) << " ms.";

        return status;
      },
      &handle));
  register_timer_descriptors_.push_back(handle);

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::BuildTableKey(
    const ::p4::v1::TableEntry& table_entry, bfrt::BfRtTableKey* table_key,
    const bfrt::BfRtTable* table) {
  bool needs_priority = false;
  std::vector<bf_rt_id_t> match_field_ids;
  RETURN_IF_BFRT_ERROR(table->keyFieldIdListGet(&match_field_ids));

  for (const auto& expected_field_id : match_field_ids) {
    bfrt::KeyFieldType field_type;
    RETURN_IF_BFRT_ERROR(
        table->keyFieldTypeGet(expected_field_id, &field_type));
    needs_priority = needs_priority ||
                     field_type == bfrt::KeyFieldType::TERNARY ||
                     field_type == bfrt::KeyFieldType::RANGE;
    auto it =
        std::find_if(table_entry.match().begin(), table_entry.match().end(),
                     [&expected_field_id](const ::p4::v1::FieldMatch& match) {
                       return match.field_id() == expected_field_id;
                     });
    if (it != table_entry.match().end()) {
      auto mk = *it;
      const auto field_id = mk.field_id();
      switch (it->field_match_type_case()) {
        case ::p4::v1::FieldMatch::kExact: {
          CHECK_RETURN_IF_FALSE(!IsDontCareMatch(mk.exact()));
          RETURN_IF_BFRT_ERROR(table_key->setValue(
              field_id,
              reinterpret_cast<const uint8*>(mk.exact().value().data()),
              mk.exact().value().size()))
              << "Could not build table key from " << mk.ShortDebugString();
          break;
        }
        case ::p4::v1::FieldMatch::kTernary: {
          CHECK_RETURN_IF_FALSE(!IsDontCareMatch(mk.ternary()));
          RETURN_IF_BFRT_ERROR(table_key->setValueandMask(
              field_id,
              reinterpret_cast<const uint8*>(mk.ternary().value().data()),
              reinterpret_cast<const uint8*>(mk.ternary().mask().data()),
              mk.ternary().value().size()))
              << "Could not build table key from " << mk.ShortDebugString();
          break;
        }
        case ::p4::v1::FieldMatch::kLpm: {
          CHECK_RETURN_IF_FALSE(!IsDontCareMatch(mk.lpm()));
          RETURN_IF_BFRT_ERROR(table_key->setValueLpm(
              field_id, reinterpret_cast<const uint8*>(mk.lpm().value().data()),
              mk.lpm().prefix_len(), mk.lpm().value().size()))
              << "Could not build table key from " << mk.ShortDebugString();
          break;
        }
        case ::p4::v1::FieldMatch::kRange: {
          RETURN_IF_BFRT_ERROR(table_key->setValueRange(
              field_id, reinterpret_cast<const uint8*>(mk.range().low().data()),
              reinterpret_cast<const uint8*>(mk.range().high().data()),
              mk.range().low().size()))
              << "Could not build table key from " << mk.ShortDebugString();
          break;
        }
        case ::p4::v1::FieldMatch::kOptional:
          CHECK_RETURN_IF_FALSE(!IsDontCareMatch(mk.optional()));
          ABSL_FALLTHROUGH_INTENDED;
        default:
          RETURN_ERROR(ERR_INVALID_PARAM)
              << "Invalid or unsupported match key: " << mk.ShortDebugString();
      }
    } else {
      switch (field_type) {
        case bfrt::KeyFieldType::EXACT:
        case bfrt::KeyFieldType::TERNARY:
        case bfrt::KeyFieldType::LPM:
          // Nothing to be done. Zero values implement a don't care match.
          break;
        case bfrt::KeyFieldType::RANGE: {
          size_t range_bitwidth;
          RETURN_IF_BFRT_ERROR(
              table->keyFieldSizeGet(expected_field_id, &range_bitwidth));
          size_t field_size = (range_bitwidth + 7) / 8;
          RETURN_IF_BFRT_ERROR(table_key->setValueRange(
              expected_field_id,
              reinterpret_cast<const uint8*>(
                  RangeDefaultLow(range_bitwidth).data()),
              reinterpret_cast<const uint8*>(
                  RangeDefaultHigh(range_bitwidth).data()),
              field_size));
          break;
        }
        default:
          RETURN_ERROR(ERR_INVALID_PARAM)
              << "Invalid field match type " << static_cast<int>(field_type)
              << ".";
      }
    }
  }

  // Priority handling.
  if (!needs_priority && table_entry.priority()) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Non-zero priority for ternary/range/optional match.";
  } else if (needs_priority && table_entry.priority() == 0) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Zero priority for ternary/range/optional match.";
  } else if (needs_priority) {
    bf_rt_id_t priority_field_id;
    RETURN_IF_BFRT_ERROR(
        table->keyFieldIdGet("$MATCH_PRIORITY", &priority_field_id))
        << "table " << table_entry.table_id()
        << " doesn't support match priority.";
    ASSIGN_OR_RETURN(uint64 priority,
                     ConvertPriorityFromP4rtToBfrt(table_entry.priority()));
    RETURN_IF_BFRT_ERROR(table_key->setValue(priority_field_id, priority));
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

  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  if (!table_entry.is_default_action()) {
    std::unique_ptr<bfrt::BfRtTableKey> table_key;
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
    RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get(), table));

    std::unique_ptr<bfrt::BfRtTableData> table_data;
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
    if (type == ::p4::v1::Update::INSERT || type == ::p4::v1::Update::MODIFY) {
      RETURN_IF_ERROR(BuildTableData(table_entry, table, table_data.get()));
    }
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
  } else {
    CHECK_RETURN_IF_FALSE(type == ::p4::v1::Update::MODIFY)
        << "The table default entry can only be modified.";
    CHECK_RETURN_IF_FALSE(table_entry.match_size() == 0)
        << "Default action must not contain match fields.";
    CHECK_RETURN_IF_FALSE(table_entry.priority() == 0)
        << "Default action must not contain a priority field.";

    if (table_entry.has_action()) {
      std::unique_ptr<bfrt::BfRtTableData> table_data;
      RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
      RETURN_IF_ERROR(BuildTableData(table_entry, table, table_data.get()));
      RETURN_IF_BFRT_ERROR(
          table->tableDefaultEntrySet(*bfrt_session, bf_dev_tgt, *table_data))
          << "Failed to modify default table entry "
          << table_entry.ShortDebugString() << ".";
    } else {
      RETURN_IF_BFRT_ERROR(
          table->tableDefaultEntryReset(*bfrt_session, bf_dev_tgt));
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
      ASSIGN_OR_RETURN(int32 p4rt_priority,
                       ConvertPriorityFromBfrtToP4rt(table_entry_priority));
      result.set_priority(p4rt_priority);
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
  auto sync_notifier = std::make_shared<absl::Notification>();
  std::weak_ptr<absl::Notification> weak_ref(sync_notifier);
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
      [table_id, weak_ref](const bf_rt_target_t& dev_tgt, void* cookie) {
        if (auto notifier = weak_ref.lock()) {
          VLOG(1) << "Table counter for table " << table_id << " synced.";
          notifier->Notify();
        } else {
          VLOG(1) << "Notifier expired before table " << table_id
                  << " could be synced.";
        }
      },
      nullptr));
  RETURN_IF_BFRT_ERROR(table->tableOperationsExecute(*table_op.get()));

  // Wait until sync done or timeout.
  if (!sync_notifier->WaitForNotificationWithTimeout(
          absl::Milliseconds(FLAGS_bfrt_table_sync_timeout_ms))) {
    return MAKE_ERROR(ERR_OPER_TIMEOUT)
           << "Timeout while syncing table counters of table " << table_id
           << ".";
  }

  return ::util::OkStatus();
}

// TODO(max): Converge with SyncTableCounters
::util::Status BfrtTableManager::SyncTableRegisters(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t table_id) {
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  const bfrt::BfRtTable* table;
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  }
  auto sync_notifier = std::make_shared<absl::Notification>();
  std::weak_ptr<absl::Notification> weak_ref(sync_notifier);
  std::set<bfrt::TableOperationsType> supported_ops;
  RETURN_IF_BFRT_ERROR(table->tableOperationsSupported(&supported_ops));
  // Controller tries to read register, but the table doesn't support it.
  CHECK_RETURN_IF_FALSE(
      supported_ops.count(bfrt::TableOperationsType::REGISTER_SYNC))
      << "Registers are not supported by table " << table_id << ".";
  std::unique_ptr<bfrt::BfRtTableOperations> table_op;
  RETURN_IF_BFRT_ERROR(table->operationsAllocate(
      bfrt::TableOperationsType::REGISTER_SYNC, &table_op));
  RETURN_IF_BFRT_ERROR(table_op->registerSyncSet(
      *bfrt_session, bf_dev_tgt,
      [table_id, weak_ref](const bf_rt_target_t& dev_tgt, void* cookie) {
        if (auto notifier = weak_ref.lock()) {
          VLOG(1) << "Table registers for table " << table_id << " synced.";
          notifier->Notify();
        } else {
          VLOG(1) << "Notifier expired before table " << table_id
                  << " could be synced.";
        }
      },
      nullptr));
  RETURN_IF_BFRT_ERROR(table->tableOperationsExecute(*table_op.get()));

  // Wait until sync done or timeout.
  if (!sync_notifier->WaitForNotificationWithTimeout(
          absl::Milliseconds(FLAGS_bfrt_table_sync_timeout_ms))) {
    return MAKE_ERROR(ERR_OPER_TIMEOUT)
           << "Timeout while syncing table registers of table " << table_id
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

namespace {
// Helper function to get the field ID of the "f1" register data field.
// TODO(max): Maybe use table name and strip off "pipe." at the beginning?
// std::string table_name;
// RETURN_IF_BFRT_ERROR(table->tableNameGet(&table_name));
// RETURN_IF_BFRT_ERROR(
//     table->dataFieldIdGet(absl::StrCat(table_name, ".", "f1"), &field_id));
::util::StatusOr<bf_rt_id_t> GetRegisterDataFieldId(
    const bfrt::BfRtTable* table) {
  std::vector<bf_rt_id_t> data_field_ids;
  RETURN_IF_BFRT_ERROR(table->dataFieldIdListGet(&data_field_ids));
  for (const auto& field_id : data_field_ids) {
    std::string field_name;
    RETURN_IF_BFRT_ERROR(table->dataFieldNameGet(field_id, &field_name));
    bfrt::DataType data_type;
    RETURN_IF_BFRT_ERROR(table->dataFieldDataTypeGet(field_id, &data_type));
    if (absl::EndsWith(field_name, ".f1")) {
      return field_id;
    }
  }

  RETURN_ERROR(ERR_INTERNAL) << "Could not find register data field id.";
}
}  // namespace

::util::Status BfrtTableManager::ReadRegisterEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::RegisterEntry& register_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(register_entry.register_id()));
  const bfrt::BfRtTable* table;
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
    RETURN_IF_ERROR(p4_info_manager_->VerifyRegisterEntry(register_entry));
  }
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  RETURN_IF_ERROR(SyncTableRegisters(bfrt_session, table_id));

  size_t lowest_id, highest_id;
  if (register_entry.has_index()) {
    lowest_id = register_entry.index().index();
    highest_id = lowest_id + 1;
  } else {
    size_t table_size;
    RETURN_IF_BFRT_ERROR(table->tableSizeGet(&table_size));
    lowest_id = 0;
    highest_id = table_size;
  }
  ::p4::v1::ReadResponse resp;
  for (size_t i = lowest_id; i < highest_id; ++i) {
    // Allocate table entry
    std::unique_ptr<bfrt::BfRtTableKey> table_key;
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));

    // Table key: $REGISTER_INDEX
    bf_rt_id_t field_id;
    RETURN_IF_BFRT_ERROR(table->keyFieldIdGet(kRegisterIndex, &field_id));
    RETURN_IF_BFRT_ERROR(table_key->setValue(field_id, static_cast<uint64>(i)));

    // Read the register data.
    std::unique_ptr<bfrt::BfRtTableData> table_data;
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *bfrt_session, bf_dev_tgt, *table_key,
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()))
        << "Could not find table entry for register "
        << register_entry.ShortDebugString() << ".";

    ::p4::v1::RegisterEntry result;
    result.set_register_id(register_entry.register_id());
    // Register key: $REGISTER_INDEX
    uint64 register_index;
    RETURN_IF_BFRT_ERROR(table->keyFieldIdGet(kRegisterIndex, &field_id));
    RETURN_IF_BFRT_ERROR(table_key->getValue(field_id, &register_index));
    result.mutable_index()->set_index(register_index);

    // Register data: <register_name>.f1
    ASSIGN_OR_RETURN(field_id, GetRegisterDataFieldId(table));
    bfrt::DataType data_type;
    RETURN_IF_BFRT_ERROR(table->dataFieldDataTypeGet(field_id, &data_type));
    switch (data_type) {
      case bfrt::DataType::BYTE_STREAM: {
        // Even though the data type says byte stream, we can only fetch the
        // data in an uint vector with one entry per pipe.
        std::vector<uint64> register_data;
        RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &register_data));
        result.mutable_data()->set_bitstring(
            Uint64ToByteStream(register_data[0]));
        // TODO(max): Switch to tuple form, once compiler support landed.
        // ::p4::v1::P4StructLike register_tuple;
        // for (const auto& data : register_data) {
        //   LOG(INFO) << data;
        //   register_tuple.add_members()->set_bitstring(Uint64ToByteStream(data));
        // }
        // *result.mutable_data()->mutable_tuple() = register_tuple;
        break;
      }
      default:
        RETURN_ERROR(ERR_INVALID_PARAM)
            << "Unsupported register data type " << static_cast<int>(data_type)
            << " for RegisterEntry " << register_entry.ShortDebugString();
    }

    {
      absl::ReaderMutexLock l(&lock_);
      RETURN_IF_ERROR(p4_info_manager_->VerifyRegisterEntry(result));
    }
    *resp.add_entities()->mutable_register_entry() = result;
  }
  VLOG(1) << "ReadRegisterEntry resp " << resp.DebugString();
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::WriteRegisterEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::RegisterEntry& register_entry) {
  CHECK_RETURN_IF_FALSE(type == ::p4::v1::Update::MODIFY)
      << "Update type of RegisterEntry " << register_entry.ShortDebugString()
      << " must be MODIFY.";
  CHECK_RETURN_IF_FALSE(register_entry.has_data())
      << "RegisterEntry " << register_entry.ShortDebugString()
      << " must have data.";
  CHECK_RETURN_IF_FALSE(register_entry.data().data_case() ==
                        ::p4::v1::P4Data::kBitstring)
      << "Only bitstring registers data types are supported.";

  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(register_entry.register_id()));
  const bfrt::BfRtTable* table;
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
    RETURN_IF_ERROR(p4_info_manager_->VerifyRegisterEntry(register_entry));
  }
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();

  // Table key: $REGISTER_INDEX
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  bf_rt_id_t register_index_field_id;
  RETURN_IF_BFRT_ERROR(
      table->keyFieldIdGet(kRegisterIndex, &register_index_field_id));

  // Table data: <register_name>.f1
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  bf_rt_id_t field_id;
  ASSIGN_OR_RETURN(field_id, GetRegisterDataFieldId(table));
  size_t data_field_size;
  RETURN_IF_BFRT_ERROR(table->dataFieldSizeGet(field_id, &data_field_size));
  // The SDE expects any array with the full width.
  std::string value((data_field_size + 7) / 8, '\x00');
  value.replace(value.size() - register_entry.data().bitstring().size(),
                register_entry.data().bitstring().size(),
                register_entry.data().bitstring());
  RETURN_IF_BFRT_ERROR(table_data->setValue(
      field_id, reinterpret_cast<const uint8*>(value.data()), value.size()));

  if (register_entry.has_index()) {
    // Single index target.
    RETURN_IF_BFRT_ERROR(table_key->setValue(
        register_index_field_id,
        static_cast<uint64>(register_entry.index().index())));
    RETURN_IF_BFRT_ERROR(table->tableEntryMod(*bfrt_session, bf_dev_tgt,
                                              *table_key, *table_data));
  } else {
    // Wildcard write to all indices.
    size_t table_size;
    RETURN_IF_BFRT_ERROR(table->tableSizeGet(&table_size));
    for (size_t i = 0; i < table_size; ++i) {
      RETURN_IF_BFRT_ERROR(
          table_key->setValue(register_index_field_id, static_cast<uint64>(i)));
      RETURN_IF_BFRT_ERROR(table->tableEntryMod(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data));
    }
  }

  return ::util::OkStatus();
}

std::unique_ptr<BfrtTableManager> BfrtTableManager::CreateInstance(
    OperationMode mode, const BfrtIdMapper* bfrt_id_mapper) {
  return absl::WrapUnique(new BfrtTableManager(mode, bfrt_id_mapper));
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
