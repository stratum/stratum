// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Target-agnostic SDE wrapper Table Entry methods.

#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"

#include <algorithm>
#include <cstddef>
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
#include "stratum/hal/lib/tdi/tdi_sde_helpers.h"

namespace stratum {
namespace hal {
namespace tdi {

using namespace stratum::hal::tdi::helpers;

::util::Status TdiSdeWrapper::InsertTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, const TableKeyInterface* table_key,
    const TableDataInterface* table_data) {

  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_key = dynamic_cast<const TableKey*>(table_key);
  CHECK_RETURN_IF_FALSE(real_table_key);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  CHECK_RETURN_IF_FALSE(real_table_data);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"), ", ",
        DumpTableKey(real_table_key->table_key_.get())
            .ValueOr("<error parsing key>"),
        ", ",
        DumpTableData(real_table_data->table_data_.get())
            .ValueOr("<error parsing data>"));
  };

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  /* Note: When multiple pipeline support is added, for device target
   * pipeline id also should be set
   */

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryAdd(
      *real_session->tdi_session_, *dev_tgt, *flags, *real_table_key->table_key_,
      *real_table_data->table_data_))
      << "Could not add table entry with: " << dump_args();

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::ModifyTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, const TableKeyInterface* table_key,
    const TableDataInterface* table_data) {

  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_key = dynamic_cast<const TableKey*>(table_key);
  CHECK_RETURN_IF_FALSE(real_table_key);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  CHECK_RETURN_IF_FALSE(real_table_data);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"), ", ",
        DumpTableKey(real_table_key->table_key_.get())
            .ValueOr("<error parsing key>"),
        ", ",
        DumpTableData(real_table_data->table_data_.get())
            .ValueOr("<error parsing data>"));
  };

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryMod(
      *real_session->tdi_session_, *dev_tgt, *flags, *real_table_key->table_key_,
      *real_table_data->table_data_))
      << "Could not modify table entry with: " << dump_args();
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::DeleteTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, const TableKeyInterface* table_key) {

  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_key = dynamic_cast<const TableKey*>(table_key);
  CHECK_RETURN_IF_FALSE(real_table_key);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  auto dump_args = [&]() -> std::string {
    return absl::StrCat(
        DumpTableMetadata(table).ValueOr("<error reading table>"), ", ",
        DumpTableKey(real_table_key->table_key_.get())
            .ValueOr("<error parsing key>"));
  };

  // TDI comments; Hardcoding device = 0
  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryDel(
      *real_session->tdi_session_, *dev_tgt, *flags, *real_table_key->table_key_))
      << "Could not delete table entry with: " << dump_args();
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, const TableKeyInterface* table_key,
    TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_key = dynamic_cast<const TableKey*>(table_key);
  CHECK_RETURN_IF_FALSE(real_table_key);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  CHECK_RETURN_IF_FALSE(real_table_data);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryGet(
      *real_session->tdi_session_, *dev_tgt, *flags, *real_table_key->table_key_,
      real_table_data->table_data_.get()));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetAllTableEntries(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id,
    std::vector<std::unique_ptr<TableKeyInterface>>* table_keys,
    std::vector<std::unique_ptr<TableDataInterface>>* table_values) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;
  RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                &keys, &datums));
  table_keys->resize(0);
  table_values->resize(0);

  for (size_t i = 0; i < keys.size(); ++i) {
    auto tk = absl::make_unique<TableKey>(std::move(keys[i]));
    auto td = absl::make_unique<TableData>(std::move(datums[i]));
    table_keys->push_back(std::move(tk));
    table_values->push_back(std::move(td));
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::SetDefaultTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, const TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  CHECK_RETURN_IF_FALSE(real_table_data);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->defaultEntrySet(
      *real_session->tdi_session_, *dev_tgt,
      *flags, *real_table_data->table_data_));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::ResetDefaultTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(
      table->defaultEntryReset(*real_session->tdi_session_, *dev_tgt, *flags));

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetDefaultTableEntry(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, TableDataInterface* table_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);
  auto real_table_data = dynamic_cast<const TableData*>(table_data);
  CHECK_RETURN_IF_FALSE(real_table_data);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->defaultEntryGet(
      *real_session->tdi_session_, *dev_tgt,
      *flags,
      real_table_data->table_data_.get()));

  return ::util::OkStatus();
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
