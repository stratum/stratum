// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_action_profile_manager.h"

#include <vector>

#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/macros.h"
#include "stratum/hal/lib/barefoot/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

::util::Status BfrtActionProfileManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config, const bfrt::BfRtInfo* bfrt_info) {
  absl::WriterMutexLock l(&lock_);
  bfrt_info_ = bfrt_info;
  return ::util::OkStatus();
}

::util::Status BfrtActionProfileManager::WriteActionProfileEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type, const ::p4::v1::ExternEntry& entry) {
  absl::WriterMutexLock l(&lock_);
  ASSIGN_OR_RETURN(bf_rt_id_t bfrt_table_id,
                   bfrt_id_mapper_->GetBfRtId(entry.extern_id()));
  switch (entry.extern_type_id()) {
    case kTnaExternActionProfileId: {
      ::p4::v1::ActionProfileMember act_prof_member;
      CHECK_RETURN_IF_FALSE(entry.entry().UnpackTo(&act_prof_member))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile member.";
      return DoWriteActionProfileMember(bfrt_session, bfrt_table_id, type,
                                        act_prof_member);
      break;
    }
    case kTnaExternActionSelectorId: {
      ::p4::v1::ActionProfileGroup act_prof_group;
      CHECK_RETURN_IF_FALSE(entry.entry().UnpackTo(&act_prof_group))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile group.";
      return DoWriteActionProfileGroup(bfrt_session, bfrt_table_id, type,
                                       act_prof_group);
      break;
    }
    default:
      RETURN_ERROR() << "Unsupported extern type " << entry.extern_type_id()
                     << ".";
  }
  return ::util::OkStatus();
}

::util::Status BfrtActionProfileManager::ReadActionProfileEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::ExternEntry& entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(bf_rt_id_t bfrt_table_id,
                   bfrt_id_mapper_->GetBfRtId(entry.extern_id()));
  ::p4::v1::ExternEntry result = entry;
  switch (entry.extern_type_id()) {
    case kTnaExternActionProfileId: {
      ::p4::v1::ActionProfileMember act_prof_member;
      CHECK_RETURN_IF_FALSE(entry.entry().UnpackTo(&act_prof_member))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile member";
      RETURN_IF_ERROR(DoReadActionProfileMember(bfrt_session, bfrt_table_id,
                                                act_prof_member, writer));
      break;
    }
    case kTnaExternActionSelectorId: {
      ::p4::v1::ActionProfileGroup act_prof_group;
      CHECK_RETURN_IF_FALSE(entry.entry().UnpackTo(&act_prof_group))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile group";
      RETURN_IF_ERROR(DoReadActionProfileGroup(bfrt_session, bfrt_table_id,
                                               act_prof_group, writer));
      break;
    }
    default:
      RETURN_ERROR(ERR_OPER_NOT_SUPPORTED)
          << "Unsupported extern type " << entry.extern_type_id() << ".";
  }

  return ::util::OkStatus();
}

::util::Status BfrtActionProfileManager::WriteActionProfileMember(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::ActionProfileMember& action_profile_member) {
  absl::WriterMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      bf_rt_id_t bfrt_table_id,
      bfrt_id_mapper_->GetBfRtId(action_profile_member.action_profile_id()));
  return DoWriteActionProfileMember(bfrt_session, bfrt_table_id, type,
                                    action_profile_member);
}

::util::Status BfrtActionProfileManager::ReadActionProfileMember(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::ActionProfileMember& action_profile_member,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      bf_rt_id_t bfrt_table_id,
      bfrt_id_mapper_->GetBfRtId(action_profile_member.action_profile_id()));
  return DoReadActionProfileMember(bfrt_session, bfrt_table_id,
                                   action_profile_member, writer);
}

::util::Status BfrtActionProfileManager::WriteActionProfileGroup(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::ActionProfileGroup& action_profile_group) {
  absl::WriterMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      bf_rt_id_t bfrt_act_prof_table_id,
      bfrt_id_mapper_->GetBfRtId(action_profile_group.action_profile_id()));
  ASSIGN_OR_RETURN(
      bf_rt_id_t bfrt_act_sel_table_id,
      bfrt_id_mapper_->GetActionSelectorBfRtId(bfrt_act_prof_table_id));
  return DoWriteActionProfileGroup(bfrt_session, bfrt_act_sel_table_id, type,
                                   action_profile_group);
}

