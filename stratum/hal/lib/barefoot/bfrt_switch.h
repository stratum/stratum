// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_SWITCH_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_SWITCH_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"
#include "stratum/hal/lib/barefoot/bf_global_vars.h"
#include "stratum/hal/lib/barefoot/bf_sde_interface.h"
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
  ::util::Status RegisterStreamMessageResponseWriter(
      uint64 node_id,
      std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>> writer)
      override LOCKS_EXCLUDED(chassis_lock);
  ::util::Status UnregisterStreamMessageResponseWriter(uint64 node_id) override
      LOCKS_EXCLUDED(chassis_lock);
  ::util::Status HandleStreamMessageRequest(
      uint64 node_id, const ::p4::v1::StreamMessageRequest& request) override
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
      PhalInterface* phal_interface, BfChassisManager* bf_chassis_manager,
      BfSdeInterface* bf_sde_interface,
      const absl::flat_hash_map<int, BfrtNode*>& device_id_to_bfrt_node);

  // BfrtSwitch is neither copyable nor movable.
  BfrtSwitch(const BfrtSwitch&) = delete;
  BfrtSwitch& operator=(const BfrtSwitch&) = delete;
  BfrtSwitch(BfrtSwitch&&) = delete;
  BfrtSwitch& operator=(BfrtSwitch&&) = delete;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BfrtSwitch(PhalInterface* phal_interface,
             BfChassisManager* bf_chassis_manager,
             BfSdeInterface* bf_sde_interface,
             const absl::flat_hash_map<int, BfrtNode*>& device_id_to_bfrt_node);

  // Internal version of VerifyForwardingPipelineConfig() which takes no locks.
  ::util::Status DoVerifyForwardingPipelineConfig(
      uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock);

  // Internal version of VerifyChassisConfig() which takes no locks.
  ::util::Status DoVerifyChassisConfig(const ChassisConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock);

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

  // Pointer to a BfSdeInterface implementation that wraps PD API calls.
  BfSdeInterface* bf_sde_interface_;  // not owned by this class.

  // Per chassis Managers. Note that there is only one instance of this class
  // per chassis.
  BfChassisManager* bf_chassis_manager_;  // not owned by the class.

  // Map from zero-based device_id number corresponding to a node/ASIC to a
  // pointer to BfrtNode which contain all the per-node managers for that
  // node/ASIC. This map is initialized in the constructor and will not change
  // during the lifetime of the class.
  // Pointers not owned.
  // TODO(max): Does this need to be protected by chassis_lock?
  const absl::flat_hash_map<int, BfrtNode*> device_id_to_bfrt_node_;

  // Map from the node ids to to a pointer to BfrtNode which contain all the
  // per-node managers for that node/ASIC. Created everytime a config is
  // pushed. At any point of time this map will contain a keys the ids of
  // the nodes which had a successful config push.
  // Pointers not owned.
  // TODO(max): Does this need to be protected by chassis_lock?
  absl::flat_hash_map<uint64, BfrtNode*> node_id_to_bfrt_node_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_SWITCH_H_
