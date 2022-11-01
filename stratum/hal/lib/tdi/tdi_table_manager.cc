// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/tdi/tdi_table_manager.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/synchronization/notification.h"
#include "gflags/gflags.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/hal/lib/tdi/tdi_constants.h"
#include "stratum/hal/lib/tdi/utils.h"
#include "stratum/lib/utils.h"

DEFINE_uint32(
    tdi_table_sync_timeout_ms,
    stratum::hal::tdi::kDefaultSyncTimeout / absl::Milliseconds(1),
    "The timeout for table sync operation like counters and registers.");
DEFINE_bool(incompatible_enable_register_reset_annotations, false,
            "Enables handling of annotions to reset registers.");

namespace stratum {
namespace hal {
namespace tdi {

TdiTableManager::TdiTableManager(OperationMode mode,
                                 TdiSdeInterface* tdi_sde_interface, int device)
    : mode_(mode),
      register_timer_descriptors_(),
      tdi_sde_interface_(ABSL_DIE_IF_NULL(tdi_sde_interface)),
      p4_info_manager_(nullptr),
      device_(device) {}

std::unique_ptr<TdiTableManager> TdiTableManager::CreateInstance(
    OperationMode mode, TdiSdeInterface* tdi_sde_interface, int device) {
  return absl::WrapUnique(new TdiTableManager(mode, tdi_sde_interface, device));
}

::util::Status TdiTableManager::PushForwardingPipelineConfig(
    const TdiDeviceConfig& config) {
  absl::WriterMutexLock l(&lock_);
  RET_CHECK(config.programs_size() == 1) << "Only one P4 program is supported.";
  register_timer_descriptors_.clear();
  const auto& program = config.programs(0);
  const auto& p4_info = program.p4info();
  std::unique_ptr<P4InfoManager> p4_info_manager =
      absl::make_unique<P4InfoManager>(p4_info);
  RETURN_IF_ERROR(p4_info_manager->InitializeAndVerify());
  p4_info_manager_ = std::move(p4_info_manager);

  return ::util::OkStatus();
}

::util::Status TdiTableManager::VerifyForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) const {
  // TODO(unknown): Implement if needed.
  return ::util::OkStatus();
}

::util::Status TdiTableManager::BuildTableKey(
    const ::p4::v1::TableEntry& table_entry,
    TdiSdeInterface::TableKeyInterface* table_key) {
  RET_CHECK(table_key);
  bool needs_priority = false;
  ASSIGN_OR_RETURN(auto table,
                   p4_info_manager_->FindTableByID(table_entry.table_id()));

  for (const auto& expected_match_field : table.match_fields()) {
    needs_priority = needs_priority ||
                     expected_match_field.match_type() ==
                         ::p4::config::v1::MatchField::TERNARY ||
                     expected_match_field.match_type() ==
                         ::p4::config::v1::MatchField::RANGE;
    auto expected_field_id = expected_match_field.id();
    auto it =
        std::find_if(table_entry.match().begin(), table_entry.match().end(),
                     [expected_field_id](const ::p4::v1::FieldMatch& match) {
                       return match.field_id() == expected_field_id;
                     });
    if (it != table_entry.match().end()) {
      auto mk = *it;
      switch (mk.field_match_type_case()) {
        case ::p4::v1::FieldMatch::kExact: {
          RET_CHECK(expected_match_field.match_type() ==
                    ::p4::config::v1::MatchField::EXACT)
              << "Found match field of type EXACT does not fit match field "
              << expected_match_field.ShortDebugString() << ".";
          RET_CHECK(!IsDontCareMatch(mk.exact()));
          RETURN_IF_ERROR(
              table_key->SetExact(mk.field_id(), mk.exact().value()));
          break;
        }
        case ::p4::v1::FieldMatch::kTernary: {
          RET_CHECK(expected_match_field.match_type() ==
                    ::p4::config::v1::MatchField::TERNARY)
              << "Found match field of type TERNARY does not fit match field "
              << expected_match_field.ShortDebugString() << ".";
          RET_CHECK(!IsDontCareMatch(mk.ternary()));
          RETURN_IF_ERROR(table_key->SetTernary(
              mk.field_id(), mk.ternary().value(), mk.ternary().mask()));
          break;
        }
        case ::p4::v1::FieldMatch::kLpm: {
          RET_CHECK(expected_match_field.match_type() ==
                    ::p4::config::v1::MatchField::LPM)
              << "Found match field of type LPM does not fit match field "
              << expected_match_field.ShortDebugString() << ".";
          RET_CHECK(!IsDontCareMatch(mk.lpm()));
          RETURN_IF_ERROR(table_key->SetLpm(mk.field_id(), mk.lpm().value(),
                                            mk.lpm().prefix_len()));
          break;
        }
        case ::p4::v1::FieldMatch::kRange: {
          RET_CHECK(expected_match_field.match_type() ==
                    ::p4::config::v1::MatchField::RANGE)
              << "Found match field of type Range does not fit match field "
              << expected_match_field.ShortDebugString() << ".";
          // TODO(max): Do we need to check this for range matches?
          // RET_CHECK(!IsDontCareMatch(match.range(), ));
          RETURN_IF_ERROR(table_key->SetRange(mk.field_id(), mk.range().low(),
                                              mk.range().high()));
          break;
        }
        case ::p4::v1::FieldMatch::kOptional:
          RET_CHECK(!IsDontCareMatch(mk.optional()));
          ABSL_FALLTHROUGH_INTENDED;
        default:
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Invalid or unsupported match key: "
                 << mk.ShortDebugString();
      }
    } else {
      switch (expected_match_field.match_type()) {
        case ::p4::config::v1::MatchField::EXACT:
        case ::p4::config::v1::MatchField::TERNARY:
        case ::p4::config::v1::MatchField::LPM:
          // Nothing to be done. Zero values implement a don't care match.
          break;
        case ::p4::config::v1::MatchField::RANGE: {
          RETURN_IF_ERROR(table_key->SetRange(
              expected_field_id,
              RangeDefaultLow(expected_match_field.bitwidth()),
              RangeDefaultHigh(expected_match_field.bitwidth())));
          break;
        }
        default:
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Invalid field match type "
                 << ::p4::config::v1::MatchField_MatchType_Name(
                        expected_match_field.match_type())
                 << ".";
      }
    }
  }

  // Priority handling.
  if (!needs_priority && table_entry.priority()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Non-zero priority for exact/LPM match.";
  } else if (needs_priority && table_entry.priority() == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Zero priority for ternary/range/optional match.";
  } else if (needs_priority) {
    ASSIGN_OR_RETURN(uint64 priority,
                     ConvertPriorityFromP4rtToTdi(table_entry.priority()));
    RETURN_IF_ERROR(table_key->SetPriority(priority));
  }

  return ::util::OkStatus();
}

::util::Status TdiTableManager::BuildTableActionData(
    const ::p4::v1::Action& action,
    TdiSdeInterface::TableDataInterface* table_data) {
  RETURN_IF_ERROR(table_data->Reset(action.action_id()));
  for (const auto& param : action.params()) {
    RETURN_IF_ERROR(table_data->SetParam(param.param_id(), param.value()));
  }
  return ::util::OkStatus();
}

::util::Status TdiTableManager::BuildTableData(
    const ::p4::v1::TableEntry& table_entry,
    TdiSdeInterface::TableDataInterface* table_data) {
  switch (table_entry.action().type_case()) {
    case ::p4::v1::TableAction::kAction:
      RETURN_IF_ERROR(
          BuildTableActionData(table_entry.action().action(), table_data));
      break;
    case ::p4::v1::TableAction::kActionProfileMemberId:
      RETURN_IF_ERROR(table_data->SetActionMemberId(
          table_entry.action().action_profile_member_id()));
      break;
    case ::p4::v1::TableAction::kActionProfileGroupId:
      RETURN_IF_ERROR(table_data->SetSelectorGroupId(
          table_entry.action().action_profile_group_id()));
      break;
    case ::p4::v1::TableAction::kActionProfileActionSet:
    default:
      return MAKE_ERROR(ERR_UNIMPLEMENTED)
             << "Unsupported action type: " << table_entry.action().type_case();
  }

  if (table_entry.has_counter_data()) {
    RETURN_IF_ERROR(
        table_data->SetCounterData(table_entry.counter_data().byte_count(),
                                   table_entry.counter_data().packet_count()));
  }

  return ::util::OkStatus();
}

::util::Status TdiTableManager::WriteTableEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::TableEntry& table_entry) {
  RET_CHECK(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;

  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(auto table,
                   p4_info_manager_->FindTableByID(table_entry.table_id()));
  ASSIGN_OR_RETURN(uint32 table_id,
                   tdi_sde_interface_->GetTdiRtId(table_entry.table_id()));

  if (!table_entry.is_default_action()) {
    if (table.is_const_table()) {
      return MAKE_ERROR(ERR_PERMISSION_DENIED)
             << "Can't write to table " << table.preamble().name()
             << " because it has const entries.";
    }
    ASSIGN_OR_RETURN(auto table_key,
                     tdi_sde_interface_->CreateTableKey(table_id));
    RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));

    ASSIGN_OR_RETURN(auto table_data,
                     tdi_sde_interface_->CreateTableData(
                         table_id, table_entry.action().action().action_id()));
    if (type == ::p4::v1::Update::INSERT || type == ::p4::v1::Update::MODIFY) {
      RETURN_IF_ERROR(BuildTableData(table_entry, table_data.get()));
    }

    switch (type) {
      case ::p4::v1::Update::INSERT:
        RETURN_IF_ERROR(tdi_sde_interface_->InsertTableEntry(
            device_, session, table_id, table_key.get(), table_data.get()));
        break;
      case ::p4::v1::Update::MODIFY:
        RETURN_IF_ERROR(tdi_sde_interface_->ModifyTableEntry(
            device_, session, table_id, table_key.get(), table_data.get()));
        break;
      case ::p4::v1::Update::DELETE:
        RETURN_IF_ERROR(tdi_sde_interface_->DeleteTableEntry(
            device_, session, table_id, table_key.get()));
        break;
      default:
        return MAKE_ERROR(ERR_INTERNAL)
               << "Unsupported update type: " << type << " in table entry "
               << table_entry.ShortDebugString() << ".";
    }
  } else {
    RET_CHECK(type == ::p4::v1::Update::MODIFY)
        << "The table default entry can only be modified.";
    RET_CHECK(table_entry.match_size() == 0)
        << "Default action must not contain match fields.";
    RET_CHECK(table_entry.priority() == 0)
        << "Default action must not contain a priority field.";

    if (table_entry.has_action()) {
      ASSIGN_OR_RETURN(
          auto table_data,
          tdi_sde_interface_->CreateTableData(
              table_id, table_entry.action().action().action_id()));
      RETURN_IF_ERROR(BuildTableData(table_entry, table_data.get()));
      RETURN_IF_ERROR(tdi_sde_interface_->SetDefaultTableEntry(
          device_, session, table_id, table_data.get()));
    } else {
      RETURN_IF_ERROR(tdi_sde_interface_->ResetDefaultTableEntry(
          device_, session, table_id));
    }
  }

