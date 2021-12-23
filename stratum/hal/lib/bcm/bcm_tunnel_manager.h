// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// This file declares the BcmTunnelManager class.

#ifndef STRATUM_HAL_LIB_BCM_BCM_TUNNEL_MANAGER_H_
#define STRATUM_HAL_LIB_BCM_BCM_TUNNEL_MANAGER_H_

#include <memory>
#include <utility>

#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/bcm_sdk_interface.h"
#include "stratum/hal/lib/bcm/bcm_table_manager.h"

namespace stratum {

namespace hal {
namespace bcm {

// The "BcmTunnelManager" class implements the encap/decap tunnel functionality.
class BcmTunnelManager {
 public:
  virtual ~BcmTunnelManager();

  // Pushes the parts of the given ChassisConfig proto that this class cares
  // about. If the class is not initialized (i.e. if config is pushed for the
  // first time), this function also initializes the instance. The given
  // node_id is used to understand which part of the ChassisConfig is intended
  // for this class.
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config,
                                           uint64 node_id);

  // Verifies the parts of ChassisConfig proto that this class cares about. The
  // given node_id is used to understand which part of the ChassisConfig is
  // intended for this class.
  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id);

  // Pushes a ForwardingPipelineConfig and sets up any tunnel-specific
  // attributes.
  virtual ::util::Status PushForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config);

  // Verfies a ForwardingPipelineConfig for the node without changing anything
  // on the HW.
  virtual ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config);

  // Performs coldboot shutdown. Note that there is no public Initialize().
  // Initialization is done as part of PushChassisConfig() if the class is not
  // initialized by the time we push config.
  virtual ::util::Status Shutdown();

  // Insert/modify/delete tunnels in P4 runtime write requests.
  // TODO(teverman): These may need to be tuned to encap/decap specifics on BCM.
  virtual ::util::Status InsertTableEntry(const ::p4::v1::TableEntry& entry);
  virtual ::util::Status ModifyTableEntry(const ::p4::v1::TableEntry& entry);
  virtual ::util::Status DeleteTableEntry(const ::p4::v1::TableEntry& entry);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BcmTunnelManager> CreateInstance(
      BcmSdkInterface* bcm_sdk_interface, BcmTableManager* bcm_table_manager,
      int unit);

  // BcmTunnelManager is neither copyable nor movable.
  BcmTunnelManager(const BcmTunnelManager&) = delete;
  BcmTunnelManager& operator=(const BcmTunnelManager&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BcmTunnelManager();

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  // TODO(teverman): Expect updates to the injected dependencies as the
  // implementation progresses.
  BcmTunnelManager(BcmSdkInterface* bcm_sdk_interface,
                   BcmTableManager* bcm_table_manager, int unit);

  // Pointer to a BcmSdkInterface implementation that wraps all the SDK calls.
  BcmSdkInterface* bcm_sdk_interface_;  // Not owned by this class.

  // Pointer to a BcmTableManger implementation that keeps track of table
  // entries and conversions.
  BcmTableManager* bcm_table_manager_;  // Not owned by this class.

  // Logical node ID corresponding to the node/ASIC managed by this class
  // instance. Assigned on PushChassisConfig() and might change during the
  // lifetime of the class.
  uint64 node_id_;

  // Fixed zero-based BCM unit number corresponding to the node/ASIC managed by
  // this class instance. Assigned in the class constructor.
  const int unit_;
};

}  // namespace bcm
}  // namespace hal

}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_TUNNEL_MANAGER_H_
