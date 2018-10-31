/* Copyright 2018-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STRATUM_HAL_LIB_PI_PI_NODE_H_
#define STRATUM_HAL_LIB_PI_PI_NODE_H_

#include <memory>

#include "PI/frontends/proto/device_mgr.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "absl/synchronization/mutex.h"

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

  ::util::Status PushChassisConfig(const ChassisConfig& config, uint64 node_id);
  ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                     uint64 node_id);
  ::util::Status PushForwardingPipelineConfig(
       const ::p4::v1::ForwardingPipelineConfig& config);
  ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config);
  ::util::Status Shutdown();
  ::util::Status Freeze();
  ::util::Status Unfreeze();
  ::util::Status WriteForwardingEntries(
       const ::p4::v1::WriteRequest& req,
       std::vector<::util::Status>* results);
  ::util::Status ReadForwardingEntries(
      const ::p4::v1::ReadRequest& req,
      WriterInterface<::p4::v1::ReadResponse>* writer,
      std::vector<::util::Status>* details);
  ::util::Status RegisterPacketReceiveWriter(
      const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer);
      LOCKS_EXCLUDED(rx_writer_lock_);
  ::util::Status UnregisterPacketReceiveWriter();
      LOCKS_EXCLUDED(rx_writer_lock_);
  ::util::Status TransmitPacket(const ::p4::v1::PacketOut& packet);

  // TODO(antonin): Currently the node_id is provided by the constructor / the
  // factory method. Implementations of SwitchInterface that use this node class
  // can use GetNodeId to retrieve the node_id. This is temporary: when we
  // support ChassisConfig we will remove the node_id parameter from the factory
  // method along with the GetNodeId method. The node will "learn" its node_id
  // through PushChassisConfig.

  int64 GetNodeId() const;

  // Factory function for creating the instance of the class.
  static std::unique_ptr<PINode> CreateInstance(
      ::pi::fe::proto::DeviceMgr* device_mgr,
      int unit,
      uint64 node_id);

  // PINode is neither copyable nor movable.
  PINode(const PINode&) = delete;
  PINode& operator=(const PINode&) = delete;
  PINode(PINode&&) = delete;
  PINode& operator=(PINode&&) = delete;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  PINode(::pi::fe::proto::DeviceMgr* device_mgr,
         int unit,
         uint64 node_id);

  // Callback registered with DeviceMgr to receive packet-ins.
  friend void PacketInCb(uint64_t node_id,
                         p4::v1::PacketIn* packet,
                         void* cookie);

  // Write packet on the registered RX writer.
  void SendPacketIn(::p4::v1::PacketIn* packet);
      LOCKS_EXCLUDED(rx_writer_lock_);

  // Flow calls made to this class are forwarded to the DeviceMgr.
  ::pi::fe::proto::DeviceMgr* device_mgr_;  // not owned by the class.

  // Mutex used for exclusive access to rx_writer_.
  mutable absl::Mutex rx_writer_lock_;

  // RX packet handler.
  std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> rx_writer_{nullptr};
      GUARDED_BY(rx_writer_lock_);

  // Fixed zero-based unit number corresponding to the node/ASIC managed by this
  // class instance. Assigned in the class constructor.
  const int unit_;

  const uint64 node_id_;
};

}  // namespace pi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PI_PI_SWITCH_H_
