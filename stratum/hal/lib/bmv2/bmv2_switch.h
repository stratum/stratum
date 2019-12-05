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

#ifndef STRATUM_HAL_LIB_BMV2_BMV2_SWITCH_H_
#define STRATUM_HAL_LIB_BMV2_BMV2_SWITCH_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/bmv2/bmv2_chassis_manager.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/switch_interface.h"
#include "stratum/hal/lib/pi/pi_node.h"

namespace stratum {
namespace hal {
namespace bmv2 {

class Bmv2Switch : public SwitchInterface {
 public:
  ~Bmv2Switch() override;

  // SwitchInterface public methods.
  ::util::Status PushChassisConfig(const ChassisConfig& config) override;
  ::util::Status VerifyChassisConfig(const ChassisConfig& config) override;
  ::util::Status PushForwardingPipelineConfig(
      uint64 node_id,
      const ::p4::v1::ForwardingPipelineConfig& config) override;
  ::util::Status SaveForwardingPipelineConfig(
      uint64 node_id,
      const ::p4::v1::ForwardingPipelineConfig& config) override;
  ::util::Status CommitForwardingPipelineConfig(uint64 node_id) override;
  ::util::Status VerifyForwardingPipelineConfig(
      uint64 node_id,
      const ::p4::v1::ForwardingPipelineConfig& config) override;
  ::util::Status Shutdown() override;
  ::util::Status Freeze() override;
  ::util::Status Unfreeze() override;
  ::util::Status WriteForwardingEntries(
      const ::p4::v1::WriteRequest& req,
      std::vector<::util::Status>* results) override;
  ::util::Status ReadForwardingEntries(
      const ::p4::v1::ReadRequest& req,
      WriterInterface<::p4::v1::ReadResponse>* writer,
      std::vector<::util::Status>* details) override;
  ::util::Status RegisterPacketReceiveWriter(
      uint64 node_id,
      std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> writer) override;
  ::util::Status UnregisterPacketReceiveWriter(uint64 node_id) override;
  ::util::Status TransmitPacket(uint64 node_id,
                                const ::p4::v1::PacketOut& packet) override;
  ::util::Status RegisterEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) override;
  ::util::Status UnregisterEventNotifyWriter() override;
  ::util::Status RetrieveValue(uint64 node_id, const DataRequest& requests,
                               WriterInterface<DataResponse>* writer,
                               std::vector<::util::Status>* details) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status SetValue(uint64 node_id, const SetRequest& request,
                          std::vector<::util::Status>* details) override;
  ::util::StatusOr<std::vector<std::string>> VerifyState() override;

  // Factory function for creating the instance of the class.
  // When using Bmv2Switch, the node id for each PINode instance is known at
  // instantiation time and cannot be changed by pushing a new chassis
  // config. The node id must match the bmv2 "device id" which must be provided
  // when initializing the bmv2 instance. When pushing a chassis config, we
  // verify that the "new" node ids match the current ones. We may change this
  // in the future: a new chassis config could trigger a new bmv2 switch
  // instance to be created with the new node id as the bmv2 device id. At the
  // moment, note that each Stratum process is limited to a single bmv2 instance
  // anyway.
  static std::unique_ptr<Bmv2Switch> CreateInstance(
      PhalInterface* phal_interface, Bmv2ChassisManager* bmv2_chassis_manager,
      const std::map<uint64, pi::PINode*>& node_id_to_pi_node);

  // Bmv2Switch is neither copyable nor movable.
  Bmv2Switch(const Bmv2Switch&) = delete;
  Bmv2Switch& operator=(const Bmv2Switch&) = delete;
  Bmv2Switch(Bmv2Switch&&) = delete;
  Bmv2Switch& operator=(Bmv2Switch&&) = delete;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  Bmv2Switch(PhalInterface* phal_interface,
             Bmv2ChassisManager* bmv2_chassis_manager,
             const std::map<uint64, pi::PINode*>& node_id_to_pi_node);

  // Helper to get PINode pointer from node id or return error indicating
  // invalid/unknown/uninitialized node.
  ::util::StatusOr<pi::PINode*> GetPINodeFromNodeId(uint64 node_id) const;

  // Pointer to a PhalInterface implementation. The pointer has been also
  // passed to a few managers for accessing HW. Note that there is only one
  // instance of this class per chassis.
  PhalInterface* phal_interface_;  // not owned by this class.

  // Per chassis Managers. Note that there is only one instance of this class
  // per chassis.
  Bmv2ChassisManager* bmv2_chassis_manager_;  // not owned by the class.

  // Map from the node ids to to a pointer to PINode which contain all the
  // per-node managers for that node/ASIC. Created everytime a config is pushed.
  // At any point of time this map will contain a keys the ids of the nodes
  // which had a successful config push.
  std::map<uint64, pi::PINode*> node_id_to_pi_node_;  //  pointers not owned
};

}  // namespace bmv2
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BMV2_BMV2_SWITCH_H_
