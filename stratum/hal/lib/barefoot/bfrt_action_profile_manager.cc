// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_action_profile_manager.h"

#include <vector>

#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/macros.h"

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
      return WriteActionProfileMember(bfrt_session, bfrt_table_id, type,
                                      act_prof_member);
      break;
    }
    case kTnaExternActionSelectorId: {
      ::p4::v1::ActionProfileGroup act_prof_group;
      CHECK_RETURN_IF_FALSE(entry.entry().UnpackTo(&act_prof_group))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile group.";
      return WriteActionProfileGroup(bfrt_session, bfrt_table_id, type,
                                     act_prof_group);
      break;
    }
    default:
      RETURN_ERROR() << "Unsupported extern type " << entry.extern_type_id()
                     << ".";
  }
  return ::util::OkStatus();
}

::util::StatusOr<::p4::v1::ExternEntry>
BfrtActionProfileManager::ReadActionProfileEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::ExternEntry& entry) {
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
      ASSIGN_OR_RETURN(act_prof_member,
                       ReadActionProfileMember(bfrt_session, bfrt_table_id,
                                               act_prof_member));
      result.mutable_entry()->PackFrom(act_prof_member);
      break;
    }
    case kTnaExternActionSelectorId: {
      ::p4::v1::ActionProfileGroup act_prof_group;
      CHECK_RETURN_IF_FALSE(entry.entry().UnpackTo(&act_prof_group))
          << "Entry " << entry.ShortDebugString()
          << " is not an action profile group";
      ASSIGN_OR_RETURN(
          act_prof_group,
          ReadActionProfileGroup(bfrt_session, bfrt_table_id, act_prof_group));
      result.mutable_entry()->PackFrom(act_prof_group);
      break;
    }
    default:
      RETURN_ERROR() << "Unsupported extern type " << entry.extern_type_id()
                     << ".";
  }
  return result;
}

::util::Status BfrtActionProfileManager::WriteActionProfileMember(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::ActionProfileMember& action_profile_member) {
  absl::WriterMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      bf_rt_id_t bfrt_table_id,
      bfrt_id_mapper_->GetBfRtId(action_profile_member.action_profile_id()));
  return WriteActionProfileMember(bfrt_session, bfrt_table_id, type,
                                  action_profile_member);
}

::util::StatusOr<::p4::v1::ActionProfileMember>
BfrtActionProfileManager::ReadActionProfileMember(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::ActionProfileMember& action_profile_member) {
  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      bf_rt_id_t bfrt_table_id,
      bfrt_id_mapper_->GetBfRtId(action_profile_member.action_profile_id()));
  return ReadActionProfileMember(bfrt_session, bfrt_table_id,
                                 action_profile_member);
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
  return WriteActionProfileGroup(bfrt_session, bfrt_act_sel_table_id, type,
                                 action_profile_group);
}

::util::StatusOr<::p4::v1::ActionProfileGroup>
BfrtActionProfileManager::ReadActionProfileGroup(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::ActionProfileGroup& action_profile_group) {
  absl::ReaderMutexLock l(&lock_);
  ASSIGN_OR_RETURN(
      bf_rt_id_t bfrt_act_prof_table_id,
      bfrt_id_mapper_->GetBfRtId(action_profile_group.action_profile_id()));
  ASSIGN_OR_RETURN(
      bf_rt_id_t bfrt_act_sel_table_id,
      bfrt_id_mapper_->GetActionSelectorBfRtId(bfrt_act_prof_table_id));
  return ReadActionProfileGroup(bfrt_session, bfrt_act_sel_table_id,
                                action_profile_group);
}

::util::Status BfrtActionProfileManager::WriteActionProfileMember(
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
  ASSIGN_OR_RETURN(auto bf_dev_tgt,
                   bfrt_id_mapper_->GetDeviceTarget(bfrt_table_id));

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
BfrtActionProfileManager::ReadActionProfileMember(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
    const ::p4::v1::ActionProfileMember& action_profile_member) {
  // Lock is already acquired by the caller
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(bfrt_table_id, &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_ERROR(BuildTableKey(table, action_profile_member, table_key.get()));
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  ASSIGN_OR_RETURN(auto bf_dev_tgt,
                   bfrt_id_mapper_->GetDeviceTarget(bfrt_table_id));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));

  // Build result
  ::p4::v1::ActionProfileMember result = action_profile_member;
  bf_rt_id_t action_id;
  RETURN_IF_BFRT_ERROR(table_data->actionIdGet(&action_id));
  result.mutable_action()->set_action_id(action_id);
  std::vector<bf_rt_id_t> field_id_list;
  RETURN_IF_BFRT_ERROR(table->dataFieldIdListGet(action_id, &field_id_list));
  for (auto field_id : field_id_list) {
    size_t field_size;
    RETURN_IF_BFRT_ERROR(
        table->dataFieldSizeGet(field_id, action_id, &field_size));
    // "field_size" describes how many "bits" is this field, need to convert
    // to bytes with padding.
    field_size = (field_size + 7) / 8;
    uint8 field_data[field_size];
    table_data->getValue(field_id, field_size, field_data);
    const void* param_val = reinterpret_cast<const void*>(field_data);

    auto* param = result.mutable_action()->add_params();
    param->set_param_id(field_id);
    param->set_value(param_val, field_size);
  }
  return result;
}

::util::Status BfrtActionProfileManager::WriteActionProfileGroup(
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
  ASSIGN_OR_RETURN(auto bf_dev_tgt,
                   bfrt_id_mapper_->GetDeviceTarget(bfrt_table_id));

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

::util::StatusOr<::p4::v1::ActionProfileGroup>
BfrtActionProfileManager::ReadActionProfileGroup(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
    const ::p4::v1::ActionProfileGroup& action_profile_group) {
  // Lock is already acquired by the caller
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(bfrt_table_id, &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_ERROR(BuildTableKey(table, action_profile_group, table_key.get()));
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  ASSIGN_OR_RETURN(auto bf_dev_tgt,
                   bfrt_id_mapper_->GetDeviceTarget(bfrt_table_id));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));
  // Build result
  ::p4::v1::ActionProfileGroup result = action_profile_group;

  // Max size
  bf_rt_id_t max_group_size_field_id;
  RETURN_IF_BFRT_ERROR(
      table->dataFieldIdGet("$MAX_GROUP_SIZE", &max_group_size_field_id));
  uint64_t max_size;
  RETURN_IF_BFRT_ERROR(
      table_data->getValue(max_group_size_field_id, &max_size));
  result.set_max_size(static_cast<int32>(max_size));

  // Members
  bf_rt_id_t action_member_arr_id;
  RETURN_IF_BFRT_ERROR(
      table->dataFieldIdGet("$ACTION_MEMBER_ID", &action_member_arr_id));
  std::vector<bf_rt_id_t> members;
  RETURN_IF_BFRT_ERROR(table_data->getValue(action_member_arr_id, &members));
  for (bf_rt_id_t member_id : members) {
    auto member = result.add_members();
    member->set_member_id(member_id);
    // TODO(Yi): Add weight support.
    member->set_weight(1);
  }

  return result;
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
  const ::p4::v1::Action action = action_profile_member.action();
  RETURN_IF_BFRT_ERROR(table->dataReset(action.action_id(), table_data));
  for (auto param : action.params()) {
    const size_t size = param.value().size();
    const uint8* val = reinterpret_cast<const uint8*>(param.value().c_str());
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
  for (auto member : action_profile_group.members()) {
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