  return ::util::OkStatus();
}

// TODO(max): the need for the original request might go away when the table
// data is correctly initialized with only the fields we care about.
::util::StatusOr<::p4::v1::TableEntry> TdiTableManager::BuildP4TableEntry(
    const ::p4::v1::TableEntry& request,
    const TdiSdeInterface::TableKeyInterface* table_key,
    const TdiSdeInterface::TableDataInterface* table_data) {
  ::p4::v1::TableEntry result;

  ASSIGN_OR_RETURN(auto table,
                   p4_info_manager_->FindTableByID(request.table_id()));
  result.set_table_id(request.table_id());

  bool has_priority_field = false;
  // Match keys
  for (const auto& expected_match_field : table.match_fields()) {
    ::p4::v1::FieldMatch match;  // Added to the entry later.
    match.set_field_id(expected_match_field.id());
    switch (expected_match_field.match_type()) {
      case ::p4::config::v1::MatchField::EXACT: {
        RETURN_IF_ERROR(table_key->GetExact(
            expected_match_field.id(), match.mutable_exact()->mutable_value()));
        if (!IsDontCareMatch(match.exact())) {
          *result.add_match() = match;
        }
        break;
      }
      case ::p4::config::v1::MatchField::TERNARY: {
        has_priority_field = true;
        std::string value, mask;
        RETURN_IF_ERROR(
            table_key->GetTernary(expected_match_field.id(), &value, &mask));
        match.mutable_ternary()->set_value(value);
        match.mutable_ternary()->set_mask(mask);
        if (!IsDontCareMatch(match.ternary())) {
          *result.add_match() = match;
        }
        break;
      }
      case ::p4::config::v1::MatchField::LPM: {
        std::string prefix;
        uint16 prefix_length;
        RETURN_IF_ERROR(table_key->GetLpm(expected_match_field.id(), &prefix,
                                          &prefix_length));
        match.mutable_lpm()->set_value(prefix);
        match.mutable_lpm()->set_prefix_len(prefix_length);
        if (!IsDontCareMatch(match.lpm())) {
          *result.add_match() = match;
        }
        break;
      }
      case ::p4::config::v1::MatchField::RANGE: {
        has_priority_field = true;
        std::string low, high;
        RETURN_IF_ERROR(
            table_key->GetRange(expected_match_field.id(), &low, &high));
        match.mutable_range()->set_low(low);
        match.mutable_range()->set_high(high);
        if (!IsDontCareMatch(match.range(), expected_match_field.bitwidth())) {
          *result.add_match() = match;
        }
        break;
      }
      default:
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Invalid field match type "
               << ::p4::config::v1::MatchField_MatchType_Name(
                      expected_match_field.match_type())
               << ".";
    }
  }

  // Default actions do not have a priority, even when the table usually
  // requires one. The SDE would return 0 (highest) which we must not translate.
  if (request.is_default_action()) {
    has_priority_field = false;
  }

  // Priority.
  if (has_priority_field) {
    uint32 bf_priority;
    RETURN_IF_ERROR(table_key->GetPriority(&bf_priority));
    ASSIGN_OR_RETURN(uint64 p4rt_priority,
                     ConvertPriorityFromTdiToP4rt(bf_priority));
    result.set_priority(p4rt_priority);
  }

  // Action and action data
  int action_id;
  RETURN_IF_ERROR(table_data->GetActionId(&action_id));
  // TODO(max): perform check if action id is valid for this table.
  if (action_id) {
    ASSIGN_OR_RETURN(auto action, p4_info_manager_->FindActionByID(action_id));
    result.mutable_action()->mutable_action()->set_action_id(action_id);
    for (const auto& expected_param : action.params()) {
      std::string value;
      RETURN_IF_ERROR(table_data->GetParam(expected_param.id(), &value));
      auto* param = result.mutable_action()->mutable_action()->add_params();
      param->set_param_id(expected_param.id());
      param->set_value(value);
    }
  }

  // Action profile member id
  uint64 action_member_id;
  if (table_data->GetActionMemberId(&action_member_id).ok()) {
    result.mutable_action()->set_action_profile_member_id(action_member_id);
  }

  // Action profile group id
  uint64 selector_group_id;
  if (table_data->GetSelectorGroupId(&selector_group_id).ok()) {
    result.mutable_action()->set_action_profile_group_id(selector_group_id);
  }

  // Counter data, if applicable.
  uint64 bytes, packets;
  if (request.has_counter_data() &&
      table_data->GetCounterData(&bytes, &packets).ok()) {
    result.mutable_counter_data()->set_byte_count(bytes);
    result.mutable_counter_data()->set_packet_count(packets);
  }

  return result;
}

