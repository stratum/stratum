// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_PRE_MANAGER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_PRE_MANAGER_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_set.h"
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

namespace stratum {
namespace hal {
namespace barefoot {

using PreEntry = ::p4::v1::PacketReplicationEngineEntry;

class BfrtPreManager {
 public:
  // Pushes a ForwardingPipelineConfig.
  ::util::Status PushForwardingPipelineConfig(const BfrtDeviceConfig& config,
                                              const bfrt::BfRtInfo* bfrt_info)
      LOCKS_EXCLUDED(lock_);

  // Writes a PRE entry.
  ::util::Status WritePreEntry(std::shared_ptr<bfrt::BfRtSession> bfrt_session,
                               const ::p4::v1::Update::Type& type,
                               const PreEntry& entry) LOCKS_EXCLUDED(lock_);

  // Reads a PRE entry.
  ::util::Status ReadPreEntry(std::shared_ptr<bfrt::BfRtSession> bfrt_session,
                              const PreEntry& entry,
                              WriterInterface<::p4::v1::ReadResponse>* writer)
      LOCKS_EXCLUDED(lock_);

  static std::unique_ptr<BfrtPreManager> CreateInstance(
      const BfrtIdMapper* bfrt_id_mapper);

 private:
  // Private constructor, we can create the instance by using `CreateInstance`
  // function only.
  explicit BfrtPreManager(const BfrtIdMapper* bfrt_id_mapper);

  // Insert/Modify/Delete a multicast group entry.
  // This function creates one or more multicast nodes based on replicas in
  // the entry and associate them to a multicast group.
  ::util::Status WriteMulticastGroupEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type& type,
      const ::p4::v1::MulticastGroupEntry& entry)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Insert/Modify/Delete a clone session entry.
  ::util::Status WriteCloneSessionEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type& type,
      const ::p4::v1::CloneSessionEntry& entry) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Reads a multicast group entry.
  ::util::Status ReadMulticastGroupEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::MulticastGroupEntry& entry,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  // Reads a clone session entry.
  ::util::Status ReadCloneSessionEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::CloneSessionEntry& entry,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  // Inserts/Modifies a multicast group ($pre.mgid table).
  ::util::Status WriteMulticastGroup(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type& type, uint32 group_id,
      std::vector<uint32> mc_node_ids) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Delete an existing multicast group.
  ::util::Status DeleteMulticastGroup(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, uint32 group_id)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Insert new multicast nodes of a given multicast group.
  ::util::StatusOr<std::vector<uint32>> InsertMulticastNodes(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::MulticastGroupEntry& entry)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Delete the given multicast nodes.
  ::util::Status DeleteMulticastNodes(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const std::vector<uint32>& mc_node_ids) SHARED_LOCKS_REQUIRED(lock_);

  // Finds and returns a free multicast node id.
  ::util::StatusOr<uint32> GetFreeMulticastNodeId(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session)
      SHARED_LOCKS_REQUIRED(lock_);

  // Get all multicast nodes from a given multicast group.
  ::util::StatusOr<std::vector<uint32>> GetNodesInMulticastGroup(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, uint32 group_id)
      SHARED_LOCKS_REQUIRED(lock_);

  // Gets all egress ports from a multicast node.
  ::util::StatusOr<std::vector<::p4::v1::Replica>> GetReplicasFromMcNode(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, uint64 mc_node_id)
      SHARED_LOCKS_REQUIRED(lock_);

  ::util::Status DumpHwState(std::shared_ptr<bfrt::BfRtSession> bfrt_session)
      SHARED_LOCKS_REQUIRED(lock_);

  // Reader-writer lock used to protect access to pipeline state.
  mutable absl::Mutex lock_;

  // The BfRt info, requires by some function to get runtime
  // instances like tables.
  const bfrt::BfRtInfo* bfrt_info_ GUARDED_BY(lock_);

  // The ID mapper that maps P4Runtime ID to BfRt ones (vice versa).
  // Not owned by this class
  const BfrtIdMapper* bfrt_id_mapper_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_PRE_MANAGER_H_