::util::Status BfrtActionProfileManager::ReadActionProfileGroup(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::ActionProfileGroup& action_profile_group,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      bf_rt_id_t bfrt_act_prof_table_id,
      bfrt_id_mapper_->GetBfRtId(action_profile_group.action_profile_id()));
  ASSIGN_OR_RETURN(
      bf_rt_id_t bfrt_act_sel_table_id,
      bfrt_id_mapper_->GetActionSelectorBfRtId(bfrt_act_prof_table_id));
  return DoReadActionProfileGroup(bfrt_session, bfrt_act_sel_table_id,
                                  action_profile_group, writer);
}

::util::Status BfrtActionProfileManager::DoWriteActionProfileMember(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::ActionProfileMember& action_profile_member) {
  // Lock is already acquired by the caller
  CHECK_RETURN_IF_FALSE(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(bfrt_table_id, &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_ERROR(BuildTableKey(table, action_profile_member, table_key.get()));
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  if (type == ::p4::v1::Update::INSERT || type == ::p4::v1::Update::MODIFY) {
    RETURN_IF_ERROR(
        BuildTableData(table, action_profile_member, table_data.get()));
  }
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();

  switch (type) {
    case ::p4::v1::Update::INSERT:
      RETURN_IF_BFRT_ERROR(table->tableEntryAdd(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data));
      break;
    case ::p4::v1::Update::MODIFY:
      RETURN_IF_BFRT_ERROR(table->tableEntryMod(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data));
      break;
    case ::p4::v1::Update::DELETE:
      RETURN_IF_BFRT_ERROR(
          table->tableEntryDel(*bfrt_session, bf_dev_tgt, *table_key));
      break;
    default:
      RETURN_ERROR() << "Unsupported update type: " << type;
  }
  return ::util::OkStatus();
}

::util::StatusOr<::p4::v1::ActionProfileMember>
BfrtActionProfileManager::BuildP4ActionProfileMember(
    const bfrt::BfRtTable* table, const bfrt::BfRtTableKey& table_key,
    const bfrt::BfRtTableData& table_data) {
  ::p4::v1::ActionProfileMember result;
  // Action profile id
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));
  ASSIGN_OR_RETURN(auto action_profile_id,
                   bfrt_id_mapper_->GetP4InfoId(table_id));
  result.set_action_profile_id(action_profile_id);

  // Member id
  bf_rt_id_t action_member_field_id;
  uint64 member_id;
  RETURN_IF_BFRT_ERROR(
      table->keyFieldIdGet("$ACTION_MEMBER_ID", &action_member_field_id));
  RETURN_IF_BFRT_ERROR(table_key.getValue(action_member_field_id, &member_id));
  result.set_member_id(member_id);

  // Action
  bf_rt_id_t action_id;
  RETURN_IF_BFRT_ERROR(table_data.actionIdGet(&action_id));
  result.mutable_action()->set_action_id(action_id);
  std::vector<bf_rt_id_t> field_id_list;
  RETURN_IF_BFRT_ERROR(table->dataFieldIdListGet(action_id, &field_id_list));
  for (const auto& field_id : field_id_list) {
    size_t field_size;
    RETURN_IF_BFRT_ERROR(
        table->dataFieldSizeGet(field_id, action_id, &field_size));
    // "field_size" describes how many "bits" is this field, need to convert
    // to bytes with padding.
    field_size = (field_size + 7) / 8;
    uint8 field_data[field_size];
    RETURN_IF_BFRT_ERROR(table_data.getValue(field_id, field_size, field_data));
    const void* param_val = reinterpret_cast<const void*>(field_data);

    auto* param = result.mutable_action()->add_params();
    param->set_param_id(field_id);
    param->set_value(param_val, field_size);
  }
  VLOG(1) << "BuildP4ActionProfileMember " << result.ShortDebugString();

  return result;
}

::util::StatusOr<::p4::v1::ActionProfileGroup>
BfrtActionProfileManager::BuildP4ActionProfileGroup(
    const bfrt::BfRtTable* table, const bfrt::BfRtTableKey& table_key,
    const bfrt::BfRtTableData& table_data) {
  ::p4::v1::ActionProfileGroup result;
  // Action profile id
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));
  ASSIGN_OR_RETURN(auto action_profile_id,
                   bfrt_id_mapper_->GetActionProfileBfRtId(table_id));
  ASSIGN_OR_RETURN(auto p4_action_profile_id,
                   bfrt_id_mapper_->GetP4InfoId(action_profile_id));
  result.set_action_profile_id(p4_action_profile_id);

  // Group id
  uint64 group_id;
  bf_rt_id_t selector_group_field_id;
  RETURN_IF_BFRT_ERROR(
      table->keyFieldIdGet("$SELECTOR_GROUP_ID", &selector_group_field_id));
  RETURN_IF_BFRT_ERROR(table_key.getValue(selector_group_field_id, &group_id));
  result.set_group_id(group_id);

  // Maximum group size
  uint64 max_group_size;
  bf_rt_id_t max_group_size_field_id;
  RETURN_IF_BFRT_ERROR(
      table->dataFieldIdGet("$MAX_GROUP_SIZE", &max_group_size_field_id));
  RETURN_IF_BFRT_ERROR(
      table_data.getValue(max_group_size_field_id, &max_group_size));
  result.set_max_size(max_group_size);

  // Members
  std::vector<bf_rt_id_t> members;
  bf_rt_id_t action_member_list_id;
  RETURN_IF_BFRT_ERROR(
      table->dataFieldIdGet("$ACTION_MEMBER_ID", &action_member_list_id));
  RETURN_IF_BFRT_ERROR(table_data.getValue(action_member_list_id, &members));
  for (const auto& member_id : members) {
    auto* member = result.add_members();
    member->set_member_id(member_id);
    member->set_weight(1);
  }

  VLOG(1) << "BuildP4ActionProfileGroup " << result.ShortDebugString();

  return result;
}