::util::Status TdiTableManager::ReadSingleTableEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::TableEntry& table_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  ASSIGN_OR_RETURN(uint32 table_id,
                   tdi_sde_interface_->GetTdiRtId(table_entry.table_id()));
  ASSIGN_OR_RETURN(auto table_key,
                   tdi_sde_interface_->CreateTableKey(table_id));
  ASSIGN_OR_RETURN(auto table_data,
                   tdi_sde_interface_->CreateTableData(
                       table_id, table_entry.action().action().action_id()));
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));
  RETURN_IF_ERROR(tdi_sde_interface_->GetTableEntry(
      device_, session, table_id, table_key.get(), table_data.get()));
  ASSIGN_OR_RETURN(
      ::p4::v1::TableEntry result,
      BuildP4TableEntry(table_entry, table_key.get(), table_data.get()));
  ::p4::v1::ReadResponse resp;
  *resp.add_entities()->mutable_table_entry() = result;
  VLOG(1) << "ReadSingleTableEntry resp " << resp.DebugString();
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

::util::Status TdiTableManager::ReadDefaultTableEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::TableEntry& table_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  RET_CHECK(table_entry.table_id())
      << "Missing table id on default action read "
      << table_entry.ShortDebugString() << ".";

  ASSIGN_OR_RETURN(uint32 table_id,
                   tdi_sde_interface_->GetTdiRtId(table_entry.table_id()));
  ASSIGN_OR_RETURN(auto table_key,
                   tdi_sde_interface_->CreateTableKey(table_id));
  ASSIGN_OR_RETURN(auto table_data,
                   tdi_sde_interface_->CreateTableData(
                       table_id, table_entry.action().action().action_id()));
  RETURN_IF_ERROR(tdi_sde_interface_->GetDefaultTableEntry(
      device_, session, table_id, table_data.get()));
  // FIXME: BuildP4TableEntry is not suitable for default entries.
  ASSIGN_OR_RETURN(
      ::p4::v1::TableEntry result,
      BuildP4TableEntry(table_entry, table_key.get(), table_data.get()));
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

