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
#include "stratum/hal/lib/barefoot/bfrt.pb.h"
#include "stratum/hal/lib/barefoot/bfrt_action_profile_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_id_mapper.h"
#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"
#include "stratum/hal/lib/barefoot/macros.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"

#define _PI_UPDATE_MAX_NAME_SIZE 100
#define _PI_UPDATE_MAX_TMP_FILENAME_SIZE (_PI_UPDATE_MAX_NAME_SIZE + 32)

namespace stratum {
namespace hal {
namespace barefoot {

// The BfRtNode class encapsulates all per P4-native node/chip/ASIC
// functionalities, primarily the flow managers. Calls made to this class are
// processed and passed through to the BfRt API.
class BfRtNode final {
 public:
  ~BfRtNode();

  ::util::Status PushChassisConfig(const ChassisConfig& config, uint64 node_id)
      LOCKS_EXCLUDED(lock_);
  ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                     uint64 node_id) LOCKS_EXCLUDED(lock_);
  ::util::Status PushForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config);
  ::util::Status SaveForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config);
  ::util::Status CommitForwardingPipelineConfig();
  ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config);
  ::util::Status Shutdown();
  ::util::Status Freeze();
  ::util::Status Unfreeze();
  ::util::Status WriteForwardingEntries(const ::p4::v1::WriteRequest& req,
                                        std::vector<::util::Status>* results);
  ::util::Status ReadForwardingEntries(
      const ::p4::v1::ReadRequest& req,
      WriterInterface<::p4::v1::ReadResponse>* writer,
      std::vector<::util::Status>* details);
  ::util::Status RegisterPacketReceiveWriter(
      const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer)
      LOCKS_EXCLUDED(rx_writer_lock_);
  ::util::Status UnregisterPacketReceiveWriter()
      LOCKS_EXCLUDED(rx_writer_lock_);
  ::util::Status TransmitPacket(const ::p4::v1::PacketOut& packet);

  // handles extern entries like ActionProfile, DirectCounter, PortMetadata
  ::util::Status WriteExternEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type type, const ::p4::v1::ExternEntry& entry);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BfRtNode> CreateInstance(
      BfRtTableManager* bfrt_table_manager,
      BfRtActionProfileManager* bfrt_action_profile_manager,
      ::bfrt::BfRtDevMgr* bfrt_device_manager, BfRtIdMapper* bfrt_id_mapper,
      int unit);

  // BfRtNode is neither copyable nor movable.
  BfRtNode(const BfRtNode&) = delete;
  BfRtNode& operator=(const BfRtNode&) = delete;
  BfRtNode(BfRtNode&&) = delete;
  BfRtNode& operator=(BfRtNode&&) = delete;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BfRtNode(BfRtTableManager* bfrt_table_manager,
           BfRtActionProfileManager* bfrt_action_profile_manager,
           ::bfrt::BfRtDevMgr* bfrt_device_manager,
           BfRtIdMapper* bfrt_id_mapper, int unit);

  // Callback registered with DeviceMgr to receive stream messages.
  friend void StreamMessageCb(uint64_t node_id,
                              p4::v1::StreamMessageResponse* msg, void* cookie);

  // Write packet on the registered RX writer.
  void SendPacketIn(const ::p4::v1::PacketIn& packet)
      LOCKS_EXCLUDED(rx_writer_lock_);

  // Extracts the device config from the packed message and loads it into the
  // node. It is not loaded into the SDE yet.
  ::util::Status LoadP4DeviceConfig(const std::string& p4_device_config)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Reader-writer lock used to protect access to node-specific state.
  mutable absl::Mutex lock_;

  // Mutex used for exclusive access to rx_writer_.
  mutable absl::Mutex rx_writer_lock_;

  // RX packet handler.
  std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> rx_writer_
      GUARDED_BY(rx_writer_lock_);

  bool pipeline_initialized_ GUARDED_BY(lock_);
  bool initialized_ GUARDED_BY(lock_);

  // Managers. Not owned by this class.
  BfRtTableManager* bfrt_table_manager_;
  BfRtActionProfileManager* bfrt_action_profile_manager_;
  ::bfrt::BfRtDevMgr* bfrt_device_manager_;

  // ID mapper which maps P4Runtime ID to BfRt ones, vice versa.
  BfRtIdMapper* bfrt_id_mapper_;  // Not owned by this class

  // Stores pipeline information for this node.
  p4::config::v1::P4Info p4info_ GUARDED_BY(lock_);
  const bfrt::BfRtInfo* bfrt_info_ GUARDED_BY(lock_);
  BfrtDeviceConfig bfrt_config_ GUARDED_BY(lock_);

  // Logical node ID corresponding to the node/ASIC managed by this class
  // instance. Assigned on PushChassisConfig() and might change during the
  // lifetime of the class.
  uint64 node_id_ GUARDED_BY(lock_);

  // Fixed zero-based BFRT unit number corresponding to the node/ASIC managed by
  // this class instance. Assigned in the class constructor.
  const int unit_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_NODE_H_
