// Copyright 2018-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PI_PI_NODE_H_
#define STRATUM_HAL_LIB_PI_PI_NODE_H_

#include <memory>
#include <vector>

#include "PI/frontends/proto/device_mgr.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"

namespace stratum {
namespace hal {
namespace pi {

// The PINode class encapsulates all per P4-native node/chip/ASIC
// functionalities, primarily the flow managers. Calls made to this class are
// processed and passed through to the DeviceMgr PIMPL from
// con_github_p4lang_PI.
class PINode final {
 public:
  ~PINode();

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

  // Factory function for creating the instance of the class.
  static std::unique_ptr<PINode> CreateInstance(
      ::pi::fe::proto::DeviceMgr* device_mgr, int unit);

  // PINode is neither copyable nor movable.
  PINode(const PINode&) = delete;
  PINode& operator=(const PINode&) = delete;
  PINode(PINode&&) = delete;
  PINode& operator=(PINode&&) = delete;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  PINode(::pi::fe::proto::DeviceMgr* device_mgr, int unit);

  // Callback registered with DeviceMgr to receive stream messages.
  friend void StreamMessageCb(uint64_t node_id,
                              p4::v1::StreamMessageResponse* msg, void* cookie);

  // Write packet on the registered RX writer.
  void SendPacketIn(const ::p4::v1::PacketIn& packet)
      LOCKS_EXCLUDED(rx_writer_lock_);

  // Reader-writer lock used to protect access to node-specific state.
  mutable absl::Mutex lock_;

  // Flow calls made to this class are forwarded to the DeviceMgr.
  ::pi::fe::proto::DeviceMgr* device_mgr_;  // not owned by the class

  // Mutex used for exclusive access to rx_writer_.
  mutable absl::Mutex rx_writer_lock_;

  // RX packet handler.
  std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> rx_writer_
      GUARDED_BY(rx_writer_lock_);

  const int unit_;

  bool pipeline_initialized_ GUARDED_BY(lock_);

  // Logical node ID corresponding to the node/ASIC managed by this class
  // instance. Assigned on PushChassisConfig() and might change during the
  // lifetime of the class.
  uint64 node_id_ GUARDED_BY(lock_);
};

}  // namespace pi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PI_PI_NODE_H_
