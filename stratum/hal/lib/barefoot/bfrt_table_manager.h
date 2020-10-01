// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_TABLE_MANAGER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_TABLE_MANAGER_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "bf_rt/bf_rt_init.hpp"
#include "bf_rt/bf_rt_session.hpp"
#include "bf_rt/bf_rt_table_key.hpp"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bfrt.pb.h"
#include "stratum/hal/lib/barefoot/bfrt_id_mapper.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/lib/timer_daemon.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtTableManager {
 public:
  // Pushes the pipline info.
  ::util::Status PushForwardingPipelineConfig(const BfrtDeviceConfig& config,
                                              const bfrt::BfRtInfo* bfrt_info)
      LOCKS_EXCLUDED(lock_);

  // Verifies a P4-based forwarding pipeline configuration intended for this
  // manager.
  ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config) const
      LOCKS_EXCLUDED(lock_);

  // Writes a table entry.
  ::util::Status WriteTableEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::TableEntry& table_entry) LOCKS_EXCLUDED(lock_);

  // Reads the P4 TableEntry(s) matched by the given table entry.
  ::util::Status ReadTableEntry(std::shared_ptr<bfrt::BfRtSession> bfrt_session,
                                const ::p4::v1::TableEntry& table_entry,
                                WriterInterface<::p4::v1::ReadResponse>* writer)
      LOCKS_EXCLUDED(lock_);

  // Modify the counter data of a table entry.
  ::util::Status WriteDirectCounterEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::DirectCounterEntry& direct_counter_entry)
      LOCKS_EXCLUDED(lock_);

  // Modify the data of a register entry.
  ::util::Status WriteRegisterEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::RegisterEntry& register_entry) LOCKS_EXCLUDED(lock_);

  // Read the counter data of a table entry.
  ::util::StatusOr<::p4::v1::DirectCounterEntry> ReadDirectCounterEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::DirectCounterEntry& direct_counter_entry)
      LOCKS_EXCLUDED(lock_);

  // Read the data of a register entry.
  ::util::Status ReadRegisterEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::RegisterEntry& register_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Creates a table manager instance.
  static std::unique_ptr<BfrtTableManager> CreateInstance(
      OperationMode mode, const BfrtIdMapper* bfrt_id_mapper);

 private:
  // Private constructor, we can create the instance by using `CreateInstance`
  // function only.
  explicit BfrtTableManager(OperationMode mode,
                            const BfrtIdMapper* bfrt_id_mapper);

  ::util::Status BuildTableKey(const ::p4::v1::TableEntry& table_entry,
                               bfrt::BfRtTableKey* table_key,
                               const bfrt::BfRtTable* table);

  ::util::Status BuildTableActionData(const ::p4::v1::Action& action,
                                      const bfrt::BfRtTable* table,
                                      bfrt::BfRtTableData* table_data);

  ::util::Status BuildTableActionProfileMemberData(
      const uint32 action_profile_member_id, const bfrt::BfRtTable* table,
      bfrt::BfRtTableData* table_data);

  ::util::Status BuildTableActionProfileGroupData(
      const uint32 action_profile_group_id, const bfrt::BfRtTable* table,
      bfrt::BfRtTableData* table_data);

  ::util::Status BuildDirectCounterEntryData(
      const ::p4::v1::DirectCounterEntry& entry, const bfrt::BfRtTable* table,
      bfrt::BfRtTableData* table_data);

  ::util::Status BuildTableData(const ::p4::v1::TableEntry& table_entry,
                                const bfrt::BfRtTable* table,
                                bfrt::BfRtTableData* table_data);

  ::util::StatusOr<std::vector<uint32>> GetP4TableIds();

  ::util::Status SyncTableCounters(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::TableEntry& table_entry) LOCKS_EXCLUDED(lock_);

  ::util::Status SyncTableRegisters(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t table_id)
      LOCKS_EXCLUDED(lock_);

  ::util::Status ReadSingleTableEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::TableEntry& table_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer);

  ::util::Status ReadDefaultTableEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::TableEntry& table_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer);

  ::util::Status ReadAllTableEntries(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::TableEntry& table_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer);

  // Construct a P4RT table entry from a table entry request, table key and
  // table data.
  ::util::StatusOr<::p4::v1::TableEntry> BuildP4TableEntry(
      const ::p4::v1::TableEntry& request, const bfrt::BfRtTable* table,
      const bfrt::BfRtTableKey& table_key,
      const bfrt::BfRtTableData& table_data);

  // Determines the mode of operation:
  // - OPERATION_MODE_STANDALONE: when Stratum stack runs independently and
  // therefore needs to do all the SDK initialization itself.
  // - OPERATION_MODE_COUPLED: when Stratum stack runs as part of Sandcastle
  // stack, coupled with the rest of stack processes.
  // - OPERATION_MODE_SIM: when Stratum stack runs in simulation mode.
  // Note that this variable is set upon initialization and is never changed
  // afterwards.
  OperationMode mode_;

  // Reader-writer lock used to protect access to pipeline state.
  mutable absl::Mutex lock_;

  std::vector<TimerDaemon::DescriptorPtr> register_timer_descriptors_
      GUARDED_BY(lock_);

  // The BfRt info, requires by some function to get runtime
  // instances like tables.
  const bfrt::BfRtInfo* bfrt_info_ GUARDED_BY(lock_);

  // Helper class to validate the P4Info and requests against it.
  // TODO(max): Maybe this manager should be created in the node and passed down
  // to all feature managers.
  std::unique_ptr<P4InfoManager> p4_info_manager_ GUARDED_BY(lock_);

  // The ID mapper that maps P4Runtime ID to BfRt ones (vice versa).
  // Not owned by this class
  const BfrtIdMapper* bfrt_id_mapper_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_TABLE_MANAGER_H_