::util::Status TdiTableManager::ReadAllTableEntries(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::TableEntry& table_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  RET_CHECK(table_entry.match_size() == 0)
      << "Match filters on wildcard reads are not supported.";
  RET_CHECK(table_entry.priority() == 0)
      << "Priority filters on wildcard reads are not supported.";
  RET_CHECK(table_entry.has_action() == false)
      << "Action filters on wildcard reads are not supported.";
  RET_CHECK(table_entry.metadata() == "")
      << "Metadata filters on wildcard reads are not supported.";
  RET_CHECK(table_entry.is_default_action() == false)
      << "Default action filters on wildcard reads are not supported.";

  ASSIGN_OR_RETURN(uint32 table_id,
                   tdi_sde_interface_->GetTdiRtId(table_entry.table_id()));
  std::vector<std::unique_ptr<TdiSdeInterface::TableKeyInterface>> keys;
  std::vector<std::unique_ptr<TdiSdeInterface::TableDataInterface>> datas;
  RETURN_IF_ERROR(tdi_sde_interface_->GetAllTableEntries(
      device_, session, table_id, &keys, &datas));
  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<TdiSdeInterface::TableKeyInterface>& table_key =
        keys[i];
    const std::unique_ptr<TdiSdeInterface::TableDataInterface>& table_data =
        datas[i];
    ASSIGN_OR_RETURN(
        auto result,
        BuildP4TableEntry(table_entry, table_key.get(), table_data.get()));
    *resp.add_entities()->mutable_table_entry() = result;
  }

  VLOG(1) << "ReadAllTableEntries resp " << resp.DebugString();
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

