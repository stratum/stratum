// Copyright 2018-present Open Networking Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STRATUM_HAL_LIB_DUMMY_DUMMY_SWITCH_H_
#define STRATUM_HAL_LIB_DUMMY_DUMMY_SWITCH_H_

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <map>

#include "absl/synchronization/mutex.h"
#include "absl/container/flat_hash_map.h"
#include "stratum/hal/lib/common/switch_interface.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/dummy/dummy_chassis_mgr.h"
#include "stratum/hal/lib/dummy/dummy_global_vars.h"
#include "stratum/hal/lib/dummy/dummy_node.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

class DummySwitch : public SwitchInterface {
 public:
  // Switch Interface methods
  ::util::Status PushChassisConfig(const ChassisConfig& config)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status VerifyChassisConfig(const ChassisConfig& config)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status Shutdown()
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status Freeze()
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status Unfreeze()
  LOCKS_EXCLUDED(chassis_lock) override;

  ::util::Status PushForwardingPipelineConfig(
      uint64 node_id,
      const ::p4::v1::ForwardingPipelineConfig& config)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status SaveForwardingPipelineConfig(
      uint64 node_id,
      const ::p4::v1::ForwardingPipelineConfig& config)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status CommitForwardingPipelineConfig(uint64 node_id)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status VerifyForwardingPipelineConfig(
      uint64 node_id,
      const ::p4::v1::ForwardingPipelineConfig& config)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status WriteForwardingEntries(
      const ::p4::v1::WriteRequest& req,
      std::vector<::util::Status>* results)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status ReadForwardingEntries(
      const ::p4::v1::ReadRequest& req,
      WriterInterface<::p4::v1::ReadResponse>* writer,
      std::vector<::util::Status>* details)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status RegisterPacketReceiveWriter(
      uint64 node_id,
      std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> writer)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status UnregisterPacketReceiveWriter(uint64 node_id)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status TransmitPacket(uint64 node_id,
                                const ::p4::v1::PacketOut& packet)
  LOCKS_EXCLUDED(chassis_lock) override;

  ::util::Status RegisterEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status UnregisterEventNotifyWriter()
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status RetrieveValue(uint64 node_id, const DataRequest& requests,
                               WriterInterface<DataResponse>* writer,
                               std::vector<::util::Status>* details)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::Status SetValue(uint64 node_id, const SetRequest& request,
                                  std::vector<::util::Status>* details)
  LOCKS_EXCLUDED(chassis_lock) override;
  ::util::StatusOr<std::vector<std::string>> VerifyState()
  LOCKS_EXCLUDED(chassis_lock) override;

  // Factory function for creating the instance of the DummySwitch.
  static std::unique_ptr<DummySwitch>
    CreateInstance(PhalInterface* phal_interface,
    DummyChassisManager* chassis_mgr);

  // Neither copyable nor movable.
  DummySwitch(const DummySwitch&) = delete;
  DummySwitch& operator=(const DummySwitch&) = delete;
  DummySwitch(DummySwitch&) = delete;
  DummySwitch& operator=(DummySwitch&) = delete;

 private:
  // Hide the constructor, using CreateInstance instead.
  DummySwitch(PhalInterface* phal_interface, DummyChassisManager* chassis_mgr);

  // Get a DummyNode based on the Id.
  ::util::StatusOr<DummyNode*> GetDummyNode(uint64 node_id)
  SHARED_LOCKS_REQUIRED(chassis_lock);

  std::vector<DummyNode*> GetDummyNodes()
  SHARED_LOCKS_REQUIRED(chassis_lock);

  PhalInterface* phal_interface_;
  DummyChassisManager* chassis_mgr_;
  ::absl::flat_hash_map<uint64, DummyNode*> dummy_nodes_;
  std::shared_ptr<WriterInterface<GnmiEventPtr>> gnmi_event_writer_;

  // gets slot number for a node_id + port_id pair
  std::map<std::pair<uint64, uint32>, int> node_port_id_to_slot;

  // gets port number for a node_id + port_id pair
  std::map<std::pair<uint64, uint32>, int> node_port_id_to_port;
};

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum
#endif  // STRATUM_HAL_LIB_DUMMY_DUMMY_SWITCH_H_
