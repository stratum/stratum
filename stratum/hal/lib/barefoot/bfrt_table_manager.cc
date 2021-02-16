// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/synchronization/notification.h"
#include "gflags/gflags.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
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
                                   BfSdeInterface* bf_sde_interface, int device)
    : mode_(mode),
      register_timer_descriptors_(),
      p4_info_manager_(nullptr),
      bf_sde_interface_(ABSL_DIE_IF_NULL(bf_sde_interface)),
      device_(device) {}

std::unique_ptr<BfrtTableManager> BfrtTableManager::CreateInstance(
    OperationMode mode, BfSdeInterface* bf_sde_interface, int device) {
  return absl::WrapUnique(new BfrtTableManager(mode, bf_sde_interface, device));
}
namespace {
struct RegisterClearThreadData {
  std::vector<p4::config::v1::Register> registers;
  BfrtTableManager* mgr;

  explicit RegisterClearThreadData(BfrtTableManager* _mgr)
      : registers(), mgr(_mgr) {}
};
}  // namespace

::util::Status BfrtTableManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config) {
  absl::WriterMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(config.programs_size() == 1)
      << "Only one P4 program is supported.";
  register_timer_descriptors_.clear();
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
        ASSIGN_OR_RETURN(auto session, bf_sde_interface_->CreateSession());
        RETURN_IF_ERROR(session->BeginBatch());
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
        RETURN_IF_ERROR(session->EndBatch());
        session.reset();

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
    const ::p4::v1::TableEntry& table_entry,
    BfSdeInterface::TableKeyInterface* table_key) {
  CHECK_RETURN_IF_FALSE(table_key);
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
          CHECK_RETURN_IF_FALSE(expected_match_field.match_type() ==
                                ::p4::config::v1::MatchField::EXACT)
              << "Found match field of type EXACT does not fit match field "
              << expected_match_field.ShortDebugString() << ".";
          CHECK_RETURN_IF_FALSE(!IsDontCareMatch(mk.exact()));
          RETURN_IF_ERROR(
              table_key->SetExact(mk.field_id(), mk.exact().value()));
          break;
        }
        case ::p4::v1::FieldMatch::kTernary: {
          CHECK_RETURN_IF_FALSE(expected_match_field.match_type() ==
                                ::p4::config::v1::MatchField::TERNARY)
              << "Found match field of type TERNARY does not fit match field "
              << expected_match_field.ShortDebugString() << ".";
          CHECK_RETURN_IF_FALSE(!IsDontCareMatch(mk.ternary()));
          RETURN_IF_ERROR(table_key->SetTernary(
              mk.field_id(), mk.ternary().value(), mk.ternary().mask()));
          break;
        }
        case ::p4::v1::FieldMatch::kLpm: {
          CHECK_RETURN_IF_FALSE(expected_match_field.match_type() ==
                                ::p4::config::v1::MatchField::LPM)
              << "Found match field of type LPM does not fit match field "
              << expected_match_field.ShortDebugString() << ".";
          CHECK_RETURN_IF_FALSE(!IsDontCareMatch(mk.lpm()));
          RETURN_IF_ERROR(table_key->SetLpm(mk.field_id(), mk.lpm().value(),
                                            mk.lpm().prefix_len()));
          break;
        }
        case ::p4::v1::FieldMatch::kRange: {
          CHECK_RETURN_IF_FALSE(expected_match_field.match_type() ==
                                ::p4::config::v1::MatchField::RANGE)
              << "Found match field of type Range does not fit match field "
              << expected_match_field.ShortDebugString() << ".";
          // TODO(max): Do we need to check this for range matches?
          // CHECK_RETURN_IF_FALSE(!IsDontCareMatch(match.range(), ));
          RETURN_IF_ERROR(table_key->SetRange(mk.field_id(), mk.range().low(),
                                              mk.range().high()));
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
          RETURN_ERROR(ERR_INVALID_PARAM)
              << "Invalid field match type "
              << ::p4::config::v1::MatchField_MatchType_Name(
                     expected_match_field.match_type())
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
    ASSIGN_OR_RETURN(uint64 priority,
                     ConvertPriorityFromP4rtToBfrt(table_entry.priority()));
    RETURN_IF_ERROR(table_key->SetPriority(priority));
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::BuildTableActionData(
    const ::p4::v1::Action& action,
    BfSdeInterface::TableDataInterface* table_data) {
  RETURN_IF_ERROR(table_data->Reset(action.action_id()));
  for (const auto& param : action.params()) {
    RETURN_IF_ERROR(table_data->SetParam(param.param_id(), param.value()));
  }
  return ::util::OkStatus();
}

::util::Status BfrtTableManager::BuildTableData(
    const ::p4::v1::TableEntry& table_entry,
    BfSdeInterface::TableDataInterface* table_data) {
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
      RETURN_ERROR(ERR_UNIMPLEMENTED)
          << "Unsupported action type: " << table_entry.action().type_case();
  }

  if (table_entry.has_counter_data()) {
    RETURN_IF_ERROR(
        table_data->SetCounterData(table_entry.counter_data().byte_count(),
                                   table_entry.counter_data().packet_count()));
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::WriteTableEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::TableEntry& table_entry) {
  CHECK_RETURN_IF_FALSE(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;

  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(uint32 table_id,
                   bf_sde_interface_->GetBfRtId(table_entry.table_id()));

  if (!table_entry.is_default_action()) {
    ASSIGN_OR_RETURN(auto table_key,
                     bf_sde_interface_->CreateTableKey(table_id));
    RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));

    ASSIGN_OR_RETURN(auto table_data,
                     bf_sde_interface_->CreateTableData(
                         table_id, table_entry.action().action().action_id()));
    if (type == ::p4::v1::Update::INSERT || type == ::p4::v1::Update::MODIFY) {
      RETURN_IF_ERROR(BuildTableData(table_entry, table_data.get()));
    }

    switch (type) {
      case ::p4::v1::Update::INSERT:
        RETURN_IF_ERROR(bf_sde_interface_->InsertTableEntry(
            device_, session, table_id, table_key.get(), table_data.get()));
        break;
      case ::p4::v1::Update::MODIFY:
        RETURN_IF_ERROR(bf_sde_interface_->ModifyTableEntry(
            device_, session, table_id, table_key.get(), table_data.get()));
        break;
      case ::p4::v1::Update::DELETE:
        RETURN_IF_ERROR(bf_sde_interface_->DeleteTableEntry(
            device_, session, table_id, table_key.get()));
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
      ASSIGN_OR_RETURN(
          auto table_data,
          bf_sde_interface_->CreateTableData(
              table_id, table_entry.action().action().action_id()));
      RETURN_IF_ERROR(BuildTableData(table_entry, table_data.get()));
      RETURN_IF_ERROR(bf_sde_interface_->SetDefaultTableEntry(
          device_, session, table_id, table_data.get()));
    } else {
      RETURN_IF_ERROR(bf_sde_interface_->ResetDefaultTableEntry(
          device_, session, table_id));
    }
  }

  return ::util::OkStatus();
}

