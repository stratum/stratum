// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/tdi/tdi_action_profile_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "stratum/hal/lib/tdi/tdi_constants.h"

namespace stratum {
namespace hal {
namespace tdi {

TdiActionProfileManager::TdiActionProfileManager(
    TdiSdeInterface* tdi_sde_interface, int device)
    : tdi_sde_interface_(ABSL_DIE_IF_NULL(tdi_sde_interface)),
      p4_info_manager_(nullptr),
      device_(device) {}

std::unique_ptr<TdiActionProfileManager>
TdiActionProfileManager::CreateInstance(TdiSdeInterface* tdi_sde_interface,
                                         int device) {
  return absl::WrapUnique(
      new TdiActionProfileManager(tdi_sde_interface, device));
}

::util::Status TdiActionProfileManager::PushForwardingPipelineConfig(
    const TdiDeviceConfig& config) {
  absl::WriterMutexLock l(&lock_);
  std::unique_ptr<P4InfoManager> p4_info_manager =
      absl::make_unique<P4InfoManager>(config.programs(0).p4info());
  RETURN_IF_ERROR(p4_info_manager->InitializeAndVerify());
  p4_info_manager_ = std::move(p4_info_manager);

  return ::util::OkStatus();
}

::util::Status TdiActionProfileManager::WriteActionProfileEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type, const ::p4::v1::ExternEntry& entry) {
  absl::WriterMutexLock l(&lock_);
  ASSIGN_OR_RETURN(uint32 bfrt_table_id,
                   tdi_sde_interface_->GetTdiRtId(entry.extern_id()));
  switch (entry.extern_type_id()) {
    case kTnaExternActionProfileId: {
      ::p4::v1::ActionProfileMember act_prof_member;
      RET_CHECK(entry.entry().UnpackTo(&act_prof_member))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile member.";
      return DoWriteActionProfileMember(session, bfrt_table_id, type,
                                        act_prof_member);
    }
    case kTnaExternActionSelectorId: {
      ::p4::v1::ActionProfileGroup act_prof_group;
      RET_CHECK(entry.entry().UnpackTo(&act_prof_group))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile group.";
      return DoWriteActionProfileGroup(session, bfrt_table_id, type,
                                       act_prof_group);
    }
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
          << "Unsupported extern type " << entry.extern_type_id() << ".";
  }
}

::util::Status TdiActionProfileManager::ReadActionProfileEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::ExternEntry& entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(uint32 bfrt_table_id,
                   tdi_sde_interface_->GetTdiRtId(entry.extern_id()));
  ::p4::v1::ExternEntry result = entry;
  switch (entry.extern_type_id()) {
    case kTnaExternActionProfileId: {
      ::p4::v1::ActionProfileMember act_prof_member;
      RET_CHECK(entry.entry().UnpackTo(&act_prof_member))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile member";
      RETURN_IF_ERROR(DoReadActionProfileMember(session, bfrt_table_id,
                                                act_prof_member, writer));
      break;
    }
    case kTnaExternActionSelectorId: {
      ::p4::v1::ActionProfileGroup act_prof_group;
      RET_CHECK(entry.entry().UnpackTo(&act_prof_group))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile group";
      RETURN_IF_ERROR(DoReadActionProfileGroup(session, bfrt_table_id,
                                               act_prof_group, writer));
      break;
    }
    default:
      return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
          << "Unsupported extern type " << entry.extern_type_id() << ".";
  }

  return ::util::OkStatus();
}

::util::Status TdiActionProfileManager::WriteActionProfileMember(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::ActionProfileMember& action_profile_member) {
  absl::WriterMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      uint32 bfrt_table_id,
      tdi_sde_interface_->GetTdiRtId(action_profile_member.action_profile_id()));
  return DoWriteActionProfileMember(session, bfrt_table_id, type,
                                    action_profile_member);
}

::util::Status TdiActionProfileManager::ReadActionProfileMember(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::ActionProfileMember& action_profile_member,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      uint32 bfrt_table_id,
      tdi_sde_interface_->GetTdiRtId(action_profile_member.action_profile_id()));
  return DoReadActionProfileMember(session, bfrt_table_id,
                                   action_profile_member, writer);
}

::util::Status TdiActionProfileManager::WriteActionProfileGroup(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::ActionProfileGroup& action_profile_group) {
  absl::WriterMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      uint32 bfrt_act_prof_table_id,
      tdi_sde_interface_->GetTdiRtId(action_profile_group.action_profile_id()));
  ASSIGN_OR_RETURN(
      uint32 bfrt_act_sel_table_id,
      tdi_sde_interface_->GetActionSelectorTdiRtId(bfrt_act_prof_table_id));
  return DoWriteActionProfileGroup(session, bfrt_act_sel_table_id, type,
                                   action_profile_group);
}

::util::Status TdiActionProfileManager::ReadActionProfileGroup(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::ActionProfileGroup& action_profile_group,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      uint32 bfrt_act_prof_table_id,
      tdi_sde_interface_->GetTdiRtId(action_profile_group.action_profile_id()));
  ASSIGN_OR_RETURN(
      uint32 bfrt_act_sel_table_id,
      tdi_sde_interface_->GetActionSelectorTdiRtId(bfrt_act_prof_table_id));
  return DoReadActionProfileGroup(session, bfrt_act_sel_table_id,
                                  action_profile_group, writer);
}