::util::Status TdiTableManager::ReadTableEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::TableEntry& table_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  RET_CHECK(writer) << "Null writer.";
  absl::ReaderMutexLock l(&lock_);

  // We have four cases to handle:
  // 1. table id not set: return all table entries from all tables
  // 2. table id set, no match key: return all table entries of that table
  // 3. table id set, no match key, is_default_action set: return default action
  // 4. table id and match key: return single entry

  if (table_entry.match_size() == 0 && !table_entry.is_default_action()) {
    std::vector<::p4::v1::TableEntry> wanted_tables;
    if (table_entry.table_id() == 0) {
      // 1.
      const ::p4::config::v1::P4Info& p4_info = p4_info_manager_->p4_info();
      for (const auto& table : p4_info.tables()) {
        ::p4::v1::TableEntry te;
        te.set_table_id(table.preamble().id());
        if (table_entry.has_counter_data()) {
          te.mutable_counter_data();
        }
        wanted_tables.push_back(te);
      }
    } else {
      // 2.
      wanted_tables.push_back(table_entry);
    }
    // TODO(max): can wildcard reads request counter_data?
    if (table_entry.has_counter_data()) {
      for (const auto& wanted_table_entry : wanted_tables) {
        RETURN_IF_ERROR(tdi_sde_interface_->SynchronizeCounters(
            device_, session, wanted_table_entry.table_id(),
            absl::Milliseconds(FLAGS_tdi_table_sync_timeout_ms)));
      }
    }
    for (const auto& wanted_table_entry : wanted_tables) {
      RETURN_IF_ERROR_WITH_APPEND(
          ReadAllTableEntries(session, wanted_table_entry, writer))
              .with_logging()
          << "Failed to read all table entries for request "
          << table_entry.ShortDebugString() << ".";
    }
    return ::util::OkStatus();
  } else if (table_entry.match_size() == 0 && table_entry.is_default_action()) {
    // 3.
    return ReadDefaultTableEntry(session, table_entry, writer);
  } else {
    // 4.
    if (table_entry.has_counter_data()) {
      RETURN_IF_ERROR(tdi_sde_interface_->SynchronizeCounters(
          device_, session, table_entry.table_id(),
          absl::Milliseconds(FLAGS_tdi_table_sync_timeout_ms)));
    }
    return ReadSingleTableEntry(session, table_entry, writer);
  }

  CHECK(false) << "This should never happen.";
}

