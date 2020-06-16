// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_PRE_MANAGER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_PRE_MANAGER_H_

#include <memory>

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
#include "stratum/hal/lib/barefoot/bfrt_id_mapper.h"
#include "stratum/hal/lib/common/common.pb.h"

namespace stratum {
namespace hal {
namespace barefoot {

using PreEntry = ::p4::v1::PacketReplicationEngineEntry;

class BfRtPreManager {
 public:
  // Pushes the pipline info.
  ::util::Status PushPipelineInfo(const p4::config::v1::P4Info& p4info,
                                  const bfrt::BfRtInfo* bfrt_info)
      LOCKS_EXCLUDED(lock_);

  // Writes a PRE entry.
  ::util::Status WritePreEntry(std::shared_ptr<bfrt::BfRtSession> bfrt_session,
                               const ::p4::v1::Update::Type& type,
                               const PreEntry& entry) LOCKS_EXCLUDED(lock_);

  // Reads a PRE entry
  ::util::StatusOr<PreEntry> ReadPreEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, const PreEntry& entry)
      LOCKS_EXCLUDED(lock_);

  static std::unique_ptr<BfRtPreManager> CreateInstance(
      int unit, const BfRtIdMapper* bfrt_id_mapper);

 private:
  // Private constructure, we can create the instance by using `CreateInstance`
  // function only.
  BfRtPreManager(int unit, const BfRtIdMapper* bfrt_id_mapper);

  // Insert/Modify/Delete a multicast group entry.
  // This function creates one or more multicast nodes based on replicas in
  // the entry and associate them to on multicast group.
  ::util::Status WriteMulticastGroupEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type& type, ::p4::v1::MulticastGroupEntry entry)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Reads a multicast group entry.
  ::util::StatusOr<::p4::v1::MulticastGroupEntry> ReadMulticastGroupEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      ::p4::v1::MulticastGroupEntry entry) SHARED_LOCKS_REQUIRED(lock_);

  // Insert/Modify/Delete a multicast node ($pre.node table).
  ::util::Status WriteMulticastNodes(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type& type, ::p4::v1::MulticastGroupEntry entry)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Insert/Modify/Delete a multicast node ($pre.mgid table).
  ::util::Status WriteMulticastGroup(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type& type, ::p4::v1::MulticastGroupEntry entry)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Gets all egress ports from a multicast node.
  ::util::StatusOr<std::vector<uint32>> GetEgressPortsFromMcNode(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, uint64 mc_node_id)
      SHARED_LOCKS_REQUIRED(lock_);

  // The BfRt info, requires by some function to get runtime
  // instances like tables.
  const bfrt::BfRtInfo* bfrt_info_ GUARDED_BY(lock_);

  // Reader-writer lock used to protect access to pipeline state.
  mutable absl::Mutex lock_;

  // The ID mapper that maps P4Runtime ID to BfRt ones (vice versa).
  // Not owned by this class
  const BfRtIdMapper* bfrt_id_mapper_;

  // The unit number, which represent the device ID in SDK level.
  const int unit_;

  // A map which stores multicast group and nodes that already installe to the
  // device.
  absl::flat_hash_map<uint64, absl::flat_hash_set<uint32>> mcast_nodes_installed
      GUARDED_BY(lock_);
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif
