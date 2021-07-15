// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_counter_manager.h"

#include <set>
#include <vector>

#include "absl/synchronization/notification.h"
#include "gflags/gflags.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"

DECLARE_uint32(bfrt_table_sync_timeout_ms);

namespace stratum {
namespace hal {
namespace barefoot {

std::unique_ptr<BfrtCounterManager> BfrtCounterManager::CreateInstance(
    BfSdeInterface* bf_sde_interface, int device) {
  return absl::WrapUnique(new BfrtCounterManager(bf_sde_interface, device));
}

BfrtCounterManager::BfrtCounterManager(BfSdeInterface* bf_sde_interface,
                                       int device)
    : bf_sde_interface_(ABSL_DIE_IF_NULL(bf_sde_interface)), device_(device) {}

::util::Status BfrtCounterManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config) {
  absl::WriterMutexLock l(&lock_);
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
  ASSIGN_OR_RETURN(uint32 table_id,
                   bf_sde_interface_->GetBfRtId(counter_entry.counter_id()));

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

::util::Status BfrtCounterManager::ReadIndirectCounterEntry(
    std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const ::p4::v1::CounterEntry& counter_entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  absl::ReaderMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(counter_entry.counter_id() != 0)
      << "Querying an indirect counter without counter id is not supported.";
  CHECK_RETURN_IF_FALSE(counter_entry.index().index() >= 0)
      << "Counter index must be greater than or equal to zero.";

  // Index 0 is a valid value and not a wildcard.
  absl::optional<uint32> optional_counter_index;
  if (counter_entry.has_index()) {
    optional_counter_index = counter_entry.index().index();
  }

  // Find counter table
  // TODO(max): revisit id translation location
  ASSIGN_OR_RETURN(uint32 table_id,
                   bf_sde_interface_->GetBfRtId(counter_entry.counter_id()));

  std::vector<uint32> counter_indices;
  std::vector<absl::optional<uint64>> byte_counts;
  std::vector<absl::optional<uint64>> packet_counts;
  RETURN_IF_ERROR(bf_sde_interface_->ReadIndirectCounter(
      device_, session, table_id, optional_counter_index, &counter_indices,
      &byte_counts, &packet_counts,
      absl::Milliseconds(FLAGS_bfrt_table_sync_timeout_ms)));

  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < counter_indices.size(); ++i) {
    const uint32 counter_index = counter_indices[i];
    const absl::optional<uint64>& byte_count = byte_counts[i];
    const absl::optional<uint64>& packet_count = packet_counts[i];
    ::p4::v1::CounterEntry result = counter_entry;

    result.mutable_index()->set_index(counter_index);
    if (byte_count) {
      result.mutable_data()->set_byte_count(byte_count.value());
    }
    if (packet_count) {
      result.mutable_data()->set_packet_count(packet_count.value());
    }

    *resp.add_entities()->mutable_counter_entry() = result;
  }

  VLOG(1) << "ReadIndirectCounterEntry resp " << resp.DebugString();
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
