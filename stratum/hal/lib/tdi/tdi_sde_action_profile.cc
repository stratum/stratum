// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Vendor-agnostic SDE wrapper for Action Profile methods.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/tdi/macros.h"
#include "stratum/hal/lib/tdi/tdi_constants.h"
#include "stratum/hal/lib/tdi/tdi_sde_helpers.h"
#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace tdi {

using namespace stratum::hal::tdi::helpers;

::util::Status TdiSdeWrapper::WriteActionProfileMember(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, const TableDataInterface* table_data,
    bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  RET_CHECK(real_table_data);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));

  // DumpTableMetadata(table);
  // DumpTableData(real_table_data->table_data_.get());
  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"),
        ", member_id: ", member_id, ", ",
        DumpTableKey(table_key.get()).ValueOr("<error parsing key>"), ", ",
        DumpTableData(real_table_data->table_data_.get())
            .ValueOr("<error parsing data>"));
  };
  // Key: $ACTION_MEMBER_ID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kActionMemberId, member_id));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  if (insert) {
    RETURN_IF_TDI_ERROR(table->entryAdd(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key,
                                        *real_table_data->table_data_))
        << "Could not add action profile member with: " << dump_args();
  } else {
    RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key,
                                        *real_table_data->table_data_))
        << "Could not modify action profile member with: " << dump_args();
  }
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::InsertActionProfileMember(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileMember(dev_id, session, table_id, member_id,
                                  table_data, true);
}

::util::Status TdiSdeWrapper::ModifyActionProfileMember(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileMember(dev_id, session, table_id, member_id,
                                  table_data, false);
}

::util::Status TdiSdeWrapper::DeleteActionProfileMember(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));

  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"),
        ", member_id: ", member_id, ", ",
        DumpTableKey(table_key.get()).ValueOr("<error parsing key>"));
  };

  // Key: $ACTION_MEMBER_ID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kActionMemberId, member_id));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryDel(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *table_key))
      << "Could not delete action profile member with: " << dump_args();
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetActionProfileMembers(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, std::vector<int>* member_ids,
    std::vector<std::unique_ptr<TableDataInterface>>* table_values) {
  RET_CHECK(member_ids);
  RET_CHECK(table_values);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));
  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;
  // Is this a wildcard read?
  if (member_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(&datums[0]));
    // Key: $ACTION_MEMBER_ID
    RETURN_IF_ERROR(SetFieldExact(keys[0].get(), kActionMemberId, member_id));
    RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *keys[0], datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
  }

  member_ids->resize(0);
  table_values->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    // Key: $sid
    uint32_t member_id = 0;
    RETURN_IF_ERROR(GetFieldExact(*keys[i], kActionMemberId, &member_id));
    member_ids->push_back(member_id);

    // Data: action params
    auto td = absl::make_unique<TableData>(std::move(datums[i]));
    table_values->push_back(std::move(td));
  }

  CHECK_EQ(member_ids->size(), keys.size());
  CHECK_EQ(table_values->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::WriteActionProfileGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, int max_group_size,
    const std::vector<uint32>& member_ids,
    const std::vector<bool>& member_status, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));

  // We have to capture the std::unique_ptrs by reference [&] here.
  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"),
        ", group_id: ", group_id, ", max_group_size: ", max_group_size,
        ", members: ", PrintVector(member_ids, ","), ", ",
        DumpTableKey(table_key.get()).ValueOr("<error parsing key>"), ", ",
        DumpTableData(table_data.get()).ValueOr("<error parsing data>"));
  };

  // Key: $SELECTOR_GROUP_ID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kSelectorGroupId, group_id));
  // Data: $ACTION_MEMBER_ID
  RETURN_IF_ERROR(SetField(table_data.get(), kActionMemberId, member_ids));
  // Data: $ACTION_MEMBER_STATUS
  RETURN_IF_ERROR(
      SetField(table_data.get(), kActionMemberStatus, member_status));
  // Data: $MAX_GROUP_SIZE
  RETURN_IF_ERROR(
      SetField(table_data.get(), "$MAX_GROUP_SIZE", max_group_size));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  if (insert) {
    RETURN_IF_TDI_ERROR(table->entryAdd(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data))
        << "Could not add action profile group with: " << dump_args();
  } else {
    RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data))
        << "Could not modify action profile group with: " << dump_args();
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::InsertActionProfileGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, int max_group_size,
    const std::vector<uint32>& member_ids,
    const std::vector<bool>& member_status) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileGroup(dev_id, session, table_id, group_id,
                                 max_group_size, member_ids, member_status,
                                 true);
}

::util::Status TdiSdeWrapper::ModifyActionProfileGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, int max_group_size,
    const std::vector<uint32>& member_ids,
    const std::vector<bool>& member_status) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileGroup(dev_id, session, table_id, group_id,
                                 max_group_size, member_ids, member_status,
                                 false);
}

::util::Status TdiSdeWrapper::DeleteActionProfileGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));
  std::unique_ptr<::tdi::TableKey> table_key;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));

  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"),
        ", group_id: ", group_id,
        DumpTableKey(table_key.get()).ValueOr("<error parsing key>"));
  };

  // Key: $SELECTOR_GROUP_ID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kSelectorGroupId, group_id));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryDel(*real_session->tdi_session_, *dev_tgt,
                                      *flags, *table_key))
      << "Could not delete action profile group with: " << dump_args();

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetActionProfileGroups(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, std::vector<int>* group_ids,
    std::vector<int>* max_group_sizes,
    std::vector<std::vector<uint32>>* member_ids,
    std::vector<std::vector<bool>>* member_status) {
  RET_CHECK(group_ids);
  RET_CHECK(max_group_sizes);
  RET_CHECK(member_ids);
  RET_CHECK(member_status);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));
  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;
  // Is this a wildcard read?
  if (group_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(&datums[0]));
    // Key: $SELECTOR_GROUP_ID
    RETURN_IF_ERROR(SetFieldExact(keys[0].get(), kSelectorGroupId, group_id));
    RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *keys[0], datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
  }

  group_ids->resize(0);
  max_group_sizes->resize(0);
  member_ids->resize(0);
  member_status->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
    const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
    // Key: $SELECTOR_GROUP_ID
    uint32_t group_id = 0;
    RETURN_IF_ERROR(GetFieldExact(*table_key, kSelectorGroupId, &group_id));
    group_ids->push_back(group_id);

    // Data: $MAX_GROUP_SIZE
    uint64 max_group_size;
    RETURN_IF_ERROR(GetField(*table_data, "$MAX_GROUP_SIZE", &max_group_size));
    max_group_sizes->push_back(max_group_size);

    // Data: $ACTION_MEMBER_ID
    std::vector<uint32> members;
    RETURN_IF_ERROR(GetField(*table_data, kActionMemberId, &members));
    member_ids->push_back(members);

    // Data: $ACTION_MEMBER_STATUS
    std::vector<bool> member_enabled;
    RETURN_IF_ERROR(
        GetField(*table_data, kActionMemberStatus, &member_enabled));
    member_status->push_back(member_enabled);
  }

  CHECK_EQ(group_ids->size(), keys.size());
  CHECK_EQ(max_group_sizes->size(), keys.size());
  CHECK_EQ(member_ids->size(), keys.size());
  CHECK_EQ(member_status->size(), keys.size());

  return ::util::OkStatus();
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
