// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
#include "stratum/hal/lib/barefoot/bfrt_counter_manager.h"

#include "bf_rt/bf_rt_table_operations.hpp"
#include "gflags/gflags.h"
#include "stratum/hal/lib/barefoot/macros.h"

DEFINE_uint64(bfrt_counter_sync_interval_ms, 30000,
              "The interval of synchronizing indirect counters.");

namespace stratum {
namespace hal {
namespace barefoot {

::util::Status BfrtCounterManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config, const bfrt::BfRtInfo* bfrt_info) {
  absl::WriterMutexLock l(&lock_);
  bfrt_info_ = bfrt_info;

  // Start syncing counters
  RETURN_IF_ERROR(TimerDaemon::Start());
  RETURN_IF_ERROR(TimerDaemon::RequestPeriodicTimer(
      0, FLAGS_bfrt_counter_sync_interval_ms,
      [this]() { return this->SyncCounters(); }, &counter_sync_timer_));
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
      << "Query indirect counter without counter index is not supported now.";

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

::util::StatusOr<::p4::v1::CounterEntry> BfrtCounterManager::ReadIndirectCounterEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::CounterEntry& counter_entry) {
  absl::ReaderMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(counter_entry.has_index())
      << "Query indirect counter without counter index is not supported now.";
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
  if (supported_ops.find(bfrt::TableOperationsType::COUNTER_SYNC) !=
      supported_ops.end()) {
    std::unique_ptr<bfrt::BfRtTableOperations> table_op;
    RETURN_IF_BFRT_ERROR(table->operationsAllocate(
        bfrt::TableOperationsType::COUNTER_SYNC, &table_op));
    RETURN_IF_BFRT_ERROR(table_op->counterSyncSet(
        *bfrt_session, bf_dev_tgt,
        [table_id](const bf_rt_target_t& dev_tgt, void* cookie) {
          VLOG(1) << "Table counter for table " << table_id << " synced.";
        },
        nullptr));
    RETURN_IF_BFRT_ERROR(table->tableOperationsExecute(*table_op.get()));
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

::util::Status BfrtCounterManager::SyncCounters() {
  absl::ReaderMutexLock l(&lock_);
  LOG(ERROR) << "sync counters";
  std::vector<const bfrt::BfRtTable*> tables;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtInfoGetTables(&tables))
      << "Unable to get tables from Bfrt info.";

  // Here we are using a separate session to handle counter sync
  auto bfrt_session = bfrt::BfRtSession::sessionCreate();
  RETURN_IF_BFRT_ERROR(bfrt_session->beginBatch());
  for (const auto* table : tables) {
    std::string table_name;
    RETURN_IF_BFRT_ERROR(table->tableNameGet(&table_name));
    bf_rt_id_t table_id;
    RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));
    ASSIGN_OR_RETURN(auto bf_dev_tgt,
                     bfrt_id_mapper_->GetDeviceTarget(table_id));

    std::set<bfrt::TableOperationsType> supported_ops;
    RETURN_IF_BFRT_ERROR(table->tableOperationsSupported(&supported_ops))
        << "Unable to get ops that supported from table " << table_name;
    if (!supported_ops.count(bfrt::TableOperationsType::COUNTER_SYNC)) {
      // Skip tables that doesn't support counter sync operation.
      continue;
    }
    std::unique_ptr<bfrt::BfRtTableOperations> table_op;
    RETURN_IF_BFRT_ERROR(table->operationsAllocate(
        bfrt::TableOperationsType::COUNTER_SYNC, &table_op))
        << "Unable to allocate COUNTER_SYNC operation for table " << table_name;

    RETURN_IF_BFRT_ERROR(table_op->counterSyncSet(
        *bfrt_session, bf_dev_tgt,
        [table_name](const bf_rt_target_t& dev_tgt, void* cookie) {
          LOG(ERROR) << "Table counter for table " << table_name << " synced.";
        }, nullptr));
    RETURN_IF_BFRT_ERROR(table->tableOperationsExecute(*table_op.get()))
        << "Ubable to execute counter sync operation for table " << table_name;
  }
  RETURN_IF_BFRT_ERROR(bfrt_session->endBatch(false));
  RETURN_IF_BFRT_ERROR(bfrt_session->sessionDestroy());
  return ::util::OkStatus();
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