// TODO(max): the need for the original request might go away when the table
// data is correctly initialized with only the fields we care about.
::util::StatusOr<::p4::v1::TableEntry> BfrtTableManager::BuildP4TableEntry(
    const ::p4::v1::TableEntry& request,
    const BfSdeInterface::TableKeyInterface* table_key,
    const BfSdeInterface::TableDataInterface* table_data) {
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
        RETURN_ERROR(ERR_INVALID_PARAM)
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
                     ConvertPriorityFromBfrtToP4rt(bf_priority));
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

  // Counter data
  uint64 bytes, packets;
  if (table_data->GetCounterData(&bytes, &packets).ok()) {
    if (request.has_counter_data()) {
      result.mutable_counter_data()->set_byte_count(bytes);
      result.mutable_counter_data()->set_packet_count(packets);
    }
  }

  return result;
}

::util::Status BfrtTableManager::ReadSingleTableEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::TableEntry& table_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  ASSIGN_OR_RETURN(uint32 table_id,
                   bf_sde_interface_->GetBfRtId(table_entry.table_id()));
  ASSIGN_OR_RETURN(auto table_key, bf_sde_interface_->CreateTableKey(table_id));
  ASSIGN_OR_RETURN(auto table_data,
                   bf_sde_interface_->CreateTableData(
                       table_id, table_entry.action().action().action_id()));
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));
  RETURN_IF_ERROR(bf_sde_interface_->GetTableEntry(
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

::util::Status BfrtTableManager::ReadDefaultTableEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::TableEntry& table_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  CHECK_RETURN_IF_FALSE(table_entry.table_id())
      << "Missing table id on default action read "
      << table_entry.ShortDebugString() << ".";

  ASSIGN_OR_RETURN(uint32 table_id,
                   bf_sde_interface_->GetBfRtId(table_entry.table_id()));
  ASSIGN_OR_RETURN(auto table_key, bf_sde_interface_->CreateTableKey(table_id));
  ASSIGN_OR_RETURN(auto table_data,
                   bf_sde_interface_->CreateTableData(
                       table_id, table_entry.action().action().action_id()));
  RETURN_IF_ERROR(bf_sde_interface_->GetDefaultTableEntry(
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

::util::Status BfrtTableManager::ReadAllTableEntries(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
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

  ASSIGN_OR_RETURN(uint32 table_id,
                   bf_sde_interface_->GetBfRtId(table_entry.table_id()));
  std::vector<std::unique_ptr<BfSdeInterface::TableKeyInterface>> keys;
  std::vector<std::unique_ptr<BfSdeInterface::TableDataInterface>> datas;
  RETURN_IF_ERROR(bf_sde_interface_->GetAllTableEntries(
      device_, session, table_id, &keys, &datas));
  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<BfSdeInterface::TableKeyInterface>& table_key =
        keys[i];
    const std::unique_ptr<BfSdeInterface::TableDataInterface>& table_data =
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

::util::Status BfrtTableManager::ReadTableEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::TableEntry& table_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  CHECK_RETURN_IF_FALSE(writer) << "Null writer.";
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
        RETURN_IF_ERROR(bf_sde_interface_->SynchronizeCounters(
            device_, session, wanted_table_entry.table_id(),
            absl::Milliseconds(FLAGS_bfrt_table_sync_timeout_ms)));
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
      RETURN_IF_ERROR(bf_sde_interface_->SynchronizeCounters(
          device_, session, table_entry.table_id(),
          absl::Milliseconds(FLAGS_bfrt_table_sync_timeout_ms)));
    }
    return ReadSingleTableEntry(session, table_entry, writer);
  }

  CHECK(false) << "This should never happen.";
}

// Modify the counter data of a table entry.
::util::Status BfrtTableManager::WriteDirectCounterEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
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
  ASSIGN_OR_RETURN(uint32 table_id,
                   bf_sde_interface_->GetBfRtId(table_entry.table_id()));
  ASSIGN_OR_RETURN(auto table_key, bf_sde_interface_->CreateTableKey(table_id));
  ASSIGN_OR_RETURN(auto table_data,
                   bf_sde_interface_->CreateTableData(
                       table_id, table_entry.action().action().action_id()));

  absl::ReaderMutexLock l(&lock_);
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));

  // Fetch existing entry with action data. This is needed since the P4RT
  // request does not provide the action (id), but the SDE requires it in the
  // later modify call.
  RETURN_IF_ERROR(bf_sde_interface_->GetTableEntry(
      device_, session, table_id, table_key.get(), table_data.get()));

  // P4RT spec requires that the referenced table entry must exist. Therefore we
  // do this check late.
  if (!direct_counter_entry.has_data()) {
    // Nothing to be updated.
    return ::util::OkStatus();
  }

  RETURN_IF_ERROR(table_data->SetOnlyCounterData(
      direct_counter_entry.data().byte_count(),
      direct_counter_entry.data().packet_count()));

  RETURN_IF_ERROR(bf_sde_interface_->ModifyTableEntry(
      device_, session, table_id, table_key.get(), table_data.get()));

  return ::util::OkStatus();
}

