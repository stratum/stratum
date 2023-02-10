// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/synchronization/notification.h"
#include "gflags/gflags.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/utils.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/utils.h"

DEFINE_uint32(
    bfrt_table_sync_timeout_ms,
    stratum::hal::barefoot::kDefaultSyncTimeout / absl::Milliseconds(1),
    "The timeout for table sync operation like counters and registers.");

namespace stratum {
namespace hal {
namespace barefoot {

BfrtTableManager::BfrtTableManager(
    OperationMode mode, BfSdeInterface* bf_sde_interface,
    BfrtP4RuntimeTranslator* bfrt_p4runtime_translator, int device)
    : mode_(mode),
      digest_rx_thread_id_(),
      bf_sde_interface_(ABSL_DIE_IF_NULL(bf_sde_interface)),
      bfrt_p4runtime_translator_(ABSL_DIE_IF_NULL(bfrt_p4runtime_translator)),
      p4_info_manager_(nullptr),
      device_(device) {}

BfrtTableManager::BfrtTableManager()
    : mode_(OPERATION_MODE_STANDALONE),
      digest_rx_thread_id_(),
      bf_sde_interface_(nullptr),
      bfrt_p4runtime_translator_(nullptr),
      p4_info_manager_(nullptr),
      device_(-1) {}

BfrtTableManager::~BfrtTableManager() = default;

std::unique_ptr<BfrtTableManager> BfrtTableManager::CreateInstance(
    OperationMode mode, BfSdeInterface* bf_sde_interface,
    BfrtP4RuntimeTranslator* bfrt_p4runtime_translator, int device) {
  return absl::WrapUnique(new BfrtTableManager(
      mode, bf_sde_interface, bfrt_p4runtime_translator, device));
}

::util::Status BfrtTableManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config) {
  absl::WriterMutexLock l(&lock_);
  RET_CHECK(config.programs_size() == 1) << "Only one P4 program is supported.";
  const auto& program = config.programs(0);
  const auto& p4_info = program.p4info();
  std::unique_ptr<P4InfoManager> p4_info_manager =
      absl::make_unique<P4InfoManager>(p4_info);
  RETURN_IF_ERROR(p4_info_manager->InitializeAndVerify());
  p4_info_manager_ = std::move(p4_info_manager);

  if (digest_rx_thread_id_ == 0) {
    digest_list_receive_channel_ =
        Channel<BfSdeInterface::DigestList>::Create(128);
    int ret = pthread_create(&digest_rx_thread_id_, nullptr,
                             &BfrtTableManager::DigestListThreadFunc, this);
    if (ret != 0) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to spawn digest list RX thread for device with ID "
             << device_ << ". Err: " << ret << ".";
    }
    RETURN_IF_ERROR(bf_sde_interface_->RegisterDigestListWriter(
        device_, ChannelWriter<BfSdeInterface::DigestList>::Create(
                     digest_list_receive_channel_)));
  }
  // Create a new session for use in digest callbacks. For now we don't modify
  // table entries in response to digests, that is up the the controller, but a
  // valid and active session is still required for the callbacks.
  ASSIGN_OR_RETURN(digest_list_session_, bf_sde_interface_->CreateSession());

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::VerifyForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) const {
  for (const auto& digest : config.p4info().digests()) {
    RET_CHECK(digest.type_spec().has_struct_())
        << "Only struct-like digests type specs are supported: "
        << digest.ShortDebugString();
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::Shutdown() {
  ::util::Status status;
  {
    absl::WriterMutexLock l(&digest_list_writer_lock_);
    digest_list_writer_ = nullptr;
  }
  {
    absl::WriterMutexLock l(&lock_);
    if (digest_rx_thread_id_ != 0) {
      APPEND_STATUS_IF_ERROR(
          status, bf_sde_interface_->UnregisterDigestListWriter(device_));
      if (!digest_list_receive_channel_ ||
          !digest_list_receive_channel_->Close()) {
        ::util::Status error = MAKE_ERROR(ERR_INTERNAL)
                               << "Digest list channel is already closed.";
        APPEND_STATUS_IF_ERROR(status, error);
      }
    }
    digest_list_receive_channel_.reset();
    digest_list_session_.reset();
  }
  // TODO(max): we release the locks between closing the channel and joining the
  // thread to prevent deadlocks with the RX handler. But there might still be a
  // bug hiding here.
  {
    absl::ReaderMutexLock l(&lock_);
    if (digest_rx_thread_id_ != 0 &&
        pthread_join(digest_rx_thread_id_, nullptr) != 0) {
      ::util::Status error = MAKE_ERROR(ERR_INTERNAL)
                             << "Failed to join thread "
                             << digest_rx_thread_id_;
      APPEND_STATUS_IF_ERROR(status, error);
    }
  }
  {
    absl::WriterMutexLock l(&lock_);
    digest_rx_thread_id_ = 0;
  }

  return status;
}

::util::Status BfrtTableManager::BuildTableKey(
    const ::p4::v1::TableEntry& table_entry,
    BfSdeInterface::TableKeyInterface* table_key) {
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
          RET_CHECK(!IsDontCareMatch(mk.exact()))
              << "Don't care match " << mk.ShortDebugString()
              << " must be omitted.";
          RETURN_IF_ERROR(
              table_key->SetExact(mk.field_id(), mk.exact().value()));
          break;
        }
        case ::p4::v1::FieldMatch::kTernary: {
          RET_CHECK(expected_match_field.match_type() ==
                    ::p4::config::v1::MatchField::TERNARY)
              << "Found match field of type TERNARY does not fit match field "
              << expected_match_field.ShortDebugString() << ".";
          RET_CHECK(!IsDontCareMatch(mk.ternary()))
              << "Don't care match " << mk.ShortDebugString()
              << " must be omitted.";
          RETURN_IF_ERROR(table_key->SetTernary(
              mk.field_id(), mk.ternary().value(), mk.ternary().mask()));
          break;
        }
        case ::p4::v1::FieldMatch::kLpm: {
          RET_CHECK(expected_match_field.match_type() ==
                    ::p4::config::v1::MatchField::LPM)
              << "Found match field of type LPM does not fit match field "
              << expected_match_field.ShortDebugString() << ".";
          RET_CHECK(!IsDontCareMatch(mk.lpm()))
              << "Don't care match " << mk.ShortDebugString()
              << " must be omitted.";
          RETURN_IF_ERROR(table_key->SetLpm(mk.field_id(), mk.lpm().value(),
                                            mk.lpm().prefix_len()));
          break;
        }
        case ::p4::v1::FieldMatch::kRange: {
          RET_CHECK(expected_match_field.match_type() ==
                    ::p4::config::v1::MatchField::RANGE)
              << "Found match field of type Range does not fit match field "
              << expected_match_field.ShortDebugString() << ".";
          RET_CHECK(
              !IsDontCareMatch(mk.range(), expected_match_field.bitwidth()))
              << "Don't care match " << mk.ShortDebugString()
              << " must be omitted.";
          RETURN_IF_ERROR(table_key->SetRange(mk.field_id(), mk.range().low(),
                                              mk.range().high()));
          break;
        }
        case ::p4::v1::FieldMatch::kOptional:
          RET_CHECK(!IsDontCareMatch(mk.optional()))
              << "Don't care match field " << mk.ShortDebugString()
              << " must be omitted.";
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
      return MAKE_ERROR(ERR_UNIMPLEMENTED)
             << "Unsupported action type: " << table_entry.action().type_case();
  }

  if (table_entry.has_counter_data()) {
    RETURN_IF_ERROR(
        table_data->SetCounterData(table_entry.counter_data().byte_count(),
                                   table_entry.counter_data().packet_count()));
  }

  if (table_entry.has_meter_config()) {
    return MAKE_ERROR(ERR_UNIMPLEMENTED)
           << "Meter configs on TablesEntries are not supported.";
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::WriteTableEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::TableEntry& table_entry) {
  RET_CHECK(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;
  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(const auto& translated_table_entry,
                   bfrt_p4runtime_translator_->TranslateTableEntry(
                       table_entry, /*to_sdk=*/true));

  ASSIGN_OR_RETURN(auto table, p4_info_manager_->FindTableByID(
                                   translated_table_entry.table_id()));
  ASSIGN_OR_RETURN(uint32 table_id, bf_sde_interface_->GetBfRtId(
                                        translated_table_entry.table_id()));

  if (!translated_table_entry.is_default_action()) {
    if (table.is_const_table()) {
      return MAKE_ERROR(ERR_PERMISSION_DENIED)
             << "Can't write to const table " << table.preamble().name()
             << " because it has const entries.";
    }
    ASSIGN_OR_RETURN(auto table_key,
                     bf_sde_interface_->CreateTableKey(table_id));
    RETURN_IF_ERROR(BuildTableKey(translated_table_entry, table_key.get()));

    ASSIGN_OR_RETURN(
        auto table_data,
        bf_sde_interface_->CreateTableData(
            table_id, translated_table_entry.action().action().action_id()));
    if (type == ::p4::v1::Update::INSERT || type == ::p4::v1::Update::MODIFY) {
      RETURN_IF_ERROR(BuildTableData(translated_table_entry, table_data.get()));
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
        return MAKE_ERROR(ERR_INTERNAL)
               << "Unsupported update type: " << type << " in table entry "
               << translated_table_entry.ShortDebugString() << ".";
    }
  } else {
    RET_CHECK(type == ::p4::v1::Update::MODIFY)
        << "The default table entry can only be modified.";
    RET_CHECK(translated_table_entry.match_size() == 0)
        << "Default action must not contain match fields.";
    RET_CHECK(translated_table_entry.priority() == 0)
        << "Default action must not contain a priority field.";

    if (translated_table_entry.has_action()) {
      ASSIGN_OR_RETURN(
          auto table_data,
          bf_sde_interface_->CreateTableData(
              table_id, translated_table_entry.action().action().action_id()));
      RETURN_IF_ERROR(BuildTableData(translated_table_entry, table_data.get()));
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

  // Counter data, if applicable.
  uint64 bytes, packets;
  if (request.has_counter_data() &&
      table_data->GetCounterData(&bytes, &packets).ok()) {
    result.mutable_counter_data()->set_byte_count(bytes);
    result.mutable_counter_data()->set_packet_count(packets);
  }

  return result;
}

::util::StatusOr<::p4::v1::DigestList> BfrtTableManager::BuildP4DigestList(
    const BfSdeInterface::DigestList& digest_list) {
  absl::ReaderMutexLock l(&lock_);
  ::p4::v1::DigestList result;

  ASSIGN_OR_RETURN(auto p4_digest_id,
                   bf_sde_interface_->GetP4InfoId(digest_list.digest_id));

  ASSIGN_OR_RETURN(auto digest, p4_info_manager_->FindDigestByID(p4_digest_id));

  result.set_digest_id(p4_digest_id);
  result.set_list_id(-1);  // currently not used, as digests are acked already.
  result.set_timestamp(absl::ToUnixNanos(digest_list.timestamp));

  // TODO(max): check that the digest conforms to its definition in P4Info.

  // Transform the SDE digest into a P4RT struct-like digest.
  for (const auto& digest_entry : digest_list.digests) {
    ::p4::v1::P4Data* data = result.add_data();
    for (const auto& field : digest_entry) {
      data->mutable_struct_()->add_members()->set_bitstring(field);
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
  ASSIGN_OR_RETURN(*resp.add_entities()->mutable_table_entry(),
                   bfrt_p4runtime_translator_->TranslateTableEntry(
                       result, /*to_sdk=*/false));
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
  RET_CHECK(table_entry.table_id())
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
  ASSIGN_OR_RETURN(*resp.add_entities()->mutable_table_entry(),
                   bfrt_p4runtime_translator_->TranslateTableEntry(
                       result, /*to_sdk=*/false));
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
    ASSIGN_OR_RETURN(*resp.add_entities()->mutable_table_entry(),
                     bfrt_p4runtime_translator_->TranslateTableEntry(
                         result, /*to_sdk=*/false));
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
  RET_CHECK(writer) << "Null writer.";
  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(const auto& translated_table_entry,
                   bfrt_p4runtime_translator_->TranslateTableEntry(
                       table_entry, /*to_sdk=*/true));

  // We have four cases to handle:
  // 1. table id not set: return all table entries from all tables
  // 2. table id set, no match key: return all table entries of that table
  // 3. table id set, no match key, is_default_action set: return default action
  // 4. table id and match key: return single entry

  if (translated_table_entry.match_size() == 0 &&
      !translated_table_entry.is_default_action()) {
    std::vector<::p4::v1::TableEntry> wanted_tables;
    if (translated_table_entry.table_id() == 0) {
      // 1.
      const ::p4::config::v1::P4Info& p4_info = p4_info_manager_->p4_info();
      for (const auto& table : p4_info.tables()) {
        ::p4::v1::TableEntry te;
        te.set_table_id(table.preamble().id());
        if (translated_table_entry.has_counter_data()) {
          te.mutable_counter_data();
        }
        wanted_tables.push_back(te);
      }
    } else {
      // 2.
      wanted_tables.push_back(translated_table_entry);
    }
    // TODO(max): can wildcard reads request counter_data?
    if (translated_table_entry.has_counter_data()) {
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
          << translated_table_entry.ShortDebugString() << ".";
    }
    return ::util::OkStatus();
  } else if (translated_table_entry.match_size() == 0 &&
             translated_table_entry.is_default_action()) {
    // 3.
    return ReadDefaultTableEntry(session, translated_table_entry, writer);
  } else {
    // 4.
    if (translated_table_entry.has_counter_data()) {
      RETURN_IF_ERROR(bf_sde_interface_->SynchronizeCounters(
          device_, session, table_entry.table_id(),
          absl::Milliseconds(FLAGS_bfrt_table_sync_timeout_ms)));
    }
    return ReadSingleTableEntry(session, translated_table_entry, writer);
  }

  CHECK(false) << "This should never happen.";
}

// Modify the counter data of a table entry.
::util::Status BfrtTableManager::WriteDirectCounterEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::DirectCounterEntry& direct_counter_entry) {
  RET_CHECK(type == ::p4::v1::Update::MODIFY)
      << "Update type of DirectCounterEntry "
      << direct_counter_entry.ShortDebugString() << " must be MODIFY.";
  ASSIGN_OR_RETURN(const auto& translated_direct_counter_entry,
                   bfrt_p4runtime_translator_->TranslateDirectCounterEntry(
                       direct_counter_entry, /*to_sdk=*/true));
  // Read table entry first.
  const auto& table_entry = translated_direct_counter_entry.table_entry();
  ASSIGN_OR_RETURN(uint32 table_id,
                   bf_sde_interface_->GetBfRtId(table_entry.table_id()));
  ASSIGN_OR_RETURN(auto table_key, bf_sde_interface_->CreateTableKey(table_id));
  ASSIGN_OR_RETURN(auto table_data,
                   bf_sde_interface_->CreateTableData(table_id, 0));

  absl::ReaderMutexLock l(&lock_);
  RETURN_IF_ERROR(BuildTableKey(table_entry, table_key.get()));

  // Fetch existing entry with action data. This is needed since the P4RT
  // request does not provide the action ID and data, but we have to provide the
  // current values in the later modify call to the SDE, else we would modify
  // the table entry.
  RETURN_IF_ERROR(bf_sde_interface_->GetTableEntry(
      device_, session, table_id, table_key.get(), table_data.get()));

  // P4RT spec requires that the referenced table entry must exist. Therefore we
  // do this check late.
  if (!translated_direct_counter_entry.has_data()) {
    // Nothing to be updated.
    return ::util::OkStatus();
  }

  RETURN_IF_ERROR(table_data->SetCounterData(
      translated_direct_counter_entry.data().byte_count(),
      translated_direct_counter_entry.data().packet_count()));

  RETURN_IF_ERROR(bf_sde_interface_->ModifyTableEntry(
      device_, session, table_id, table_key.get(), table_data.get()));

  return ::util::OkStatus();
}

// Read the counter data of a table entry.
::util::StatusOr<::p4::v1::DirectCounterEntry>
BfrtTableManager::ReadDirectCounterEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::DirectCounterEntry& direct_counter_entry) {
  ASSIGN_OR_RETURN(const auto& translated_direct_counter_entry,
                   bfrt_p4runtime_translator_->TranslateDirectCounterEntry(
                       direct_counter_entry, /*to_sdk=*/true));
  const auto& table_entry = translated_direct_counter_entry.table_entry();
  ASSIGN_OR_RETURN(uint32 table_id,
                   bf_sde_interface_->GetBfRtId(table_entry.table_id()));
  ASSIGN_OR_RETURN(auto table_key, bf_sde_interface_->CreateTableKey(table_id));
  ASSIGN_OR_RETURN(auto table_data,
                   bf_sde_interface_->CreateTableData(table_id, 0));

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
  ::p4::v1::DirectCounterEntry result(translated_direct_counter_entry);

  uint64 bytes = 0;
  uint64 packets = 0;
  RETURN_IF_ERROR(table_data->GetCounterData(&bytes, &packets));
  result.mutable_data()->set_byte_count(static_cast<int64>(bytes));
  result.mutable_data()->set_packet_count(static_cast<int64>(packets));

  return bfrt_p4runtime_translator_->TranslateDirectCounterEntry(
      result, /*to_sdk=*/false);
}

::util::Status BfrtTableManager::ReadRegisterEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::RegisterEntry& register_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  ASSIGN_OR_RETURN(const auto& translated_register_entry,
                   bfrt_p4runtime_translator_->TranslateRegisterEntry(
                       register_entry, /*to_sdk=*/true));
  {
    absl::ReaderMutexLock l(&lock_);
    RETURN_IF_ERROR(
        p4_info_manager_->VerifyRegisterEntry(translated_register_entry));
  }

  // Index 0 is a valid value and not a wildcard.
  absl::optional<uint32> optional_register_index;
  if (translated_register_entry.has_index()) {
    optional_register_index = translated_register_entry.index().index();
  }

  ASSIGN_OR_RETURN(
      uint32 table_id,
      bf_sde_interface_->GetBfRtId(translated_register_entry.register_id()));
  std::vector<uint32> register_indices;
  std::vector<uint64> register_datas;
  RETURN_IF_ERROR(bf_sde_interface_->ReadRegisters(
      device_, session, table_id, optional_register_index, &register_indices,
      &register_datas, absl::Milliseconds(FLAGS_bfrt_table_sync_timeout_ms)));

  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < register_indices.size(); ++i) {
    const uint32 register_index = register_indices[i];
    const uint64 register_data = register_datas[i];
    ::p4::v1::RegisterEntry result;

    result.set_register_id(translated_register_entry.register_id());
    result.mutable_index()->set_index(register_index);
    // TODO(max): Switch to tuple form, once compiler support landed.
    // ::p4::v1::P4StructLike register_tuple;
    // for (const auto& data : register_data) {
    //   LOG(INFO) << data;
    //   register_tuple.add_members()->set_bitstring(Uint64ToByteStream(data));
    // }
    // *result.mutable_data()->mutable_tuple() = register_tuple;
    result.mutable_data()->set_bitstring(Uint64ToByteStream(register_data));

    ASSIGN_OR_RETURN(*resp.add_entities()->mutable_register_entry(),
                     bfrt_p4runtime_translator_->TranslateRegisterEntry(
                         result, /*to_sdk=*/false));
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
  RET_CHECK(type == ::p4::v1::Update::MODIFY)
      << "Update type of RegisterEntry " << register_entry.ShortDebugString()
      << " must be MODIFY.";
  RET_CHECK(register_entry.has_data())
      << "RegisterEntry " << register_entry.ShortDebugString()
      << " must have data.";
  RET_CHECK(register_entry.data().data_case() == ::p4::v1::P4Data::kBitstring)
      << "Only bitstring registers data types are supported.";

  ASSIGN_OR_RETURN(uint32 table_id,
                   bf_sde_interface_->GetBfRtId(register_entry.register_id()));

  ASSIGN_OR_RETURN(const auto& translated_register_entry,
                   bfrt_p4runtime_translator_->TranslateRegisterEntry(
                       register_entry, /*to_sdk=*/true));
  absl::optional<uint32> register_index;
  if (translated_register_entry.has_index()) {
    register_index = translated_register_entry.index().index();
  }
  RETURN_IF_ERROR(bf_sde_interface_->WriteRegister(
      device_, session, table_id, register_index,
      translated_register_entry.data().bitstring()));

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::ReadMeterEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::MeterEntry& meter_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  ASSIGN_OR_RETURN(const auto& translated_meter_entry,
                   bfrt_p4runtime_translator_->TranslateMeterEntry(
                       meter_entry, /*to_sdk=*/true));
  RET_CHECK(translated_meter_entry.meter_id() != 0)
      << "Wildcard MeterEntry reads are not supported.";
  ASSIGN_OR_RETURN(uint32 table_id, bf_sde_interface_->GetBfRtId(
                                        translated_meter_entry.meter_id()));
  bool meter_units_in_bits;  // or packets
  {
    absl::ReaderMutexLock l(&lock_);
    ASSIGN_OR_RETURN(auto meter, p4_info_manager_->FindMeterByID(
                                     translated_meter_entry.meter_id()));
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
  if (translated_meter_entry.has_index()) {
    optional_meter_index = translated_meter_entry.index().index();
  }

  std::vector<uint32> meter_indices;
  std::vector<uint64> cirs;
  std::vector<uint64> cbursts;
  std::vector<uint64> pirs;
  std::vector<uint64> pbursts;
  std::vector<bool> in_pps;
  RETURN_IF_ERROR(bf_sde_interface_->ReadIndirectMeters(
      device_, session, table_id, optional_meter_index, &meter_indices, &cirs,
      &cbursts, &pirs, &pbursts, &in_pps));

  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < meter_indices.size(); ++i) {
    ::p4::v1::MeterEntry result;
    result.set_meter_id(translated_meter_entry.meter_id());
    result.mutable_index()->set_index(meter_indices[i]);
    if (cirs[i] >= kUnsetMeterThresholdRead) {
      // The high value returned from the SDE indicates that this meter is
      // unset, i.e., in "all green" configuration. According to the P4Rtuntime
      // spec, this means we have to leave the MeterConfig field unset. Since it
      // is not possible to just configure a subset of the four fields, we just
      // have to check the cir value.
    } else {
      result.mutable_config()->set_cir(cirs[i]);
      result.mutable_config()->set_cburst(cbursts[i]);
      result.mutable_config()->set_pir(pirs[i]);
      result.mutable_config()->set_pburst(pbursts[i]);
    }

    ASSIGN_OR_RETURN(*resp.add_entities()->mutable_meter_entry(),
                     bfrt_p4runtime_translator_->TranslateMeterEntry(
                         result, /*to_sdk=*/false));
  }

  VLOG(1) << "ReadMeterEntry resp " << resp.DebugString();
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::WriteMeterEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::MeterEntry& meter_entry) {
  RET_CHECK(type == ::p4::v1::Update::MODIFY)
      << "Update type of MeterEntry " << meter_entry.ShortDebugString()
      << " must be MODIFY.";
  ASSIGN_OR_RETURN(const auto& translated_meter_entry,
                   bfrt_p4runtime_translator_->TranslateMeterEntry(
                       meter_entry, /*to_sdk=*/true));
  RET_CHECK(translated_meter_entry.meter_id() != 0)
      << "Missing meter id in MeterEntry "
      << translated_meter_entry.ShortDebugString() << ".";

  bool meter_units_in_packets;  // or bytes
  {
    absl::ReaderMutexLock l(&lock_);
    ASSIGN_OR_RETURN(auto meter, p4_info_manager_->FindMeterByID(
                                     translated_meter_entry.meter_id()));
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

  ASSIGN_OR_RETURN(uint32 meter_id, bf_sde_interface_->GetBfRtId(
                                        translated_meter_entry.meter_id()));

  absl::optional<uint32> meter_index;
  if (translated_meter_entry.has_index()) {
    meter_index = translated_meter_entry.index().index();
  }
  if (translated_meter_entry.has_config()) {
    RETURN_IF_ERROR(IsValidMeterConfig(translated_meter_entry.config()));
    RETURN_IF_ERROR(bf_sde_interface_->WriteIndirectMeter(
        device_, session, meter_id, meter_index, meter_units_in_packets,
        translated_meter_entry.config().cir(),
        translated_meter_entry.config().cburst(),
        translated_meter_entry.config().pir(),
        translated_meter_entry.config().pburst()));
  } else {
    RETURN_IF_ERROR(bf_sde_interface_->WriteIndirectMeter(
        device_, session, meter_id, meter_index, meter_units_in_packets,
        kUnsetMeterThresholdReset, kUnsetMeterThresholdReset,
        kUnsetMeterThresholdReset, kUnsetMeterThresholdReset));
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::WriteDigestEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::DigestEntry& digest_entry) {
  absl::ReaderMutexLock l(&lock_);
  const auto& translated_digest_entry = digest_entry;
  // ASSIGN_OR_RETURN(const auto& translated_digest_entry,
  //                  bfrt_p4runtime_translator_->TranslateDigestEntry(
  //                      digest_entry, /*to_sdk=*/true));
  RET_CHECK(translated_digest_entry.digest_id() != 0)
      << "Missing digest id in DigestEntry "
      << translated_digest_entry.ShortDebugString() << ".";
  absl::Duration max_timeout;
  if (type == ::p4::v1::Update::INSERT || type == ::p4::v1::Update::MODIFY) {
    RET_CHECK(translated_digest_entry.has_config())
        << "Digest entry is missing its config: "
        << translated_digest_entry.ShortDebugString();
    max_timeout =
        absl::Nanoseconds(translated_digest_entry.config().max_timeout_ns());
  }

  ASSIGN_OR_RETURN(uint32 table_id, bf_sde_interface_->GetBfRtId(
                                        translated_digest_entry.digest_id()));
  switch (type) {
    case ::p4::v1::Update::INSERT:
      RETURN_IF_ERROR(bf_sde_interface_->InsertDigest(
          device_, digest_list_session_, table_id, max_timeout));
      break;
    case ::p4::v1::Update::MODIFY:
      RETURN_IF_ERROR(bf_sde_interface_->ModifyDigest(
          device_, digest_list_session_, table_id, max_timeout));
      break;
    case ::p4::v1::Update::DELETE:
      RETURN_IF_ERROR(bf_sde_interface_->DeleteDigest(
          device_, digest_list_session_, table_id));
      break;
    default:
      return MAKE_ERROR(ERR_INTERNAL)
             << "Unsupported update type: " << type << " in digest entry "
             << translated_digest_entry.ShortDebugString() << ".";
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::ReadDigestEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::DigestEntry& digest_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  const auto& translated_digest_entry = digest_entry;
  // ASSIGN_OR_RETURN(const auto& translated_digest_entry,
  //                  bfrt_p4runtime_translator_->TranslateDigestEntry(
  //                      digest_entry, /*to_sdk=*/true));
  absl::ReaderMutexLock l(&lock_);
  RET_CHECK(translated_digest_entry.digest_id() != 0)
      << "Missing digest id in DigestEntry "
      << translated_digest_entry.ShortDebugString() << ".";
  ASSIGN_OR_RETURN(uint32 table_id, bf_sde_interface_->GetBfRtId(
                                        translated_digest_entry.digest_id()));
  std::vector<uint32> digest_ids;
  absl::Duration max_timeout;
  RETURN_IF_ERROR(bf_sde_interface_->ReadDigests(device_, session, table_id,
                                                 &digest_ids, &max_timeout));
  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < digest_ids.size(); ++i) {
    ASSIGN_OR_RETURN(auto p4_digest_id,
                     bf_sde_interface_->GetP4InfoId(digest_ids[i]));
    ::p4::v1::DigestEntry result;
    result.set_digest_id(p4_digest_id);
    result.mutable_config()->set_max_timeout_ns(
        absl::ToInt64Nanoseconds(max_timeout));
    // ASSIGN_OR_RETURN(*resp.add_entities()->mutable_digest_entry(),
    //                  bfrt_p4runtime_translator_->TranslateDigestEntry(
    //                      result, /*to_sdk=*/false));
    *resp.add_entities()->mutable_digest_entry() = result;
  }

  VLOG(1) << "ReadDigestEntry resp " << resp.ShortDebugString();
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::WriteActionProfileMember(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::ActionProfileMember& action_profile_member) {
  RET_CHECK(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;
  absl::WriterMutexLock l(&lock_);
  ASSIGN_OR_RETURN(const auto& translated_action_profile_member,
                   bfrt_p4runtime_translator_->TranslateActionProfileMember(
                       action_profile_member, /*to_sdk=*/true));
  ASSIGN_OR_RETURN(uint32 bfrt_table_id,
                   bf_sde_interface_->GetBfRtId(
                       translated_action_profile_member.action_profile_id()));

  // Action data
  ASSIGN_OR_RETURN(auto table_data,
                   bf_sde_interface_->CreateTableData(
                       bfrt_table_id,
                       translated_action_profile_member.action().action_id()));
  for (const auto& param : translated_action_profile_member.action().params()) {
    RETURN_IF_ERROR(table_data->SetParam(param.param_id(), param.value()));
  }

  switch (type) {
    case ::p4::v1::Update::INSERT: {
      RETURN_IF_ERROR(bf_sde_interface_->InsertActionProfileMember(
          device_, session, bfrt_table_id,
          translated_action_profile_member.member_id(), table_data.get()));
      break;
    }
    case ::p4::v1::Update::MODIFY: {
      RETURN_IF_ERROR(bf_sde_interface_->ModifyActionProfileMember(
          device_, session, bfrt_table_id,
          translated_action_profile_member.member_id(), table_data.get()));
      break;
    }
    case ::p4::v1::Update::DELETE: {
      RETURN_IF_ERROR(bf_sde_interface_->DeleteActionProfileMember(
          device_, session, bfrt_table_id,
          translated_action_profile_member.member_id()));
      break;
    }
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Unsupported update type: " << type;
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::ReadActionProfileMember(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::ActionProfileMember& action_profile_member,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  RET_CHECK(action_profile_member.action_profile_id() != 0)
      << "Reading all action profiles is not supported yet.";

  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(const auto& translated_action_profile_member,
                   bfrt_p4runtime_translator_->TranslateActionProfileMember(
                       action_profile_member, /*to_sdk=*/true))
  ASSIGN_OR_RETURN(uint32 bfrt_table_id,
                   bf_sde_interface_->GetBfRtId(
                       translated_action_profile_member.action_profile_id()));

  std::vector<int> member_ids;
  std::vector<std::unique_ptr<BfSdeInterface::TableDataInterface>> table_datas;
  RETURN_IF_ERROR(bf_sde_interface_->GetActionProfileMembers(
      device_, session, bfrt_table_id,
      translated_action_profile_member.member_id(), &member_ids, &table_datas));

  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < member_ids.size(); ++i) {
    const int member_id = member_ids[i];
    const std::unique_ptr<BfSdeInterface::TableDataInterface>& table_data =
        table_datas[i];

    ::p4::v1::ActionProfileMember result;
    ASSIGN_OR_RETURN(auto action_profile_id,
                     bf_sde_interface_->GetP4InfoId(bfrt_table_id));
    result.set_action_profile_id(action_profile_id);
    result.set_member_id(member_id);

    // Action id
    int action_id;
    RETURN_IF_ERROR(table_data->GetActionId(&action_id));
    result.mutable_action()->set_action_id(action_id);

    // Action data
    // TODO(max): perform check if action id is valid for this table.
    ASSIGN_OR_RETURN(auto action, p4_info_manager_->FindActionByID(action_id));
    for (const auto& expected_param : action.params()) {
      std::string value;
      RETURN_IF_ERROR(table_data->GetParam(expected_param.id(), &value));
      auto* param = result.mutable_action()->add_params();
      param->set_param_id(expected_param.id());
      param->set_value(value);
    }

    ASSIGN_OR_RETURN(*resp.add_entities()->mutable_action_profile_member(),
                     bfrt_p4runtime_translator_->TranslateActionProfileMember(
                         result, /*to_sdk*/ false));
  }

  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream channel failed.";
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::WriteActionProfileGroup(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::ActionProfileGroup& action_profile_group) {
  RET_CHECK(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;

  absl::WriterMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      uint32 bfrt_act_prof_table_id,
      bf_sde_interface_->GetBfRtId(action_profile_group.action_profile_id()));
  ASSIGN_OR_RETURN(
      uint32 bfrt_act_sel_table_id,
      bf_sde_interface_->GetActionSelectorBfRtId(bfrt_act_prof_table_id));

  std::vector<uint32> member_ids;
  std::vector<bool> member_status;
  for (const auto& member : action_profile_group.members()) {
    RET_CHECK(member.watch_kind_case() ==
              ::p4::v1::ActionProfileGroup::Member::WATCH_KIND_NOT_SET)
        << "Watch ports are not supported.";
    RET_CHECK(member.weight() != 0) << "Zero member weights are not allowed.";
    if (member.weight() != 1) {
      return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
             << "Member weights greater than 1 are not supported.";
    }
    member_ids.push_back(member.member_id());
    member_status.push_back(true);  // Activate the member.
  }

  switch (type) {
    case ::p4::v1::Update::INSERT: {
      RETURN_IF_ERROR(bf_sde_interface_->InsertActionProfileGroup(
          device_, session, bfrt_act_sel_table_id,
          action_profile_group.group_id(), action_profile_group.max_size(),
          member_ids, member_status));
      break;
    }
    case ::p4::v1::Update::MODIFY: {
      RETURN_IF_ERROR(bf_sde_interface_->ModifyActionProfileGroup(
          device_, session, bfrt_act_sel_table_id,
          action_profile_group.group_id(), action_profile_group.max_size(),
          member_ids, member_status));
      break;
    }
    case ::p4::v1::Update::DELETE: {
      RETURN_IF_ERROR(bf_sde_interface_->DeleteActionProfileGroup(
          device_, session, bfrt_act_sel_table_id,
          action_profile_group.group_id()));
      break;
    }
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Unsupported update type: " << type;
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::ReadActionProfileGroup(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::ActionProfileGroup& action_profile_group,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      uint32 bfrt_act_prof_table_id,
      bf_sde_interface_->GetBfRtId(action_profile_group.action_profile_id()));
  ASSIGN_OR_RETURN(
      uint32 bfrt_act_sel_table_id,
      bf_sde_interface_->GetActionSelectorBfRtId(bfrt_act_prof_table_id));
  RET_CHECK(action_profile_group.action_profile_id() != 0)
      << "Reading all action profiles is not supported yet.";

  std::vector<int> group_ids;
  std::vector<int> max_group_sizes;
  std::vector<std::vector<uint32>> member_ids;
  std::vector<std::vector<bool>> member_statuses;
  RETURN_IF_ERROR(bf_sde_interface_->GetActionProfileGroups(
      device_, session, bfrt_act_sel_table_id, action_profile_group.group_id(),
      &group_ids, &max_group_sizes, &member_ids, &member_statuses));

  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < group_ids.size(); ++i) {
    const int group_id = group_ids[i];
    const int max_group_size = max_group_sizes[i];
    const std::vector<uint32>& members = member_ids[i];
    const std::vector<bool>& member_status = member_statuses[i];
    ::p4::v1::ActionProfileGroup result;
    // Action profile id
    ASSIGN_OR_RETURN(
        auto action_profile_id,
        bf_sde_interface_->GetActionProfileBfRtId(bfrt_act_sel_table_id));
    ASSIGN_OR_RETURN(auto p4_action_profile_id,
                     bf_sde_interface_->GetP4InfoId(action_profile_id));
    result.set_action_profile_id(p4_action_profile_id);
    // Group id
    result.set_group_id(group_id);
    // Maximum group size
    result.set_max_size(max_group_size);
    // Members
    for (const auto& member_id : members) {
      auto* member = result.add_members();
      member->set_member_id(member_id);
      member->set_weight(1);
    }
    *resp.add_entities()->mutable_action_profile_group() = result;
  }

  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream channel failed.";
  }

  return ::util::OkStatus();
}

::util::Status BfrtTableManager::RegisterDigestListWriter(
    const std::shared_ptr<WriterInterface<::p4::v1::DigestList>>& writer) {
  absl::WriterMutexLock l(&digest_list_writer_lock_);
  digest_list_writer_ = writer;
  return ::util::OkStatus();
}

::util::Status BfrtTableManager::UnregisterDigestListWriter() {
  absl::WriterMutexLock l(&digest_list_writer_lock_);
  digest_list_writer_ = nullptr;
  return ::util::OkStatus();
}

void* BfrtTableManager::DigestListThreadFunc(void* arg) {
  BfrtTableManager* mgr = reinterpret_cast<BfrtTableManager*>(arg);
  ::util::Status status = mgr->HandleDigestList();
  if (!status.ok()) {
    LOG(ERROR) << "Non-OK exit of handler thread for digest lists.";
  }

  return nullptr;
}

::util::Status BfrtTableManager::HandleDigestList() {
  std::unique_ptr<ChannelReader<BfSdeInterface::DigestList>> reader;
  {
    absl::ReaderMutexLock l(&lock_);
    if (!digest_rx_thread_id_)
      return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized.";
    reader = ChannelReader<BfSdeInterface::DigestList>::Create(
        digest_list_receive_channel_);
    if (!reader) return MAKE_ERROR(ERR_INTERNAL) << "Failed to create reader.";
  }

  while (true) {
    {
      absl::ReaderMutexLock l(&chassis_lock);
      if (shutdown) break;
    }
    BfSdeInterface::DigestList digest_list;
    int code =
        reader->Read(&digest_list, absl::InfiniteDuration()).error_code();
    if (code == ERR_CANCELLED) break;
    if (code == ERR_ENTRY_NOT_FOUND) {
      LOG(ERROR) << "Read with infinite timeout failed with ENTRY_NOT_FOUND.";
      continue;
    }

    auto p4rt_digest_list = BuildP4DigestList(digest_list);
    if (!p4rt_digest_list.ok()) {
      LOG(ERROR) << "BuildP4DigestList failed: " << p4rt_digest_list.status();
      continue;
    }
    // TODO(max): perform P4RT metadata translation
    // const auto& translated_packet_in =
    //     bfrt_p4runtime_translator_->TranslatePacketIn(packet_in);
    // if (!translated_packet_in.ok()) {
    //   LOG(ERROR) << "TranslatePacketIn failed: " << status;
    //   continue;
    // }
    VLOG(1) << "Handled DigestList: "
            << p4rt_digest_list.ValueOrDie().ShortDebugString();
    {
      absl::WriterMutexLock l(&digest_list_writer_lock_);
      digest_list_writer_->Write(p4rt_digest_list.ConsumeValueOrDie());
    }
  }

  return ::util::OkStatus();
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
