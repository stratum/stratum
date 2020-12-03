// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_NODE_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_NODE_H_

#include <memory>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "bf_rt/bf_rt_init.hpp"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/barefoot/bf.pb.h"
#include "stratum/hal/lib/barefoot/bfrt_action_profile_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_counter_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_id_mapper.h"
#include "stratum/hal/lib/barefoot/bfrt_packetio_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_pre_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"
#include "stratum/hal/lib/barefoot/macros.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"

#define _PI_UPDATE_MAX_NAME_SIZE 100
#define _PI_UPDATE_MAX_TMP_FILENAME_SIZE (_PI_UPDATE_MAX_NAME_SIZE + 32)

namespace stratum {
namespace hal {
namespace barefoot {

// The BfrtNode class encapsulates all per P4-native node/chip/ASIC
// functionalities, primarily the flow managers. Calls made to this class are
// processed and passed through to the BfRt API.
class BfrtNode final {
 public:
  ~BfrtNode();

  ::util::Status PushChassisConfig(const ChassisConfig& config, uint64 node_id)
      LOCKS_EXCLUDED(lock_);
  ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                     uint64 node_id) LOCKS_EXCLUDED(lock_);
  ::util::Status PushForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config);
  ::util::Status SaveForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config) LOCKS_EXCLUDED(lock_);
  ::util::Status CommitForwardingPipelineConfig() LOCKS_EXCLUDED(lock_);
  ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config) const;
  ::util::Status Shutdown();
  ::util::Status Freeze();
  ::util::Status Unfreeze();
  ::util::Status WriteForwardingEntries(const ::p4::v1::WriteRequest& req,
                                        std::vector<::util::Status>* results)
      LOCKS_EXCLUDED(lock_);
  ::util::Status ReadForwardingEntries(
      const ::p4::v1::ReadRequest& req,
      WriterInterface<::p4::v1::ReadResponse>* writer,
      std::vector<::util::Status>* details) LOCKS_EXCLUDED(lock_);
  ::util::Status RegisterPacketReceiveWriter(
      const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer)
      LOCKS_EXCLUDED(lock_);
  ::util::Status UnregisterPacketReceiveWriter() LOCKS_EXCLUDED(lock_);
  ::util::Status TransmitPacket(const ::p4::v1::PacketOut& packet)
      LOCKS_EXCLUDED(lock_);
  // Factory function for creating the instance of the class.
  static std::unique_ptr<BfrtNode> CreateInstance(
      BfrtTableManager* bfrt_table_manager,
      BfrtActionProfileManager* bfrt_action_profile_manager,
      BfrtPacketioManager* bfrt_packetio_manager,
      BfrtPreManager* bfrt_pre_manager,
      BfrtCounterManager* bfrt_counter_manager,
      ::bfrt::BfRtDevMgr* bfrt_device_manager, BfrtIdMapper* bfrt_id_mapper,
      BfSdeInterface* bf_sde_interface, int device_id);

  // BfrtNode is neither copyable nor movable.
  BfrtNode(const BfrtNode&) = delete;
  BfrtNode& operator=(const BfrtNode&) = delete;
  BfrtNode(BfrtNode&&) = delete;
  BfrtNode& operator=(BfrtNode&&) = delete;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BfrtNode(BfrtTableManager* bfrt_table_manager,
           BfrtActionProfileManager* bfrt_action_profile_manager,
           BfrtPacketioManager* bfrt_packetio_manager,
           BfrtPreManager* bfrt_pre_manager,
           BfrtCounterManager* bfrt_counter_manager,
           ::bfrt::BfRtDevMgr* bfrt_device_manager,
           BfrtIdMapper* bfrt_id_mapper, BfSdeInterface* bf_sde_interface,
           int device_id);

  // Write extern entries like ActionProfile, DirectCounter, PortMetadata
  ::util::Status WriteExternEntry(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type, const ::p4::v1::ExternEntry& entry);

  // Read extern entries like ActionProfile, DirectCounter, PortMetadata
  ::util::Status ReadExternEntry(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      const ::p4::v1::ExternEntry& entry,
      WriterInterface<::p4::v1::ReadResponse>* writer);

  // Callback registered with DeviceMgr to receive stream messages.
  friend void StreamMessageCb(uint64 node_id,
                              p4::v1::StreamMessageResponse* msg, void* cookie);

  // Reader-writer lock used to protect access to node-specific state.
  mutable absl::Mutex lock_;

  // Mutex used for exclusive access to rx_writer_.
  mutable absl::Mutex rx_writer_lock_;

  bool pipeline_initialized_ GUARDED_BY(lock_);
  bool initialized_ GUARDED_BY(lock_);

  // Managers. Not owned by this class.
  BfrtTableManager* bfrt_table_manager_;
  BfrtActionProfileManager* bfrt_action_profile_manager_;
  BfrtPacketioManager* bfrt_packetio_manager_;
  BfrtPreManager* bfrt_pre_manager_;
  BfrtCounterManager* bfrt_counter_manager_;
  ::bfrt::BfRtDevMgr* bfrt_device_manager_;

  // ID mapper which maps P4Runtime ID to BfRt ones, vice versa.
  BfrtIdMapper* bfrt_id_mapper_;  // Not owned by this class

  // Stores pipeline information for this node.
  const bfrt::BfRtInfo* bfrt_info_ GUARDED_BY(lock_);
  BfrtDeviceConfig bfrt_config_ GUARDED_BY(lock_);

  // Pointer to a BfSdeInterface implementation that wraps all the SDE calls.
  BfSdeInterface* bf_sde_interface_ = nullptr;  // not owned by this class.

  // Logical node ID corresponding to the node/ASIC managed by this class
  // instance. Assigned on PushChassisConfig() and might change during the
  // lifetime of the class.
  uint64 node_id_ GUARDED_BY(lock_);

  // Fixed zero-based BFRT device_id number corresponding to the node/ASIC
  // managed by this class instance. Assigned in the class constructor.
  const int device_id_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_NODE_H_
