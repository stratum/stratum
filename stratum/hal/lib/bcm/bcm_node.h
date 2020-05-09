// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_BCM_BCM_NODE_H_
#define STRATUM_HAL_LIB_BCM_BCM_NODE_H_

#include <memory>
#include <vector>

#include "stratum/hal/lib/bcm/bcm_acl_manager.h"
#include "stratum/hal/lib/bcm/bcm_global_vars.h"
#include "stratum/hal/lib/bcm/bcm_l2_manager.h"
#include "stratum/hal/lib/bcm/bcm_l3_manager.h"
#include "stratum/hal/lib/bcm/bcm_packetio_manager.h"
#include "stratum/hal/lib/bcm/bcm_table_manager.h"
#include "stratum/hal/lib/bcm/bcm_tunnel_manager.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/p4/p4_table_mapper.h"
#include "stratum/glue/integral_types.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace bcm {

// The BcmNode class encapsulates all per BCM node/chip/ASIC functionalities,
// primarily the flow managers. Calls made to this class are processed and
// passed through to the appropriate managers.
class BcmNode {
 public:
  virtual ~BcmNode();

  // Configures per-node managers handled by this BcmNode instance based on the
  // given ChassisConfig and sets the P4 node_id for this node. This does not
  // handle forwarding pipeline configuration.
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config,
                                           uint64 node_id)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(lock_);

  // Verifies the given ChassisConfig proto for all node-specific managers.
  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(lock_);

  // Configures the P4-based forwarding pipeline configuration for this node.
  virtual ::util::Status PushForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(lock_);

  // Verifies a P4-based forwarding pipeline configuration intended for this
  // node.
  virtual ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(lock_);

  // Performs the shutdown sequence in coldboot mode for per-node managers
  // handled by this BcmNode instance.
  virtual ::util::Status Shutdown() LOCKS_EXCLUDED(chassis_lock, lock_);

  // Performs NSF freeze. This includes the warmboot shutdown sequence and
  // saving of checkpoint data to local storage.
  virtual ::util::Status Freeze() SHARED_LOCKS_REQUIRED(chassis_lock)
      LOCKS_EXCLUDED(lock_);

  // Performs NSF unfreeze. This includes initialization of per-node managers
  // handled by this class and restoration of checkpointed data from Freeze().
  virtual ::util::Status Unfreeze() SHARED_LOCKS_REQUIRED(chassis_lock)
      LOCKS_EXCLUDED(lock_);

  // Writes P4-based forwarding entries (table entries, action profile members,
  // action profile groups, meters, counters) to this node.
  virtual ::util::Status WriteForwardingEntries(
      const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(lock_);

  // Reads P4-based forwarding entries (table entries, action profile members,
  // action profile groups, meters, counters) from this node.
  virtual ::util::Status ReadForwardingEntries(
      const ::p4::v1::ReadRequest& req,
      WriterInterface<::p4::v1::ReadResponse>* writer,
      std::vector<::util::Status>* details) SHARED_LOCKS_REQUIRED(chassis_lock)
      LOCKS_EXCLUDED(lock_);

  // Registers a writer to be invoked on receipt of a packet on any port on this
  // node. The sent P4 PacketIn instance includes all the info on where
  // the packet was received on this node as well as its payload.
  virtual ::util::Status RegisterPacketReceiveWriter(
      const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(lock_);

  // Unregisters writer registered in RegisterPacketReceiveWriter().
  virtual ::util::Status UnregisterPacketReceiveWriter()
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(lock_);

  // Transmits a packet received from controller directly to a port on this node
  // or to the ingress pipeline of the node to let the chip route the packet.
  // The given P4 PacketOut instance includes all the info on where to
  // transmit the packet as well as its payload.
  virtual ::util::Status TransmitPacket(const ::p4::v1::PacketOut& packet)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(lock_);

  // Updates any managers which rely on current port state. This is generally
  // invoked by BcmChassisManager in the linkscan event handler.
  virtual ::util::Status UpdatePortState(uint32 port_id)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(lock_);

  // Factory function for creating a BcmNode instance.
  static std::unique_ptr<BcmNode> CreateInstance(
      BcmAclManager* bcm_acl_manager, BcmL2Manager* bcm_l2_manager,
      BcmL3Manager* bcm_l3_manager, BcmPacketioManager* bcm_packetio_manager,
      BcmTableManager* bcm_table_manager, BcmTunnelManager* bcm_tunnel_manager,
      P4TableMapper* p4_table_mapper, int unit);

  // BcmNode is neither copyable nor movable.
  BcmNode(const BcmNode&) = delete;
  BcmNode& operator=(const BcmNode&) = delete;

 protected:
  // Default constructor. To be used by Mock class only.
  BcmNode();

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BcmNode(BcmAclManager* bcm_acl_manager, BcmL2Manager* bcm_l2_manager,
          BcmL3Manager* bcm_l3_manager,
          BcmPacketioManager* bcm_packetio_manager,
          BcmTableManager* bcm_table_manager,
          BcmTunnelManager* bcm_tunnel_manager, P4TableMapper* p4_table_mapper,
          int unit);

  // Writes static entries from config to the affected tables. The post_push
  // flag distinguishes entries that need to be handled after the pipeline
  // config change is fully in effect from those that must be changed prior to
  // pushing config.
  ::util::Status StaticEntryWrite(const P4PipelineConfig& config,
                                  bool post_push)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Non-locking internal version of WriteForwardingEntries().
  virtual ::util::Status DoWriteForwardingEntries(
      const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Write a single P4 TableEntry.
  ::util::Status TableWrite(const ::p4::v1::TableEntry& entry,
                            ::p4::v1::Update::Type type);

  // Write a single P4 ActionProfileMember.
  ::util::Status ActionProfileMemberWrite(
      const ::p4::v1::ActionProfileMember& member, ::p4::v1::Update::Type type);

  // Write a single P4 ActionProfileGroup.
  ::util::Status ActionProfileGroupWrite(
      const ::p4::v1::ActionProfileGroup& group, ::p4::v1::Update::Type type);

  // Write a single P4 PacketReplicationEngineEntry.
  ::util::Status PacketReplicationEngineEntryWrite(
      const ::p4::v1::PacketReplicationEngineEntry& entry,
      ::p4::v1::Update::Type type);

  // Reader-writer lock used to protect access to node-specific state.
  mutable absl::Mutex lock_;

  // Flag indicate whether chip is initialized.
  bool initialized_ GUARDED_BY(lock_);

  // Managers. Not owned by the class.
  BcmAclManager* bcm_acl_manager_;
  BcmL2Manager* bcm_l2_manager_;
  BcmL3Manager* bcm_l3_manager_;
  BcmPacketioManager* bcm_packetio_manager_;
  BcmTableManager* bcm_table_manager_;
  BcmTunnelManager* bcm_tunnel_manager_;

  // Pointer to P4TableMapper. The pointer may also be passed to a few
  // managers for parsing/deparsing P4 data.
  P4TableMapper* p4_table_mapper_;  // not owned by this class.

  // Logical node ID corresponding to the node/ASIC managed by this class
  // instance. Assigned on PushChassisConfig() and might change during the
  // lifetime of the class.
  uint64 node_id_ GUARDED_BY(lock_);

  // Fixed zero-based BCM unit number corresponding to the node/ASIC managed by
  // this class instance. Assigned in the class constructor.
  const int unit_;

  friend class BcmNodeTest;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_NODE_H_