// Read the counter data of a table entry.
::util::StatusOr<::p4::v1::DirectCounterEntry>
BfrtTableManager::ReadDirectCounterEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::DirectCounterEntry& direct_counter_entry) {
  const auto& table_entry = direct_counter_entry.table_entry();
  CHECK_RETURN_IF_FALSE(table_entry.action().action().action_id() == 0)
      << "Found action on DirectCounterEntry "
      << direct_counter_entry.ShortDebugString();

  ASSIGN_OR_RETURN(uint32 table_id,
                   bf_sde_interface_->GetBfRtId(table_entry.table_id()));
  ASSIGN_OR_RETURN(auto table_key, bf_sde_interface_->CreateTableKey(table_id));
  ASSIGN_OR_RETURN(auto table_data,
                   bf_sde_interface_->CreateTableData(
                       table_id, table_entry.action().action().action_id()));

  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));
  }

  // Sync table counters.
  RETURN_IF_ERROR(bf_sde_interface_->SynchronizeCounters(
      device_, session, table_id,
      absl::Milliseconds(FLAGS_bfrt_table_sync_timeout_ms)));

  RETURN_IF_ERROR(bf_sde_interface_->GetTableEntry(
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

::util::Status BfrtTableManager::ReadRegisterEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
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

  std::vector<uint32> register_indices;
  std::vector<uint64> register_datas;
  RETURN_IF_ERROR(bf_sde_interface_->ReadRegisters(
      device_, session, register_entry.register_id(), optional_register_index,
      &register_indices, &register_datas,
      absl::Milliseconds(FLAGS_bfrt_table_sync_timeout_ms)));

  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < register_indices.size(); ++i) {
    const uint32 register_index = register_indices[i];
    const uint64 register_data = register_datas[i];
    ::p4::v1::RegisterEntry result;

    result.set_register_id(register_entry.register_id());
    result.mutable_index()->set_index(register_index);
    // TODO(max): Switch to tuple form, once compiler support landed.
    // ::p4::v1::P4StructLike register_tuple;
    // for (const auto& data : register_data) {
    //   LOG(INFO) << data;
    //   register_tuple.add_members()->set_bitstring(Uint64ToByteStream(data));
    // }
    // *result.mutable_data()->mutable_tuple() = register_tuple;
    result.mutable_data()->set_bitstring(Uint64ToByteStream(register_data));

    *resp.add_entities()->mutable_register_entry() = result;
  }

  VLOG(1) << "ReadRegisterEntry resp " << resp.DebugString();
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::WriteRegisterEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
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

  ASSIGN_OR_RETURN(uint32 table_id,
                   bf_sde_interface_->GetBfRtId(register_entry.register_id()));

  absl::optional<uint32> register_index;
  if (register_entry.has_index()) {
    register_index = register_entry.index().index();
  }
  RETURN_IF_ERROR(bf_sde_interface_->WriteRegister(
      device_, session, table_id, register_index,
      register_entry.data().bitstring()));

  return ::util::OkStatus();
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
