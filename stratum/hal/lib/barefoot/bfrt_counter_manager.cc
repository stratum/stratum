// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
#include "stratum/hal/lib/barefoot/bfrt_counter_manager.h"

#include "absl/synchronization/notification.h"
#include "bf_rt/bf_rt_table_operations.hpp"
#include "gflags/gflags.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/macros.h"

namespace stratum {
namespace hal {
namespace barefoot {

::util::Status BfrtCounterManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config, const bfrt::BfRtInfo* bfrt_info) {
  absl::WriterMutexLock l(&lock_);
  bfrt_info_ = bfrt_info;
  return ::util::OkStatus();
}

::util::Status BfrtCounterManager::WriteIndirectCounterEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::CounterEntry& counter_entry) {
  absl::WriterMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(type == ::p4::v1::Update::MODIFY)
      << "Update.Type must be MODIFY";
  CHECK_RETURN_IF_FALSE(counter_entry.has_index())
      << "Modify indirect counter without counter index is not supported now.";
  CHECK_RETURN_IF_FALSE(counter_entry.index().index() >= 0)
      << "Counter index must be greater than or equal to zero.";

  if(!counter_entry.has_data()) {
    // Nothing to be modified
    return ::util::OkStatus();
  }

  // Find counter table
  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(counter_entry.counter_id()));
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  ASSIGN_OR_RETURN(auto bf_dev_tgt, bfrt_id_mapper_->GetDeviceTarget(table_id));

  // Counter key: $COUNTER_INDEX
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  bf_rt_id_t field_id;
  RETURN_IF_BFRT_ERROR(table->keyFieldIdGet("$COUNTER_INDEX", &field_id));
  RETURN_IF_BFRT_ERROR(table_key->setValue(
      field_id, static_cast<uint64>(counter_entry.index().index())));

  // Counter data: $COUNTER_SPEC_BYTES, $COUNTER_SPEC_PKTS
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  auto bf_status = table->dataFieldIdGet("$COUNTER_SPEC_BYTES", &field_id);
  if (bf_status == BF_SUCCESS) {
    RETURN_IF_BFRT_ERROR(table_data->setValue(
        field_id, static_cast<uint64>(counter_entry.data().byte_count())));
  }
  bf_status = table->dataFieldIdGet("$COUNTER_SPEC_PKTS", &field_id);
  if (bf_status == BF_SUCCESS) {
    RETURN_IF_BFRT_ERROR(table_data->setValue(
        field_id, static_cast<uint64>(counter_entry.data().packet_count())));
  }
  RETURN_IF_BFRT_ERROR(
      table->tableEntryMod(*bfrt_session, bf_dev_tgt, *table_key, *table_data));
  return ::util::OkStatus();
}

::util::StatusOr<::p4::v1::CounterEntry>
BfrtCounterManager::ReadIndirectCounterEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::CounterEntry& counter_entry) {
  absl::ReaderMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(counter_entry.counter_id() != 0)
      << "Query indirect counter without counter id is not supported now.";
  CHECK_RETURN_IF_FALSE(counter_entry.has_index())
      << "Query indirect counter without counter index is not supported now.";
  CHECK_RETURN_IF_FALSE(counter_entry.index().index() >= 0)
      << "Counter index must be greater than or equal to zero.";
  ::p4::v1::CounterEntry result = counter_entry;

  // Find counter table
  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(counter_entry.counter_id()));
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  ASSIGN_OR_RETURN(auto bf_dev_tgt, bfrt_id_mapper_->GetDeviceTarget(table_id));

  // Sync table counter
  std::set<bfrt::TableOperationsType> supported_ops;
  RETURN_IF_BFRT_ERROR(table->tableOperationsSupported(&supported_ops));
  if (supported_ops.count(bfrt::TableOperationsType::COUNTER_SYNC)) {
    absl::Notification sync_notifier;
    std::unique_ptr<bfrt::BfRtTableOperations> table_op;
    RETURN_IF_BFRT_ERROR(table->operationsAllocate(
        bfrt::TableOperationsType::COUNTER_SYNC, &table_op));
    RETURN_IF_BFRT_ERROR(table_op->counterSyncSet(
        *bfrt_session, bf_dev_tgt,
        [table_id, &sync_notifier](const bf_rt_target_t& dev_tgt,
                                   void* cookie) {
          VLOG(1) << "Table counter for table " << table_id << " synced.";
          sync_notifier.Notify();
        },
        nullptr));
    RETURN_IF_BFRT_ERROR(table->tableOperationsExecute(*table_op.get()));

    // Wait until sync done or timeout.
    CHECK_RETURN_IF_FALSE(
        sync_notifier.WaitForNotificationWithTimeout(kDefaultSyncTimeout))
        << "Unable to sync table counter for table " << table_id
        << ", timeout.";
  }

  // Counter key: $COUNTER_INDEX
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  bf_rt_id_t field_id;
  RETURN_IF_BFRT_ERROR(table->keyFieldIdGet("$COUNTER_INDEX", &field_id));
  RETURN_IF_BFRT_ERROR(table_key->setValue(
      field_id, static_cast<uint64>(counter_entry.index().index())));

  // Read the counter data
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));

  // Counter data: $COUNTER_SPEC_BYTES, $COUNTER_SPEC_PKTS
  uint64 counter_data;
  auto bf_status = table->dataFieldIdGet("$COUNTER_SPEC_BYTES", &field_id);
  if (bf_status == BF_SUCCESS) {
    RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &counter_data));
    result.mutable_data()->set_byte_count(counter_data);
  }

  bf_status = table->dataFieldIdGet("$COUNTER_SPEC_PKTS", &field_id);
  if (bf_status == BF_SUCCESS) {
    RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &counter_data));
    result.mutable_data()->set_packet_count(counter_data);
  }

  return result;
}

// Creates a table manager instance.
std::unique_ptr<BfrtCounterManager> BfrtCounterManager::CreateInstance(
    const BfrtIdMapper* bfrt_id_mapper) {
  return absl::WrapUnique(new BfrtCounterManager(bfrt_id_mapper));
}

BfrtCounterManager::BfrtCounterManager(const BfrtIdMapper* bfrt_id_mapper)
    : bfrt_id_mapper_(ABSL_DIE_IF_NULL(bfrt_id_mapper)) {}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
