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

#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/common/switch_interface.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/dummy/dummy_chassis_mgr.h"
#include "stratum/glue/gtl/flat_hash_map.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

class DummySwitch : public SwitchInterface {
 public:
  ~DummySwitch() override;

  // Switch Interface methods
  ::util::Status PushChassisConfig(const ChassisConfig& config)
  EXCLUSIVE_LOCKS_REQUIRED(switch_config_lock_) override;
  ::util::Status VerifyChassisConfig(const ChassisConfig& config)
  SHARED_LOCKS_REQUIRED(switch_config_lock_) override;
  ::util::Status PushForwardingPipelineConfig(
      uint64 node_id,
      const ::p4::v1::ForwardingPipelineConfig& config)
  EXCLUSIVE_LOCKS_REQUIRED(switch_config_lock_) override;
  ::util::Status VerifyForwardingPipelineConfig(
      uint64 node_id,
      const ::p4::v1::ForwardingPipelineConfig& config)
  SHARED_LOCKS_REQUIRED(switch_config_lock_) override;
  ::util::Status Shutdown()
  EXCLUSIVE_LOCKS_REQUIRED(switch_config_lock_) override;
  ::util::Status Freeze()
  EXCLUSIVE_LOCKS_REQUIRED(switch_config_lock_) override;
  ::util::Status Unfreeze()
  EXCLUSIVE_LOCKS_REQUIRED(switch_config_lock_) override;

  ::util::Status WriteForwardingEntries(
      const ::p4::v1::WriteRequest& req,
      std::vector<::util::Status>* results)
  EXCLUSIVE_LOCKS_REQUIRED(p4_lock_) override;
  ::util::Status ReadForwardingEntries(
      const ::p4::v1::ReadRequest& req,
      WriterInterface<::p4::v1::ReadResponse>* writer,
      std::vector<::util::Status>* details)
  SHARED_LOCKS_REQUIRED(p4_lock_) override;
  ::util::Status RegisterPacketReceiveWriter(
      uint64 node_id,
      std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> writer)
  EXCLUSIVE_LOCKS_REQUIRED(p4_lock_) override;
  ::util::Status UnregisterPacketReceiveWriter(uint64 node_id)
  EXCLUSIVE_LOCKS_REQUIRED(p4_lock_) override;
  ::util::Status TransmitPacket(uint64 node_id,
                                const ::p4::v1::PacketOut& packet)
  EXCLUSIVE_LOCKS_REQUIRED(p4_lock_) override;
  ::util::Status RegisterEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer)
  EXCLUSIVE_LOCKS_REQUIRED(gnmi_event_lock_) override;
  ::util::Status UnregisterEventNotifyWriter()
  EXCLUSIVE_LOCKS_REQUIRED(gnmi_event_lock_) override;
  ::util::Status RetrieveValue(uint64 node_id, const DataRequest& requests,
                               WriterInterface<DataResponse>* writer,
                               std::vector<::util::Status>* details)
  SHARED_LOCKS_REQUIRED(p4_lock_) override;
  ::util::Status SetValue(uint64 node_id, const SetRequest& request,
                                  std::vector<::util::Status>* details)
  EXCLUSIVE_LOCKS_REQUIRED(switch_config_lock_) override;
  ::util::StatusOr<std::vector<std::string>> VerifyState()
  SHARED_LOCKS_REQUIRED(switch_config_lock_) override;

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

  PhalInterface* phal_interface_;
  DummyChassisManager* chassis_mgr_;

  // Locks
  ::absl::Mutex p4_lock_;
  ::absl::Mutex gnmi_event_lock_;
  ::absl::Mutex switch_config_lock_;
};

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum
#endif  // STRATUM_HAL_LIB_DUMMY_DUMMY_SWITCH_H_