// Modify the counter data of a table entry.
::util::Status TdiTableManager::WriteDirectCounterEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::DirectCounterEntry& direct_counter_entry) {
  RET_CHECK(type == ::p4::v1::Update::MODIFY)
      << "Update type of DirectCounterEntry "
      << direct_counter_entry.ShortDebugString() << " must be MODIFY.";

  // Read table entry first.
  const auto& table_entry = direct_counter_entry.table_entry();
  RET_CHECK(table_entry.action().action().action_id() == 0)
      << "Found action on DirectCounterEntry "
      << direct_counter_entry.ShortDebugString();
  ASSIGN_OR_RETURN(uint32 table_id,
                   tdi_sde_interface_->GetTdiRtId(table_entry.table_id()));
  ASSIGN_OR_RETURN(auto table_key,
                   tdi_sde_interface_->CreateTableKey(table_id));
  ASSIGN_OR_RETURN(auto table_data,
                   tdi_sde_interface_->CreateTableData(
                       table_id, table_entry.action().action().action_id()));

  absl::ReaderMutexLock l(&lock_);
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));

  // Fetch existing entry with action data. This is needed since the P4RT
  // request does not provide the action ID and data, but we have to provide the
  // current values in the later modify call to the SDE, else we would modify
  // the table entry.
  RETURN_IF_ERROR(tdi_sde_interface_->GetTableEntry(
      device_, session, table_id, table_key.get(), table_data.get()));

  // P4RT spec requires that the referenced table entry must exist. Therefore we
  // do this check late.
  if (!direct_counter_entry.has_data()) {
    // Nothing to be updated.
    return ::util::OkStatus();
  }

  RETURN_IF_ERROR(
      table_data->SetCounterData(direct_counter_entry.data().byte_count(),
                                 direct_counter_entry.data().packet_count()));

  RETURN_IF_ERROR(tdi_sde_interface_->ModifyTableEntry(
      device_, session, table_id, table_key.get(), table_data.get()));

  return ::util::OkStatus();
}

// Read the counter data of a table entry.
::util::StatusOr<::p4::v1::DirectCounterEntry>
TdiTableManager::ReadDirectCounterEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::DirectCounterEntry& direct_counter_entry) {
  const auto& table_entry = direct_counter_entry.table_entry();
  RET_CHECK(table_entry.action().action().action_id() == 0)
      << "Found action on DirectCounterEntry "
      << direct_counter_entry.ShortDebugString();

  ASSIGN_OR_RETURN(uint32 table_id,
                   tdi_sde_interface_->GetTdiRtId(table_entry.table_id()));
  ASSIGN_OR_RETURN(auto table_key,
                   tdi_sde_interface_->CreateTableKey(table_id));
  ASSIGN_OR_RETURN(auto table_data,
                   tdi_sde_interface_->CreateTableData(
                       table_id, table_entry.action().action().action_id()));

  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));
  }

  // Sync table counters.
  RETURN_IF_ERROR(tdi_sde_interface_->SynchronizeCounters(
      device_, session, table_id,
      absl::Milliseconds(FLAGS_tdi_table_sync_timeout_ms)));

  RETURN_IF_ERROR(tdi_sde_interface_->GetTableEntry(
      device_, session, table_id, table_key.get(), table_data.get()));

  // TODO(max): build response entry from returned data
  ::p4::v1::DirectCounterEntry result = direct_counter_entry;

  uint64 bytes = 0;
  uint64 packets = 0;
  RETURN_IF_ERROR(table_data->GetCounterData(&bytes, &packets));
  result.mutable_data()->set_byte_count(static_cast<int64>(bytes));
  result.mutable_data()->set_packet_count(static_cast<int64>(packets));

  return result;
}

::util::Status TdiTableManager::ReadRegisterEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::RegisterEntry& register_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_ERROR(p4_info_manager_->VerifyRegisterEntry(register_entry));
  }

  // Index 0 is a valid value and not a wildcard.
  absl::optional<uint32> optional_register_index;
  if (register_entry.has_index()) {
    optional_register_index = register_entry.index().index();
  }

  // TODO(max): we don't translate p4rt id to TDI here?
  std::vector<uint32> register_indices;
  std::vector<uint64> register_values;
  RETURN_IF_ERROR(tdi_sde_interface_->ReadRegisters(
      device_, session, register_entry.register_id(), optional_register_index,
      &register_indices, &register_values,
      absl::Milliseconds(FLAGS_tdi_table_sync_timeout_ms)));

  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < register_indices.size(); ++i) {
    const uint32 register_index = register_indices[i];
    const uint64 register_datum = register_values[i];
    ::p4::v1::RegisterEntry result;

    result.set_register_id(register_entry.register_id());
    result.mutable_index()->set_index(register_index);
    // TODO(max): Switch to tuple form, once compiler support landed.
    // ::p4::v1::P4StructLike register_tuple;
    // for (const auto& data : register_datum) {
    //   LOG(INFO) << data;
    //   register_tuple.add_members()->set_bitstring(Uint64ToByteStream(data));
    // }
    // *result.mutable_data()->mutable_tuple() = register_tuple;
    result.mutable_data()->set_bitstring(Uint64ToByteStream(register_datum));

    *resp.add_entities()->mutable_register_entry() = result;
  }

  VLOG(1) << "ReadRegisterEntry resp " << resp.DebugString();
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

::util::Status TdiTableManager::WriteRegisterEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::RegisterEntry& register_entry) {
  RET_CHECK(type == ::p4::v1::Update::MODIFY)
      << "Update type of RegisterEntry " << register_entry.ShortDebugString()
      << " must be MODIFY.";
  RET_CHECK(register_entry.has_data())
      << "RegisterEntry " << register_entry.ShortDebugString()
      << " must have data.";
  RET_CHECK(register_entry.data().data_case() == ::p4::v1::P4Data::kBitstring)
      << "Only bitstring registers data types are supported.";

  ASSIGN_OR_RETURN(uint32 table_id, tdi_sde_interface_->GetTdiRtId(
                                        register_entry.register_id()));

  absl::optional<uint32> register_index;
  if (register_entry.has_index()) {
    register_index = register_entry.index().index();
  }
  RETURN_IF_ERROR(tdi_sde_interface_->WriteRegister(
      device_, session, table_id, register_index,
      register_entry.data().bitstring()));

  return ::util::OkStatus();
}