::util::Status BfrtActionProfileManager::DoReadActionProfileMember(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
    const ::p4::v1::ActionProfileMember& action_profile_member,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  CHECK_RETURN_IF_FALSE(action_profile_member.action_profile_id() != 0)
      << "Reading all action profiles is not supported yet.";

  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(bfrt_table_id, &table));

  // Check if wildcard read for all members.
  if (action_profile_member.member_id() == 0) {
    ::p4::v1::ReadResponse resp;
    std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
    std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
    RETURN_IF_ERROR(
        GetAllEntries(bfrt_session, bf_dev_tgt, table, &keys, &datums));
    for (size_t i = 0; i < keys.size(); ++i) {
      const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
      const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
      ASSIGN_OR_RETURN(auto result, BuildP4ActionProfileMember(
                                        table, *table_key, *table_data));
      *resp.add_entities()->mutable_action_profile_member() = result;
    }

    if (!writer->Write(resp)) {
      return MAKE_ERROR(ERR_INTERNAL) << "Write to stream channel failed.";
    }

    return ::util::OkStatus();
  }

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_ERROR(BuildTableKey(table, action_profile_member, table_key.get()));
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()))
      << "Could not find action profile member "
      << action_profile_member.ShortDebugString();
  ASSIGN_OR_RETURN(auto result,
                   BuildP4ActionProfileMember(table, *table_key, *table_data));
  ::p4::v1::ReadResponse resp;
  auto* entity = resp.add_entities();
  *entity->mutable_action_profile_member() = result;

  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream channel failed.";
  }

  return ::util::OkStatus();
}

::util::Status BfrtActionProfileManager::DoWriteActionProfileGroup(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::ActionProfileGroup& action_profile_group) {
  // Lock is already acquired by the caller
  CHECK_RETURN_IF_FALSE(type != ::p4::v1::Update::UNSPECIFIED)
      << "Invalid update type " << type;
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(bfrt_table_id, &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_ERROR(BuildTableKey(table, action_profile_group, table_key.get()));
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  if (type == ::p4::v1::Update::INSERT || type == ::p4::v1::Update::MODIFY) {
    RETURN_IF_ERROR(
        BuildTableData(table, action_profile_group, table_data.get()));
  }
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();

  switch (type) {
    case ::p4::v1::Update::INSERT:
      RETURN_IF_BFRT_ERROR(table->tableEntryAdd(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data));
      break;
    case ::p4::v1::Update::MODIFY:
      RETURN_IF_BFRT_ERROR(table->tableEntryMod(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data));
      break;
    case ::p4::v1::Update::DELETE:
      RETURN_IF_BFRT_ERROR(
          table->tableEntryDel(*bfrt_session, bf_dev_tgt, *table_key));
      break;
    default:
      RETURN_ERROR() << "Unsupported update type: " << type;
  }
  return ::util::OkStatus();
}

::util::Status BfrtActionProfileManager::DoReadActionProfileGroup(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
    const ::p4::v1::ActionProfileGroup& action_profile_group,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  CHECK_RETURN_IF_FALSE(action_profile_group.action_profile_id() != 0)
      << "Reading all action profiles is not supported yet.";
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(bfrt_table_id, &table));
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();

  // Check if wildcard read for all groups.
  if (action_profile_group.group_id() == 0) {
    ::p4::v1::ReadResponse resp;
    std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
    std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
    RETURN_IF_ERROR(
        GetAllEntries(bfrt_session, bf_dev_tgt, table, &keys, &datums));
    for (size_t i = 0; i < keys.size(); ++i) {
      const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
      const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
      ASSIGN_OR_RETURN(auto result, BuildP4ActionProfileGroup(table, *table_key,
                                                              *table_data));
      *resp.add_entities()->mutable_action_profile_group() = result;
    }

    if (!writer->Write(resp)) {
      return MAKE_ERROR(ERR_INTERNAL) << "Write to stream channel failed.";
    }

    return ::util::OkStatus();
  }

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_ERROR(BuildTableKey(table, action_profile_group, table_key.get()));
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()))
      << "Could not find action profile group "
      << action_profile_group.ShortDebugString();

  ASSIGN_OR_RETURN(auto result,
                   BuildP4ActionProfileGroup(table, *table_key, *table_data));
  ::p4::v1::ReadResponse resp;
  auto* entity = resp.add_entities();
  *entity->mutable_action_profile_group() = result;

  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream channel failed.";
  }

  return ::util::OkStatus();
}

