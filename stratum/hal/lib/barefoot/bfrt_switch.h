// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_SWITCH_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_SWITCH_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"
#include "stratum/hal/lib/barefoot/bf_pd_interface.h"
#include "stratum/hal/lib/barefoot/bfrt_node.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/switch_interface.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtSwitch : public SwitchInterface {
 public:
  ~BfrtSwitch() override;

  // SwitchInterface public methods.
  ::util::Status PushChassisConfig(const ChassisConfig& config) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status VerifyChassisConfig(const ChassisConfig& config) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status PushForwardingPipelineConfig(
      uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status SaveForwardingPipelineConfig(
      uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status CommitForwardingPipelineConfig(uint64 node_id) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status VerifyForwardingPipelineConfig(
      uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status Shutdown() override LOCKS_EXCLUDED(chassis_lock);
  ::util::Status Freeze() override;
  ::util::Status Unfreeze() override;
  ::util::Status WriteForwardingEntries(const ::p4::v1::WriteRequest& req,
                                        std::vector<::util::Status>* results)
      override LOCKS_EXCLUDED(chassis_lock);
  ::util::Status ReadForwardingEntries(
      const ::p4::v1::ReadRequest& req,
      WriterInterface<::p4::v1::ReadResponse>* writer,
      std::vector<::util::Status>* details) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status RegisterPacketReceiveWriter(
      uint64 node_id,
      std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> writer) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status UnregisterPacketReceiveWriter(uint64 node_id) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status TransmitPacket(uint64 node_id,
                                const ::p4::v1::PacketOut& packet) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status RegisterEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status UnregisterEventNotifyWriter() override
      LOCKS_EXCLUDED(chassis_lock) LOCKS_EXCLUDED(chassis_lock);
  ::util::Status RetrieveValue(uint64 node_id, const DataRequest& requests,
                               WriterInterface<DataResponse>* writer,
                               std::vector<::util::Status>* details) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status SetValue(uint64 node_id, const SetRequest& request,
                          std::vector<::util::Status>* details) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::StatusOr<std::vector<std::string>> VerifyState() override;

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BfrtSwitch> CreateInstance(
      PhalInterface* phal_interface, BFChassisManager* bf_chassis_manager,
      BFPdInterface* bf_pd_interface,
      const std::map<int, BfrtNode*>& device_id_to_bfrt_node);

  // BfrtSwitch is neither copyable nor movable.
  BfrtSwitch(const BfrtSwitch&) = delete;
  BfrtSwitch& operator=(const BfrtSwitch&) = delete;
  BfrtSwitch(BfrtSwitch&&) = delete;
  BfrtSwitch& operator=(BfrtSwitch&&) = delete;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BfrtSwitch(PhalInterface* phal_interface,
             BFChassisManager* bf_chassis_manager,
             BFPdInterface* bf_pd_interface,
             const std::map<int, BfrtNode*>& device_id_to_bfrt_node);

  // Helper to get BfrtNode pointer from device_id number or return error
  // indicating invalid device_id.
  ::util::StatusOr<BfrtNode*> GetBfrtNodeFromDeviceId(int device_id) const;

  // Helper to get BfrtNode pointer from node id or return error indicating
  // invalid/unknown/uninitialized node.
  ::util::StatusOr<BfrtNode*> GetBfrtNodeFromNodeId(uint64 node_id) const;

  // Pointer to a PhalInterface implementation. The pointer has been also
  // passed to a few managers for accessing HW. Note that there is only one
  // instance of this class per chassis.
  PhalInterface* phal_interface_;  // not owned by this class.

  // Per chassis Managers. Note that there is only one instance of this class
  // per chassis.
  BFChassisManager* bf_chassis_manager_;  // not owned by the class.

  // Pointer to a BFPdInterface implementation that wraps PD API calls.
  BFPdInterface* bf_pd_interface_;  // not owned by this class.

  // Map from zero-based device_id number corresponding to a node/ASIC to a
  // pointer to BfrtNode which contain all the per-node managers for that
  // node/ASIC. This map is initialized in the constructor and will not change
  // during the lifetime of the class.
  // TODO(max): Does this need to be protected by chassis_lock?
  const std::map<int, BfrtNode*> device_id_to_bfrt_node_;  // pointers not owned

  // Map from the node ids to to a pointer to BfrtNode which contain all the
  // per-node managers for that node/ASIC. Created everytime a config is pushed.
  // At any point of time this map will contain a keys the ids of the nodes
  // which had a successful config push.
  // TODO(max): Does this need to be protected by chassis_lock?
  std::map<uint64, BfrtNode*> node_id_to_bfrt_node_;  //  pointers not owned
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_SWITCH_H_