::util::Status TdiTableManager::ReadMeterEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::MeterEntry& meter_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  RET_CHECK(meter_entry.meter_id() != 0)
      << "Wildcard MeterEntry reads are not supported.";
  ASSIGN_OR_RETURN(uint32 table_id,
                   tdi_sde_interface_->GetTdiRtId(meter_entry.meter_id()));
  bool meter_units_in_bits;  // or packets
  {
    absl::ReaderMutexLock l(&lock_);
    ASSIGN_OR_RETURN(auto meter,
                     p4_info_manager_->FindMeterByID(meter_entry.meter_id()));
    switch (meter.spec().unit()) {
      case ::p4::config::v1::MeterSpec::BYTES:
        meter_units_in_bits = true;
        break;
      case ::p4::config::v1::MeterSpec::PACKETS:
        meter_units_in_bits = false;
        break;
      default:
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Unsupported meter spec on meter " << meter.ShortDebugString()
               << ".";
    }
  }
  // Index 0 is a valid value and not a wildcard.
  absl::optional<uint32> optional_meter_index;
  if (meter_entry.has_index()) {
    optional_meter_index = meter_entry.index().index();
  }

  std::vector<uint32> meter_indices;
  std::vector<uint64> cirs;
  std::vector<uint64> cbursts;
  std::vector<uint64> pirs;
  std::vector<uint64> pbursts;
  std::vector<bool> in_pps;
  RETURN_IF_ERROR(tdi_sde_interface_->ReadIndirectMeters(
      device_, session, table_id, optional_meter_index, &meter_indices, &cirs,
      &cbursts, &pirs, &pbursts, &in_pps));

  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < meter_indices.size(); ++i) {
    ::p4::v1::MeterEntry result;
    result.set_meter_id(meter_entry.meter_id());
    result.mutable_index()->set_index(meter_indices[i]);
    result.mutable_config()->set_cir(cirs[i]);
    result.mutable_config()->set_cburst(cbursts[i]);
    result.mutable_config()->set_pir(pirs[i]);
    result.mutable_config()->set_pburst(pbursts[i]);

    *resp.add_entities()->mutable_meter_entry() = result;
  }

  VLOG(1) << "ReadMeterEntry resp " << resp.DebugString();
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

::util::Status TdiTableManager::WriteMeterEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::MeterEntry& meter_entry) {
  RET_CHECK(type == ::p4::v1::Update::MODIFY)
      << "Update type of RegisterEntry " << meter_entry.ShortDebugString()
      << " must be MODIFY.";
  RET_CHECK(meter_entry.meter_id() != 0)
      << "Missing meter id in MeterEntry " << meter_entry.ShortDebugString()
      << ".";

  bool meter_units_in_packets;  // or bytes
  {
    absl::ReaderMutexLock l(&lock_);
    ASSIGN_OR_RETURN(auto meter,
                     p4_info_manager_->FindMeterByID(meter_entry.meter_id()));
    switch (meter.spec().unit()) {
      case ::p4::config::v1::MeterSpec::BYTES:
        meter_units_in_packets = false;
        break;
      case ::p4::config::v1::MeterSpec::PACKETS:
        meter_units_in_packets = true;
        break;
      default:
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Unsupported meter spec on meter " << meter.ShortDebugString()
               << ".";
    }
  }

  ASSIGN_OR_RETURN(uint32 meter_id,
                   tdi_sde_interface_->GetTdiRtId(meter_entry.meter_id()));

  absl::optional<uint32> meter_index;
  if (meter_entry.has_index()) {
    meter_index = meter_entry.index().index();
  }
  RETURN_IF_ERROR(tdi_sde_interface_->WriteIndirectMeter(
      device_, session, meter_id, meter_index, meter_units_in_packets,
      meter_entry.config().cir(), meter_entry.config().cburst(),
      meter_entry.config().pir(), meter_entry.config().pburst()));

  return ::util::OkStatus();
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