::util::Status BfrtActionProfileManager::BuildTableKey(
    const bfrt::BfRtTable* table,
    const ::p4::v1::ActionProfileMember& action_profile_member,
    bfrt::BfRtTableKey* table_key) {
  bf_rt_id_t action_member_field_id;
  RETURN_IF_BFRT_ERROR(
      table->keyFieldIdGet("$ACTION_MEMBER_ID", &action_member_field_id));
  table_key->setValue(action_member_field_id,
                      static_cast<uint64>(action_profile_member.member_id()));
  return ::util::OkStatus();
}

::util::Status BfrtActionProfileManager::BuildTableKey(
    const bfrt::BfRtTable* table,
    const ::p4::v1::ActionProfileGroup& action_profile_group,
    bfrt::BfRtTableKey* table_key) {
  bf_rt_id_t selector_group_field_id;
  RETURN_IF_BFRT_ERROR(
      table->keyFieldIdGet("$SELECTOR_GROUP_ID", &selector_group_field_id));
  table_key->setValue(selector_group_field_id,
                      static_cast<uint64>(action_profile_group.group_id()));
  return ::util::OkStatus();
}

::util::Status BfrtActionProfileManager::BuildTableData(
    const bfrt::BfRtTable* table,
    const ::p4::v1::ActionProfileMember& action_profile_member,
    bfrt::BfRtTableData* table_data) {
  const ::p4::v1::Action& action = action_profile_member.action();
  RETURN_IF_BFRT_ERROR(table->dataReset(action.action_id(), table_data));
  for (const auto& param : action.params()) {
    const size_t size = param.value().size();
    const uint8* val = reinterpret_cast<const uint8*>(param.value().data());
    RETURN_IF_BFRT_ERROR(table_data->setValue(param.param_id(), val, size));
  }
  return ::util::OkStatus();
}

::util::Status BfrtActionProfileManager::BuildTableData(
    const bfrt::BfRtTable* table,
    const ::p4::v1::ActionProfileGroup& action_profile_group,
    bfrt::BfRtTableData* table_data) {
  std::vector<bf_rt_id_t> members;
  std::vector<bool> member_status;
  for (const auto& member : action_profile_group.members()) {
    members.push_back(member.member_id());
    member_status.push_back(true);  // Activate the member.
  }

  bf_rt_id_t action_member_list_id;
  RETURN_IF_BFRT_ERROR(
      table->dataFieldIdGet("$ACTION_MEMBER_ID", &action_member_list_id));
  RETURN_IF_BFRT_ERROR(table_data->setValue(action_member_list_id, members));

  bf_rt_id_t action_member_status_list_id;
  RETURN_IF_BFRT_ERROR(table->dataFieldIdGet("$ACTION_MEMBER_STATUS",
                                             &action_member_status_list_id));
  RETURN_IF_BFRT_ERROR(
      table_data->setValue(action_member_status_list_id, member_status));

  bf_rt_id_t max_group_size_field_id;
  RETURN_IF_BFRT_ERROR(
      table->dataFieldIdGet("$MAX_GROUP_SIZE", &max_group_size_field_id));
  RETURN_IF_BFRT_ERROR(table_data->setValue(
      max_group_size_field_id,
      static_cast<uint64>(action_profile_group.max_size())));

  return ::util::OkStatus();
}

// Creates an action profile manager instance.
std::unique_ptr<BfrtActionProfileManager>
BfrtActionProfileManager::CreateInstance(const BfrtIdMapper* bfrt_id_mapper) {
  return absl::WrapUnique(new BfrtActionProfileManager(bfrt_id_mapper));
}

BfrtActionProfileManager::BfrtActionProfileManager(
    const BfrtIdMapper* bfrt_id_mapper)
    : bfrt_id_mapper_(ABSL_DIE_IF_NULL(bfrt_id_mapper)) {}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
