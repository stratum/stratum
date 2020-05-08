// Copyright 2018-present Barefoot Networks, Inc.
// Copyright 2019-present Dell EMC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_NP4INTEL_NP4_SWITCH_H_
#define STRATUM_HAL_LIB_NP4INTEL_NP4_SWITCH_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/switch_interface.h"
#include "stratum/hal/lib/np4intel/np4_chassis_manager.h"
#include "stratum/hal/lib/pi/pi_node.h"

namespace stratum {
namespace hal {
namespace np4intel {

class NP4Switch : public SwitchInterface {
 public:
  ~NP4Switch() override;

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
  static std::unique_ptr<NP4Switch> CreateInstance(
      PhalInterface* phal_interface, NP4ChassisManager* np4_chassis_manager);

  // Bmv2Switch is neither copyable nor movable.
  NP4Switch(const NP4Switch&) = delete;
  NP4Switch& operator=(const NP4Switch&) = delete;
  NP4Switch(NP4Switch&&) = delete;
  NP4Switch& operator=(NP4Switch&&) = delete;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  NP4Switch(PhalInterface* phal_interface,
            NP4ChassisManager* np4_chassis_manager);

  // Helper to get PINode pointer from node id or return error indicating
  // invalid/unknown/uninitialized node.
  ::util::StatusOr<pi::PINode*> GetPINodeFromNodeId(uint64 node_id) const;

  // Pointer to a PhalInterface implementation. The pointer has been also
  // passed to a few managers for accessing HW. Note that there is only one
  // instance of this class per chassis.
  PhalInterface* phal_interface_;  // not owned by this class.

  // Per chassis Managers. Note that there is only one instance of this class
  // per chassis.
  NP4ChassisManager* np4_chassis_manager_;  // not owned by the class.

  // Map from the node ids to to a pointer to PINode which contain all the
  // per-node managers for that node/ASIC. Created everytime a config is pushed.
  // At any point of time this map will contain a keys the ids of the nodes
  // which had a successful config push.
  std::map<uint64, std::unique_ptr<pi::PINode>> node_id_to_pi_node_;

  // Map from the node ids to a unique point to the PI DeviceMgr
  std::map<uint64, std::unique_ptr<::pi::fe::proto::DeviceMgr>>
      node_id_to_device_mgr_;
};

}  // namespace np4intel
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_NP4INTEL_NP4_SWITCH_H_
