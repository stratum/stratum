// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_BCM_BCM_ACL_MANAGER_H_
#define STRATUM_HAL_LIB_BCM_BCM_ACL_MANAGER_H_

#include <memory>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/bcm/bcm_chassis_ro_interface.h"
#include "stratum/hal/lib/bcm/bcm_sdk_interface.h"
#include "stratum/hal/lib/bcm/bcm_table_manager.h"
#include "stratum/hal/lib/bcm/pipeline_processor.h"
#include "stratum/hal/lib/p4/p4_control.pb.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/hal/lib/p4/p4_table_mapper.h"
#include "stratum/glue/integral_types.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

DECLARE_string(bcm_hardware_specs_file);

namespace stratum {
namespace hal {
namespace bcm {

class BcmAclManager {
 public:
  virtual ~BcmAclManager();

  // Pushes the parts of the given ChassisConfig proto that this class cares
  // about and mutate the internal state if needed. The given node_id is used to
  // understand which part of the ChassisConfig is intended for this class
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config,
                                           uint64 node_id);

  // Verifies the parts of ChassisConfig proto that this class cares about. The
  // given node_id is used to understand which part of the ChassisConfig is
  // intended for this class
  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id);

  // Pushes a ForwardingPipelineConfig and setup ACL tables based on that.
  virtual ::util::Status PushForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config);

  // Verfied a ForwardingPipelineConfig for the node without changing anything
  // on the HW.
  virtual ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config);

  // Performs coldboot shutdown. Note that there is no public Initialize().
  // Initialization is done as part of PushChassisConfig() if the class is not
  // initialized by the time we push config.
  virtual ::util::Status Shutdown();

  // Add an entry to an ACL table.
  virtual ::util::Status InsertTableEntry(
      const ::p4::v1::TableEntry& entry) const;

  // Modify an entry in an ACL table. Only actions can be modified.
  virtual ::util::Status ModifyTableEntry(
      const ::p4::v1::TableEntry& entry) const;

  // Delete an entry from an ACL table.
  virtual ::util::Status DeleteTableEntry(
      const ::p4::v1::TableEntry& entry) const;

  // Add/Modify direct meter (meter bound to a TableEntry) in hardware.
  virtual ::util::Status UpdateTableEntryMeter(
      const ::p4::v1::DirectMeterEntry& meter) const;

  // Get ACL table entry stats from hardware.
  virtual ::util::Status GetTableEntryStats(
      const ::p4::v1::TableEntry& entry, ::p4::v1::CounterData* counter) const;

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BcmAclManager> CreateInstance(
      BcmChassisRoInterface* bcm_chassis_ro_interface,
      BcmTableManager* bcm_table_manager, BcmSdkInterface* bcm_sdk_interface,
      P4TableMapper* p4_table_mapper, int unit);

  // BcmAclManager is neither copyable nor movable.
  BcmAclManager(const BcmAclManager&) = delete;
  BcmAclManager& operator=(const BcmAclManager&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BcmAclManager();

 private:
  template <typename T>
  using BcmAclStageMap =
      absl::flat_hash_map<BcmAclStage, T, EnumHash<BcmAclStage>>;

  // A physical ACL table represented by its component logical tables and the
  // ACL stage.
  struct PhysicalAclTable {
    std::vector<AclTable> logical_tables;
    BcmAclStage stage;
  };

  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BcmAclManager(BcmChassisRoInterface* bcm_chassis_ro_interface,
                BcmTableManager* bcm_table_manager,
                BcmSdkInterface* bcm_sdk_interface,
                P4TableMapper* p4_table_mapper, int unit);

  // Perform one-time ACL setup for a given unit.
  ::util::Status OneTimeSetup();

  // Clear the contents of all the ACL tables from the hardware and the
  // BcmTableManager instance.
  ::util::Status ClearAllAclTables();

  // Split an ACL control block into per-BcmAclStage control blocks. These
  // individual blocks represent IFP, VFP, and EFP control flows.
  ::util::Status SplitAclControlBlocks(
      const P4ControlBlock& control_block,
      BcmAclStageMap<P4ControlBlock>* stage_blocks) const;

  // Generate the set of physical vectors described in a P4 control pipeline.
  ::util::StatusOr<std::vector<PhysicalAclTable>> PhysicalAclTablesFromPipeline(
      const P4ControlBlock& control_block) const;

  // Generate the set of AclTable objects in a PhysicalAclTable and return the
  // PhysicalAclTable (a collection of AclTables).
  ::util::StatusOr<PhysicalAclTable> GeneratePhysicalAclTables(
      BcmAclStage stage,
      const PipelineProcessor::PhysicalTableAsVector& physical_table) const;

  // Install a group of logical tables as one physical table in Bcm.
  ::util::StatusOr<int> InstallPhysicalTable(
      const PhysicalAclTable& physical_acl_table) const;

  // Get the set of BcmField types supported by an AclTable.
  ::util::StatusOr<
      absl::flat_hash_set<BcmField::Type, EnumHash<BcmField::Type>>>
  GetTableMatchTypes(const AclTable& table) const;

  // The last P4PipelineConfig pushed to the class.
  P4PipelineConfig p4_pipeline_config_;

  // Initialized to false, set once only on first PushChassisConfig.
  bool initialized_;

  // Pointer to a BcmChassisRoInterface that keeps track of the chassis
  // information.
  BcmChassisRoInterface* bcm_chassis_ro_interface_;  // not owned by this class.

  // Pointer to a BcmTableManager that keeps track of the ACL tables.
  BcmTableManager* bcm_table_manager_;  // not owned by this class.

  // Pointer to a BcmSdkInterface implementation that wraps all the SDK calls.
  BcmSdkInterface* bcm_sdk_interface_;  // not owned by this class.

  // Pointer to a P4TableMapper used to look up P4 table information.
  P4TableMapper* p4_table_mapper_;  // not owned by this class.

  // Logical node ID corresponding to the node/ASIC managed by this class
  // instance. Assigned on PushChassisConfig() and might change during the
  // lifetime of the class.
  uint64 node_id_;

  // Fixed zero-based BCM unit number corresponding to the node/ASIC managed by
  // this class instance. Assigned in the class constructor.
  const int unit_;

  // Hardware description of the current chip.
  BcmHardwareSpecs::ChipModelSpec chip_hardware_description_;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_ACL_MANAGER_H_
