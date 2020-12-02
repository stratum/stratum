// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_counter_manager.h"

#include <set>

#include "absl/synchronization/notification.h"
#include "bf_rt/bf_rt_table_operations.hpp"
#include "gflags/gflags.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"

DECLARE_uint32(bfrt_table_sync_timeout_ms);

namespace stratum {
namespace hal {
namespace barefoot {

std::unique_ptr<BfrtCounterManager> BfrtCounterManager::CreateInstance(
    const BfrtIdMapper* bfrt_id_mapper, BfSdeInterface* bf_sde_interface,
    int device) {
  return absl::WrapUnique(
      new BfrtCounterManager(bfrt_id_mapper, bf_sde_interface, device));
}

BfrtCounterManager::BfrtCounterManager(const BfrtIdMapper* bfrt_id_mapper,
                                       BfSdeInterface* bf_sde_interface,
                                       int device)
    : bfrt_id_mapper_(ABSL_DIE_IF_NULL(bfrt_id_mapper)),
      bf_sde_interface_(ABSL_DIE_IF_NULL(bf_sde_interface)),
      device_(device) {}

::util::Status BfrtCounterManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config, const bfrt::BfRtInfo* bfrt_info) {
  absl::WriterMutexLock l(&lock_);
  bfrt_info_ = bfrt_info;
  return ::util::OkStatus();
}

::util::Status BfrtCounterManager::WriteIndirectCounterEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::CounterEntry& counter_entry) {
  absl::WriterMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(type == ::p4::v1::Update::MODIFY)
      << "Update type of CounterEntry " << counter_entry.ShortDebugString()
      << " must be MODIFY.";
  CHECK_RETURN_IF_FALSE(counter_entry.has_index())
      << "Modifying an indirect counter without counter index is currently not "
         "supported.";
  CHECK_RETURN_IF_FALSE(counter_entry.index().index() >= 0)
      << "Counter index must be greater than or equal to zero.";

  // Find counter table.
  // TODO(max): revisit id translation location
  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(counter_entry.counter_id()));

  absl::optional<uint64> byte_count;
  absl::optional<uint64> packet_count;
  if (counter_entry.has_data()) {
    byte_count = counter_entry.data().byte_count();
    packet_count = counter_entry.data().packet_count();
  }
  RETURN_IF_ERROR(bf_sde_interface_->WriteIndirectCounter(
      device_, session, table_id, counter_entry.index().index(), byte_count,
      packet_count));

  return ::util::OkStatus();
}

::util::StatusOr<::p4::v1::CounterEntry>
BfrtCounterManager::ReadIndirectCounterEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::CounterEntry& counter_entry) {
  absl::ReaderMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(counter_entry.counter_id() != 0)
      << "Querying an indirect counter without counter id is not supported.";
  CHECK_RETURN_IF_FALSE(counter_entry.has_index())
      << "Querying an indirect counter without counter index is not supported.";
  CHECK_RETURN_IF_FALSE(counter_entry.index().index() >= 0)
      << "Counter index must be greater than or equal to zero.";

  // Find counter table
  // TODO(max): revisit id translation location
  ASSIGN_OR_RETURN(bf_rt_id_t table_id,
                   bfrt_id_mapper_->GetBfRtId(counter_entry.counter_id()));

  absl::optional<uint64> byte_count;
  absl::optional<uint64> packet_count;
  RETURN_IF_ERROR(bf_sde_interface_->ReadIndirectCounter(
      device_, session, table_id, counter_entry.index().index(), &byte_count,
      &packet_count, absl::Milliseconds(FLAGS_bfrt_table_sync_timeout_ms)));

  ::p4::v1::CounterEntry result = counter_entry;
  if (byte_count) {
    result.mutable_data()->set_byte_count(byte_count.value());
  }
  if (packet_count) {
    result.mutable_data()->set_packet_count(packet_count.value());
  }

  return result;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
