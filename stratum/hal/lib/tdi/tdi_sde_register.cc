// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Target-agnostic SDE wrapper for Register methods.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/hal/lib/tdi/macros.h"
#include "stratum/hal/lib/tdi/tdi_constants.h"
#include "stratum/hal/lib/tdi/tdi_sde_common.h"
#include "stratum/hal/lib/tdi/tdi_sde_helpers.h"
#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"
#include "stratum/lib/macros.h"
#include "stratum/public/proto/error.pb.h"

namespace stratum {
namespace hal {
namespace tdi {

using namespace stratum::hal::tdi::helpers;

namespace {

// Helper function to get the field ID of the "f1" register data field.
// TODO(max): Maybe use table name and strip off "pipe." at the beginning?
// std::string table_name;
// RETURN_IF_TDI_ERROR(table->tableNameGet(&table_name));
// RETURN_IF_TDI_ERROR(
//     table->dataFieldIdGet(absl::StrCat(table_name, ".", "f1"), &field_id));

::util::StatusOr<tdi_id_t> GetRegisterDataFieldId(const ::tdi::Table* table) {
  std::vector<tdi_id_t> data_field_ids;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  data_field_ids = table->tableInfoGet()->dataFieldIdListGet();
  for (const auto& field_id : data_field_ids) {
    std::string field_name;
    dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_id);
    RET_CHECK(dataFieldInfo);
    field_name = dataFieldInfo->nameGet();
    if (absl::EndsWith(field_name, ".f1")) {
      return field_id;
    }
  }

  return MAKE_ERROR(ERR_INTERNAL) << "Could not find register data field id.";
}

}  // namespace

::util::Status TdiSdeWrapper::WriteRegister(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, absl::optional<uint32> register_index,
    const std::string& register_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));

  // Register data: <register_name>.f1
  // The current bf-p4c compiler emits the fully-qualified field name, including
  // parent table and pipeline. We cannot use just "f1" as the field name.
  tdi_id_t field_id;
  ASSIGN_OR_RETURN(field_id, GetRegisterDataFieldId(table));
  size_t data_field_size_bits;
  const ::tdi::DataFieldInfo* dataFieldInfo;
  dataFieldInfo = table->tableInfoGet()->dataFieldGet(field_id);
  RET_CHECK(dataFieldInfo);
  data_field_size_bits = dataFieldInfo->sizeGet();
  // The SDE expects a string with the full width.
  std::string value = P4RuntimeByteStringToPaddedByteString(
      register_data, data_field_size_bits);
  RETURN_IF_TDI_ERROR(table_data->setValue(
      field_id, reinterpret_cast<const uint8*>(value.data()), value.size()));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  if (register_index) {
    // Single index target.
    // Register key: $REGISTER_INDEX
    RETURN_IF_ERROR(
        SetFieldExact(table_key.get(), kRegisterIndex, register_index.value()));
    RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data));
  } else {
    // Wildcard write to all indices.
    size_t table_size;
    RETURN_IF_TDI_ERROR(table->sizeGet(*real_session->tdi_session_, *dev_tgt,
                                       *flags, &table_size));
    for (size_t i = 0; i < table_size; ++i) {
      // Register key: $REGISTER_INDEX
      RETURN_IF_ERROR(SetFieldExact(table_key.get(), kRegisterIndex, i));
      RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                          *flags, *table_key, *table_data));
    }
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::ReadRegisters(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, absl::optional<uint32> register_index,
    std::vector<uint32>* register_indices, std::vector<uint64>* register_values,
    absl::Duration timeout) {
  RET_CHECK(register_indices);
  RET_CHECK(register_values);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  RETURN_IF_ERROR(SynchronizeRegisters(dev_id, session, table_id, timeout));

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
  if (register_index) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(&datums[0]));

    // Key: $REGISTER_INDEX
    RETURN_IF_ERROR(
        SetFieldExact(keys[0].get(), kRegisterIndex, register_index.value()));
    RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *keys[0], datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
  }

  register_indices->resize(0);
  register_values->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
    const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
    // Key: $REGISTER_INDEX
    uint32_t tdi_register_index = 0;
    RETURN_IF_ERROR(
        GetFieldExact(*table_key, kRegisterIndex, &tdi_register_index));
    register_indices->push_back(tdi_register_index);
    // Data: <register_name>.f1
    ASSIGN_OR_RETURN(auto f1_field_id, GetRegisterDataFieldId(table));

    tdi_field_data_type_e data_type;
    const ::tdi::DataFieldInfo* dataFieldInfo;
    dataFieldInfo = table->tableInfoGet()->dataFieldGet(f1_field_id);
    RET_CHECK(dataFieldInfo);
    data_type = dataFieldInfo->dataTypeGet();
    switch (data_type) {
      case TDI_FIELD_DATA_TYPE_BYTE_STREAM: {
        // Even though the data type says byte stream, the SDE can only allows
        // fetching the data in an uint64 vector with one entry per pipe.
        std::vector<uint64> register_data;
        RETURN_IF_TDI_ERROR(table_data->getValue(f1_field_id, &register_data));
        RET_CHECK(register_data.size() > 0);
        register_values->push_back(register_data[0]);
        break;
      }
      default:
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Unsupported register data type "
               << static_cast<int>(data_type) << " for register in table "
               << table_id;
    }
  }

  CHECK_EQ(register_indices->size(), keys.size());
  CHECK_EQ(register_values->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::SynchronizeRegisters(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, absl::Duration timeout) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(table_id, &table));

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  // Sync table registers.
  // TDI comments ; its supposed to be tdi_rt_operations_type_e ??
  // const std::set<tdi_rt_operations_type_e> supported_ops;
  // supported_ops =
  // static_cast<tdi_rt_operations_type_e>(table->tableInfoGet()->operationsSupported());

  std::set<tdi_operations_type_e> supported_ops;
  supported_ops = table->tableInfoGet()->operationsSupported();
  // TODO TDI comments : Need to uncomment this after SDE exposes
  // registerSyncSet
#if 0
  if (supported_ops.count(static_cast<tdi_operations_type_e>(
          tdi_rt_operations_type_e::REGISTER_SYNC))) {
    auto sync_notifier = std::make_shared<absl::Notification>();
    std::weak_ptr<absl::Notification> weak_ref(sync_notifier);
    std::unique_ptr<::tdi::TableOperations> table_op;
    RETURN_IF_TDI_ERROR(
        table->operationsAllocate(static_cast<tdi_operations_type_e>(
                                      tdi_rt_operations_type_e::REGISTER_SYNC),
                                  &table_op));
    RETURN_IF_TDI_ERROR(table_op->registerSyncSet(
        *real_session->tdi_session_, dev_tgt,
        [table_id, weak_ref](const ::tdi::Target& dev_tgt, void* cookie) {
          if (auto notifier = weak_ref.lock()) {
            VLOG(1) << "Table registers for table " << table_id << " synced.";
            notifier->Notify();
          } else {
            VLOG(1) << "Notifier expired before table " << table_id
                    << " could be synced.";
          }
        },
        nullptr));
    RETURN_IF_TDI_ERROR(table->tableOperationsExecute(*table_op.get()));
    // Wait until sync done or timeout.
    if (!sync_notifier->WaitForNotificationWithTimeout(timeout)) {
      return MAKE_ERROR(ERR_OPER_TIMEOUT)
             << "Timeout while syncing (indirect) table registers of table "
             << table_id << ".";
    }
  }
#endif
  return ::util::OkStatus();
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
