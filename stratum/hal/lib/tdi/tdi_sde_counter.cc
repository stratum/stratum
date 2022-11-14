// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Target-agnostic SDE wrapper for Counter methods.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <set>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/tdi/macros.h"
#include "stratum/hal/lib/tdi/tdi_constants.h"
#include "stratum/hal/lib/tdi/tdi_sde_common.h"
#include "stratum/hal/lib/tdi/tdi_sde_helpers.h"
#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace tdi {

using namespace stratum::hal::tdi::helpers;

::util::Status TdiSdeWrapper::WriteIndirectCounter(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 counter_id, int counter_index, absl::optional<uint64> byte_count,
    absl::optional<uint64> packet_count) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  const ::tdi::DataFieldInfo* dataFieldInfo;
  RET_CHECK(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(counter_id, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));

  // Counter key: $COUNTER_INDEX
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kCounterIndex, counter_index));

  // Counter data: $COUNTER_SPEC_BYTES
  if (byte_count.has_value()) {
    tdi_id_t field_id;
    dataFieldInfo = table->tableInfoGet()->dataFieldGet(kCounterBytes);
    RET_CHECK(dataFieldInfo);
    field_id = dataFieldInfo->idGet();
    RETURN_IF_TDI_ERROR(table_data->setValue(field_id, byte_count.value()));
  }
  // Counter data: $COUNTER_SPEC_PKTS
  if (packet_count.has_value()) {
    tdi_id_t field_id;
    dataFieldInfo = table->tableInfoGet()->dataFieldGet(kCounterPackets);
    RET_CHECK(dataFieldInfo);
    field_id = dataFieldInfo->idGet();
    RETURN_IF_TDI_ERROR(table_data->setValue(field_id, packet_count.value()));
  }
  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  if (byte_count.value() == 0 && packet_count.value() == 0) {
    LOG(INFO) << "Resetting counters";
    RETURN_IF_TDI_ERROR(
        table->clear(*real_session->tdi_session_, *dev_tgt, *flags));
  } else {
    RETURN_IF_TDI_ERROR(table->entryMod(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::ReadIndirectCounter(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 counter_id, absl::optional<uint32> counter_index,
    std::vector<uint32>* counter_indices,
    std::vector<absl::optional<uint64>>* byte_counts,
    std::vector<absl::optional<uint64>>* packet_counts,
    absl::Duration timeout) {
  RET_CHECK(counter_indices);
  RET_CHECK(byte_counts);
  RET_CHECK(packet_counts);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device* device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags* flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromIdGet(counter_id, &table));
  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;

  RETURN_IF_ERROR(DoSynchronizeCounters(dev_id, session, counter_id, timeout));

  // Is this a wildcard read?
  if (counter_index) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(&datums[0]));

    // Key: $COUNTER_INDEX
    RETURN_IF_ERROR(
        SetFieldExact(keys[0].get(), kCounterIndex, counter_index.value()));
    RETURN_IF_TDI_ERROR(table->entryGet(*real_session->tdi_session_, *dev_tgt,
                                        *flags, *keys[0], datums[0].get()));

  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt, table,
                                  &keys, &datums));
  }

  counter_indices->resize(0);
  byte_counts->resize(0);
  packet_counts->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
    const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
    // Key: $COUNTER_INDEX
    uint32_t tdi_counter_index = 0;
    RETURN_IF_ERROR(
        GetFieldExact(*table_key, kCounterIndex, &tdi_counter_index));
    counter_indices->push_back(tdi_counter_index);

    absl::optional<uint64> byte_count;
    absl::optional<uint64> packet_count;
    // Counter data: $COUNTER_SPEC_BYTES
    tdi_id_t field_id;

    if (table->tableInfoGet()->dataFieldGet(kCounterBytes)) {
      field_id = table->tableInfoGet()->dataFieldGet(kCounterBytes)->idGet();
      uint64 counter_bytes;
      RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &counter_bytes));
      byte_count = counter_bytes;
    }
    byte_counts->push_back(byte_count);

    // Counter data: $COUNTER_SPEC_PKTS
    if (table->tableInfoGet()->dataFieldGet(kCounterPackets)) {
      field_id = table->tableInfoGet()->dataFieldGet(kCounterPackets)->idGet();
      uint64 counter_pkts;
      RETURN_IF_TDI_ERROR(table_data->getValue(field_id, &counter_pkts));
      packet_count = counter_pkts;
    }
    packet_counts->push_back(packet_count);
  }

  CHECK_EQ(counter_indices->size(), keys.size());
  CHECK_EQ(byte_counts->size(), keys.size());
  CHECK_EQ(packet_counts->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::SynchronizeCounters(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 table_id, absl::Duration timeout) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return DoSynchronizeCounters(dev_id, session, table_id, timeout);
}

::util::Status TdiSdeWrapper::DoSynchronizeCounters(
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

  // Sync table counter
  std::set<tdi_operations_type_e> supported_ops;
  supported_ops = table->tableInfoGet()->operationsSupported();
  // TODO TDI comments : Uncomment this after SDE exposes counterSyncSet
#if 0
  if (supported_ops.count(static_cast<tdi_operations_type_e>(
      tdi_rt_operations_type_e::COUNTER_SYNC))) {
    auto sync_notifier = std::make_shared<absl::Notification>();
    std::weak_ptr<absl::Notification> weak_ref(sync_notifier);
    std::unique_ptr<::tdi::TableOperations> table_op;
    RETURN_IF_TDI_ERROR(table->operationsAllocate(
        static_cast<tdi_operations_type_e>(
          tdi_rt_operations_type_e::COUNTER_SYNC), &table_op));
    RETURN_IF_TDI_ERROR(table_op->counterSyncSet(
        *real_session->tdi_session_, dev_tgt,
        [table_id, weak_ref](const ::tdi::Target& dev_tgt, void* cookie) {
          if (auto notifier = weak_ref.lock()) {
            VLOG(1) << "Table counter for table " << table_id << " synced.";
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
             << "Timeout while syncing (indirect) table counters of table "
             << table_id << ".";
    }
  }
#endif
  return ::util::OkStatus();
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