::util::Status TdiActionProfileManager::DoWriteActionProfileMember(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 bfrt_table_id, const ::p4::v1::Update::Type type,
    const ::p4::v1::ActionProfileMember& action_profile_member) {
  // Lock is already acquired by the caller
  RET_CHECK(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;

  // Action data
  ASSIGN_OR_RETURN(
      auto table_data,
      tdi_sde_interface_->CreateTableData(
          bfrt_table_id, action_profile_member.action().action_id()));
  for (const auto& param : action_profile_member.action().params()) {
    RETURN_IF_ERROR(table_data->SetParam(param.param_id(), param.value()));
  }

  switch (type) {
    case ::p4::v1::Update::INSERT: {
      RETURN_IF_ERROR(tdi_sde_interface_->InsertActionProfileMember(
          device_, session, bfrt_table_id, action_profile_member.member_id(),
          table_data.get()));
      break;
    }
    case ::p4::v1::Update::MODIFY: {
      RETURN_IF_ERROR(tdi_sde_interface_->ModifyActionProfileMember(
          device_, session, bfrt_table_id, action_profile_member.member_id(),
          table_data.get()));
      break;
    }
    case ::p4::v1::Update::DELETE: {
      RETURN_IF_ERROR(tdi_sde_interface_->DeleteActionProfileMember(
          device_, session, bfrt_table_id, action_profile_member.member_id()));
      break;
    }
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Unsupported update type: " << type;
  }

  return ::util::OkStatus();
}

::util::Status TdiActionProfileManager::DoReadActionProfileMember(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 bfrt_table_id,
    const ::p4::v1::ActionProfileMember& action_profile_member,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  RET_CHECK(action_profile_member.action_profile_id() != 0)
      << "Reading all action profiles is not supported yet.";

  std::vector<int> member_ids;
  std::vector<std::unique_ptr<TdiSdeInterface::TableDataInterface>> table_values;
  RETURN_IF_ERROR(tdi_sde_interface_->GetActionProfileMembers(
      device_, session, bfrt_table_id, action_profile_member.member_id(),
      &member_ids, &table_values));

  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < member_ids.size(); ++i) {
    const int member_id = member_ids[i];
    const std::unique_ptr<TdiSdeInterface::TableDataInterface>& table_data =
        table_values[i];

    ::p4::v1::ActionProfileMember result;
    ASSIGN_OR_RETURN(auto action_profile_id,
                     tdi_sde_interface_->GetP4InfoId(bfrt_table_id));
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

    *resp.add_entities()->mutable_action_profile_member() = result;
  }

  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream channel failed.";
  }

  return ::util::OkStatus();
}

::util::Status TdiActionProfileManager::DoWriteActionProfileGroup(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 bfrt_table_id, const ::p4::v1::Update::Type type,
    const ::p4::v1::ActionProfileGroup& action_profile_group) {
  RET_CHECK(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;

  std::vector<uint32> member_ids;
  std::vector<bool> member_status;
  for (const auto& member : action_profile_group.members()) {
    member_ids.push_back(member.member_id());
    member_status.push_back(true);  // Activate the member.
  }

  switch (type) {
    case ::p4::v1::Update::INSERT: {
      RETURN_IF_ERROR(tdi_sde_interface_->InsertActionProfileGroup(
          device_, session, bfrt_table_id, action_profile_group.group_id(),
          action_profile_group.max_size(), member_ids, member_status));
      break;
    }
    case ::p4::v1::Update::MODIFY: {
      RETURN_IF_ERROR(tdi_sde_interface_->ModifyActionProfileGroup(
          device_, session, bfrt_table_id, action_profile_group.group_id(),
          action_profile_group.max_size(), member_ids, member_status));
      break;
    }
    case ::p4::v1::Update::DELETE: {
      RETURN_IF_ERROR(tdi_sde_interface_->DeleteActionProfileGroup(
          device_, session, bfrt_table_id, action_profile_group.group_id()));
      break;
    }
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Unsupported update type: " << type;
  }

  return ::util::OkStatus();
}

::util::Status TdiActionProfileManager::DoReadActionProfileGroup(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 bfrt_table_id,
    const ::p4::v1::ActionProfileGroup& action_profile_group,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  RET_CHECK(action_profile_group.action_profile_id() != 0)
      << "Reading all action profiles is not supported yet.";

  std::vector<int> group_ids;
  std::vector<int> max_group_sizes;
  std::vector<std::vector<uint32>> member_ids;
  std::vector<std::vector<bool>> member_statuses;
  RETURN_IF_ERROR(tdi_sde_interface_->GetActionProfileGroups(
      device_, session, bfrt_table_id, action_profile_group.group_id(),
      &group_ids, &max_group_sizes, &member_ids, &member_statuses));

  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < group_ids.size(); ++i) {
    const int group_id = group_ids[i];
    const int max_group_size = max_group_sizes[i];
    const std::vector<uint32>& members = member_ids[i];
    const std::vector<bool>& member_status = member_statuses[i];
    ::p4::v1::ActionProfileGroup result;
    // Action profile id
    ASSIGN_OR_RETURN(auto action_profile_id,
                     tdi_sde_interface_->GetActionProfileTdiRtId(bfrt_table_id));
    ASSIGN_OR_RETURN(auto p4_action_profile_id,
                     tdi_sde_interface_->GetP4InfoId(action_profile_id));
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

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
