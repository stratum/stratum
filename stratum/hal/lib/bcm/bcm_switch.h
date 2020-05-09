// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_BCM_BCM_SWITCH_H_
#define STRATUM_HAL_LIB_BCM_BCM_SWITCH_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"
#include "stratum//hal/lib/bcm/bcm_global_vars.h"
#include "stratum/hal/lib/bcm/bcm_node.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/switch_interface.h"
#include "stratum/glue/integral_types.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace bcm {

// The "BcmSwitch" class represents an implementation of SwitchInterface based
// on BCM SDK. We use SDK calls directly to program the switching ASIC.
class BcmSwitch : public SwitchInterface {
 public:
  ~BcmSwitch() override;

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
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status RetrieveValue(uint64 node_id, const DataRequest& request,
                               WriterInterface<DataResponse>* writer,
                               std::vector<::util::Status>* details) override;
  ::util::StatusOr<std::vector<std::string>> VerifyState() override;
  ::util::Status SetValue(uint64 node_id, const SetRequest& request,
                          std::vector<::util::Status>* details) override
      LOCKS_EXCLUDED(chassis_lock);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BcmSwitch> CreateInstance(
      PhalInterface* phal_interface, BcmChassisManager* bcm_chassis_manager,
      const std::map<int, BcmNode*>& unit_to_bcm_node);

  // BcmSwitch is neither copyable nor movable.
  BcmSwitch(const BcmSwitch&) = delete;
  BcmSwitch& operator=(const BcmSwitch&) = delete;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BcmSwitch(PhalInterface* phal_interface,
            BcmChassisManager* bcm_chassis_manager,
            const std::map<int, BcmNode*>& unit_to_bcm_node);

  // Internal version of VerifyChassisConfig() which takes no locks.
  ::util::Status DoVerifyChassisConfig(const ChassisConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock);

  // Internal version of VerifyForwardingPipelineConfig() which takes no locks.
  ::util::Status DoVerifyForwardingPipelineConfig(
      uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock);

  // Helper to get BcmNode pointer from unit number or return error indicating
  // invalid unit.
  ::util::StatusOr<BcmNode*> GetBcmNodeFromUnit(int unit) const;

  // Helper to get BcmNode pointer from node id or return error indicating
  // invalid/unknown/uninitialized node.
  ::util::StatusOr<BcmNode*> GetBcmNodeFromNodeId(uint64 node_id) const;

  // Pointer to a PhalInterface implementation. The pointer has been also
  // passed to a few managers for accessing HW. Note that there is only one
  // instance of this class per chassis.
  PhalInterface* phal_interface_;  // not owned by this class.

  // Per chassis Managers. Note that there is only one
  // instance of this class per chassis.
  BcmChassisManager* bcm_chassis_manager_;  // not owned by the class.

  // Map from zero-based unit number corresponding to a node/ASIC to a pointer
  // to BcmNode which contain all the per-node managers for that node/ASIC. This
  // map is initialized in the constructor and will not change during the
  // lifetime of the class.
  const std::map<int, BcmNode*> unit_to_bcm_node_;  // pointers not owned.

  // Map from the node ids to to a pointer to BcmNode which contain all the
  // per-node managers for that node/ASIC. Created everytime a config is pushed.
  // At any point of time this map will contain a keys the ids of the nodes
  // which had a successful config push.
  std::map<uint64, BcmNode*> node_id_to_bcm_node_;  //  pointers not owned

  friend class BcmSwitchTest;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_SWITCH_H_
