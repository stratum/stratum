/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef STRATUM_HAL_LIB_BCM_BCM_TABLE_MANAGER_H_
#define STRATUM_HAL_LIB_BCM_BCM_TABLE_MANAGER_H_

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/bcm/acl_table.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/bcm_acl_pipeline.h"
#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"
#include "stratum/hal/lib/bcm/bcm_flow_table.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/p4/common_flow_entry.pb.h"
#include "stratum/hal/lib/p4/p4_table_mapper.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/hal.grpc.pb.h"
#include "stratum/glue/integral_types.h"
#include "github.com/p4lang/PI/p4/config/v1/p4info.pb.h"
#include "github.com/p4lang/PI/p4/v1/p4runtime.grpc.pb.h"
#include "stratum/glue/gtl/flat_hash_map.h"
#include "stratum/glue/gtl/flat_hash_set.h"

namespace stratum {
namespace hal {
namespace bcm {

// Custom hash and equal function for ::p4::ActionProfileMember protos. For
// members, all that matters is the member_id (local to each node). If anything
// else id different (for example when a member with a fixed ID is modified), we
// expect to remove the old member from the container and replace it with the
// new one.
struct ActionProfileMemberHash {
  size_t operator()(const ::p4::ActionProfileMember& x) const {
    return static_cast<size_t>(x.member_id());
  }
};

struct ActionProfileMemberEqual {
  bool operator()(const ::p4::ActionProfileMember& x,
                  const ::p4::ActionProfileMember& y) const {
    return x.member_id() == y.member_id();
  }
};

// Custom hash and equal function for ::p4::ActionProfileGroup protos. For
// groups, all that matters is the group_id (local to each node). If anything
// else id different (for example when a group with a fixed ID is modified), we
// expect to remove the old group from the container and replace it with the new
// one.
struct ActionProfileGroupHash {
  size_t operator()(const ::p4::ActionProfileGroup& x) const {
    return static_cast<size_t>(x.group_id());
  }
};

struct ActionProfileGroupEqual {
  bool operator()(const ::p4::ActionProfileGroup& x,
                  const ::p4::ActionProfileGroup& y) const {
    return x.group_id() == y.group_id();
  }
};

// This struct encapsulates all the egress info for a non-multipath nexthop that
// has been already programmed on the specific node/ASIC managed by the
// BcmTableManager class.
struct BcmNonMultipathNexthopInfo {
  // Egress intf ID for the nexthop as given by the SDK after creation.
  int egress_intf_id;
  // The type of the nexthop.
  BcmNonMultipathNexthop::Type type;
  // Ref count for groups (number of groups this member is part of).
  uint32 group_ref_count;
  // Ref count for flows (number of flows directly pointing to this member).
  uint32 flow_ref_count;
  BcmNonMultipathNexthopInfo()
      : egress_intf_id(-1),
        type(BcmNonMultipathNexthop::NEXTHOP_TYPE_UNKNOWN),
        group_ref_count(0),
        flow_ref_count(0) {}
};

// This struct encapsulates all the egress info for a multipath (ECMP/WCMP)
// nexthop has been already programmed on the specific node/ASIC managed by the
// BcmTableManager class.
struct BcmMultipathNexthopInfo {
  // Egress intf ID for the nexthop as given by the SDK after creation.
  int egress_intf_id;
  // Ref count for flows (number of flows directly pointing to this group).
  uint32 flow_ref_count;
  // Map from all the members to their weights (for WCMP).
  std::map<uint32, uint32> member_id_to_weight;
  BcmMultipathNexthopInfo()
      : egress_intf_id(-1), flow_ref_count(0), member_id_to_weight() {}
};

// The "BcmTableManager" class implements the L3 routing functionality.
class BcmTableManager {
 public:
  virtual ~BcmTableManager();

  // Pushes the parts of the given ChassisConfig proto that this class cares
  // about. If the class is not initialized (i.e. if config is pushed for the
  // first time), this function also initializes class. The given node_id is
  // used to understand which part of the ChassisConfig is intended for this
  // class.
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config,
                                           uint64 node_id);

  // Verifies the parts of ChassisConfig proto that this class cares about. The
  // given node_id is used to understand which part of the ChassisConfig is
  // intended for this class.
  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id);

  // Pushes the P4-based forwarding pipeline configuration for the node. The
  // ::p4::ForwardingPipelineConfig proto includes the config for one node. This
  // method is expected to be called after PushChassisConfig().
  virtual ::util::Status PushForwardingPipelineConfig(
      const ::p4::ForwardingPipelineConfig& config);

  // Verifies the P4-based forwarding pipeline configuration for the node.
  virtual ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::ForwardingPipelineConfig& config);

  // Performs coldboot shutdown. Note that there is no public Initialize().
  // Initialization is done as part of PushChassisConfig() if the class is not
  // initialized by the time we push config.
  virtual ::util::Status Shutdown();

  // Given a P4FieldType, returns the corresponding BcmField::Type. Returns
  // BcmField::UNKNOWN if the conversion fails.
  virtual BcmField::Type P4FieldTypeToBcmFieldType(
      P4FieldType p4_field_type) const;

  // Given a CommonFlowEntry, populates the BCM specific BcmFlowEntry message to
  // be passed to low-level managers to program to the ASIC.
  virtual ::util::Status CommonFlowEntryToBcmFlowEntry(
      const CommonFlowEntry& common_flow_entry,
      BcmFlowEntry* bcm_flow_entry) const;

  // Given a ::p4::TableEntry, populates the BCM specific BcmFlowEntry message
  // to be passed to low-level managers to program to the ASIC.
  virtual ::util::Status FillBcmFlowEntry(const ::p4::TableEntry& table_entry,
                                          ::p4::Update::Type type,
                                          BcmFlowEntry* bcm_flow_entry) const;

  // Given a ::p4::ActionProfileMember, populates the BCM-specific
  // BcmNonMultipathNexthop message to be passed to low-level managers to
  // program to the ASIC.
  virtual ::util::Status FillBcmNonMultipathNexthop(
      const ::p4::ActionProfileMember& action_profile_member,
      BcmNonMultipathNexthop* bcm_non_multipath_nexthop) const;

  // Given a ::p4::ActionProfileGroup, populates the BCM specific
  // BcmMultipathNexthop message to be passed to low-level managers to program
  // to the ASIC.
  virtual ::util::Status FillBcmMultipathNexthop(
      const ::p4::ActionProfileGroup& action_profile_group,
      BcmMultipathNexthop* bcm_multipath_nexthop) const;

  // Transer meter configuration from ::p4::MeterConfig to BcmMeterConfig.
  ::util::Status FillBcmMeterConfig(const ::p4::MeterConfig& p4_meter,
                                    BcmMeterConfig* bcm_meter) const;

  // Saves a copy of ::p4::TableEntry for sending back to controller later. This
  // is called after the flow is programmed on hardware. This should not be
  // called in conjunction with AddAclTableEntry.
  virtual ::util::Status AddTableEntry(const ::p4::TableEntry& table_entry);

  // Add an ACL table entry. This function is the same as AddTableEntry, but it
  // also associates the entry with a BCM flow ID. For any table entry, only one
  // of AddTableEntry or AddAclTableEntry should be called.
  virtual ::util::Status AddAclTableEntry(const ::p4::TableEntry& table_entry,
                                          int bcm_flow_id);

  // Replaces an existing copy of ::p4::TableEntry with the one passed to
  // the function and matches the copy (assumes there is one copy that matches
  // the given ::p4::TableEntry). This is called after the flow is modified on
  // hardware.
  virtual ::util::Status UpdateTableEntry(const ::p4::TableEntry& table_entry);

  // Deletes an existing copy of ::p4::TableEntry which is matching the one
  // passed to the function. This is called after the flow is removed from
  // hardware.
  virtual ::util::Status DeleteTableEntry(const ::p4::TableEntry& table_entry);

  // Updates the meter configuration for the table entry specified inside the
  // given DirectMeterEntry argument.
  virtual ::util::Status UpdateTableEntryMeter(
      const ::p4::DirectMeterEntry& meter);

  // Saves a copy of the ::p4::ActionProfileMember for sending back to the
  // controller later. This function also adds a BcmNonMultipathNexthopInfo for
  // the ECMP/WCMP group member corresponding to the ::p4::ActionProfileMember
  // giving its type and egress intf ID. This is called after the member is
  // added to hardware.
  virtual ::util::Status AddActionProfileMember(
      const ::p4::ActionProfileMember& action_profile_member,
      BcmNonMultipathNexthop::Type type, int egress_intf_id);

  // Saves a copy of the ::p4::ActionProfileGroup for sending back to the
  // controller later. This function also adds a BcmMultipathNexthopInfo for the
  // ECMP/WCMP group corresponding to the ::p4::ActionProfileGroup, giving its
  // egress intf ID (there is no type for BcmMultipathNexthopInfo). This is
  // called after the group is added to hardware.
  virtual ::util::Status AddActionProfileGroup(
      const ::p4::ActionProfileGroup& action_profile_group, int egress_intf_id);

  // Replaces an existing copy of ::p4::ActionProfileMember with the one passed
  // to the function and matches the copy (assumes there is one copy that
  // matches the given ::p4::ActionProfileMember). This function also updates an
  // existing BcmNonMultipathNexthopInfo for the ECMP/WCMP group member
  // corresponding to the ::p4::ActionProfileMember. The egress intf ID of the
  // ECMP/WCMP group member and/or its type may change as part of a member
  // update. This is called after the member is modified on hardware.
  virtual ::util::Status UpdateActionProfileMember(
      const ::p4::ActionProfileMember& action_profile_member,
      BcmNonMultipathNexthop::Type type);

  // Replaces an existing copy of ::p4::ActionProfileGroup with the one passed
  // to the function and matches the copy (assumes there is one copy that
  // matches the given ::p4::ActionProfileGroup). This function also updates an
  // existing BcmMultipathNexthopInfo for the ECMP/WCMP group corresponding to
  // the ::p4::ActionProfileGroup. This is called after the group is modified on
  // hardware.
  virtual ::util::Status UpdateActionProfileGroup(
      const ::p4::ActionProfileGroup& action_profile_group);

  // Deletes an existing copy of ::p4::ActionProfileMember which is matching the
  // one passed to the function. This function also deletes the
  // BcmNonMultipathNexthopInfo for the ECMP/WCMP group member corresponding to
  // the ::p4::ActionProfileMember. This is called after the member is removed
  // from hardware.
  virtual ::util::Status DeleteActionProfileMember(
      const ::p4::ActionProfileMember& action_profile_member);

  // Deletes an existing copy of ::p4::ActionProfileGroup which is matching the
  // one passed to the function. This function also deletes the
  // BcmNonMultipathNexthopInfo for the ECMP/WCMP group corresponding to the
  // ::p4::ActionProfileGroup. This is called after the group is removed from
  // hardware.
  virtual ::util::Status DeleteActionProfileGroup(
      const ::p4::ActionProfileGroup& action_profile_group);

  // Returns the vector of the IDs of all the groups which a member is part of.
  virtual ::util::StatusOr<std::set<uint32>> GetGroupsForMember(
      uint32 member_id) const;

  // Helper which determines whether a member exists.
  virtual bool ActionProfileMemberExists(uint32 member_id) const;

  // Helper which determines whether a group exists.
  virtual bool ActionProfileGroupExists(uint32 group_id) const;

  // Populates the BcmNonMultipathNexthopInfo corresponding to an ECMP/WCMP
  // group member given its member_id. Returns error if the member cannot be
  // found in the internal maps. Internally uses the private version of
  // GetBcmNonMultipathNexthopInfo which return the pointer to the member
  // nexthop info.
  virtual ::util::Status GetBcmNonMultipathNexthopInfo(
      uint32 member_id, BcmNonMultipathNexthopInfo* info) const;

  // Populates the BcmMultipathNexthopInfo corresponding to an ECMP/WCMP group
  // given its group_id. Returns error if the group cannot be found in the
  // internal maps. Internally uses the private version of
  // GetBcmMultipathNexthopInfo which returns the pointer to the group nexthop
  // info.
  virtual ::util::Status GetBcmMultipathNexthopInfo(
      uint32 group_id, BcmMultipathNexthopInfo* info) const;

  // Adds a copy of an ACL table to the BcmTableManager.
  // Returns ERR_ENTRY_EXISTS if a table with the same ID already exists.
  virtual ::util::Status AddAclTable(AclTable table);

    // Gets a read-only pointer to an ACL table from the BcmTableManager.
  // Returns ERR_ENTRY_NOT_FOUND if the table cannot be found.
  // Returns ERR_INVALID_PARAM if the table is not an ACL table.
  virtual ::util::StatusOr<const AclTable*> GetReadOnlyAclTable(
      uint32 table_id) const;

  // Returns the set of all known ACL table ids.
  virtual std::set<uint32> GetAllAclTableIDs() const;

  // Delete all entries in a table and delete the table itself. This is the only
  // way to fully remove an ACL table.
  virtual ::util::Status DeleteTable(uint32 table_id);

  // Reads the ::p4::TableEntry(s) programmed in the given set of tables (given
  // by table_ids) on the node. If table_ids is empty, return all the entries
  // programmed on the node. Assembles a list of pointers to the returned
  // entries which have counters to be read.
  virtual ::util::Status ReadTableEntries(
      const std::set<uint32>& table_ids, ::p4::ReadResponse* resp,
      std::vector<::p4::TableEntry*>* acl_flows) const;

  // Finds the and returns the stored ::p4::TableEntry that matches the given
  // entry.
  virtual util::StatusOr<p4::TableEntry> LookupTableEntry(
      const ::p4::TableEntry& entry) const;

  // Reads the ::p4::ActionProfileMember(s) whose action_profile_id fields are
  // in action_profile_ids on the node. If action_profile_ids is empty, returns
  // all the ::p4::ActionProfileMember(s) programmed on the node.
  virtual ::util::Status ReadActionProfileMembers(
      const std::set<uint32>& action_profile_ids,
      WriterInterface<::p4::ReadResponse>* writer) const;

  // Reads the ::p4::ActionProfileGroup(s) whose action_profile_id fields are in
  // in action_profile_ids on the node. If action_profile_ids is empty, returns
  // all the ::p4::ActionProfileGroup(s) programmed on the node.
  virtual ::util::Status ReadActionProfileGroups(
      const std::set<uint32>& action_profile_ids,
      WriterInterface<::p4::ReadResponse>* writer) const;

  // Takes the input P4 table_entry for the given node and maps it to the output
  // flow_entry.
  virtual ::util::Status MapFlowEntry(const ::p4::TableEntry& table_entry,
                                      ::p4::Update::Type type,
                                      CommonFlowEntry* flow_entry) const;

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BcmTableManager> CreateInstance(
      const BcmChassisManager* bcm_chassis_manager,
      P4TableMapper* p4_table_mapper, int unit);

  // BcmTableManager is neither copyable nor movable.
  BcmTableManager(const BcmTableManager&) = delete;
  BcmTableManager& operator=(const BcmTableManager&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BcmTableManager();

 private:
  // Typedefs for ::p4::TableEntry storage.
  typedef stratum::gtl::flat_hash_set<::p4::TableEntry, TableEntryHash, TableEntryEqual>
      TableEntrySet;
  typedef stratum::gtl::flat_hash_map<uint32, TableEntrySet> TableIdToTableEntrySetMap;

  // Typedefs for ::p4::ActionProfileMember storage.
  typedef stratum::gtl::flat_hash_set<::p4::ActionProfileMember, ActionProfileMemberHash,
                             ActionProfileMemberEqual>
      ActionProfileMemberSet;
  typedef stratum::gtl::flat_hash_set<::p4::ActionProfileGroup, ActionProfileGroupHash,
                             ActionProfileGroupEqual>
      ActionProfileGroupSet;

  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BcmTableManager(const BcmChassisManager* bcm_chassis_manager,
                  P4TableMapper* p4_table_mapper, int unit);

  // Private helpers for mutating flow_ref_count for members and groups.
  ::util::Status UpdateFlowRefCountForMember(uint32 member_id, int delta);
  ::util::Status UpdateFlowRefCountForGroup(uint32 group_id, int delta);

  // Private helpers to get the pointers to nexthop info for members and groups.
  ::util::StatusOr<BcmNonMultipathNexthopInfo*> GetBcmNonMultipathNexthopInfo(
      uint32 member_id) const;
  ::util::StatusOr<BcmMultipathNexthopInfo*> GetBcmMultipathNexthopInfo(
      uint32 group_id) const;

  // Construct an egress port action from a port_id. Verify the port against the
  // node_id_. The bcm_action parameter type will indicate if the port is a
  // logical port or a trunk port.
  ::util::Status CreateEgressPortAction(uint64 port_id,
                                        BcmAction* bcm_action) const;

  // Convert a CommonFlowEntry.fields (MappedField) value to a BcmField.
  ::util::Status MappedFieldToBcmField(
      BcmFlowEntry::BcmTableType bcm_table_type,
      const MappedField& common_field, BcmField* bcm_field) const;

  // ConvertSendOrCopyToCpuAction focuses on four action types:
  //   CPU_QUEUE_ID, EGRESS_PORT, DROP, CLONE
  // Below are exact sets used to determine Copy/Send-to-CPU actions.
  //   Skip
  //     No CPU_QUEUE_ID + No CPU EGRESS_PORT +     Any DROP + No CLONE
  //   Copy-to-CPU combinations:
  //        CPU_QUEUE_ID + No CPU EGRESS_PORT +      No DROP +    CLONE
  //        CPU_QUEUE_ID + No CPU EGRESS_PORT + Colored DROP +    CLONE
  //   Send-to-CPU combinations:
  //        CPU_QUEUE_ID +    CPU EGRESS_PORT +      No DROP + No CLONE
  //        CPU_QUEUE_ID +    CPU EGRESS_PORT + Colored DROP + No CLONE
  //   Error
  //     All other cases.
  ::util::Status ConvertSendOrCopyToCpuAction(
      P4ActionFunction* action_function,
      std::vector<BcmAction>* bcm_actions) const;

  // Convert a nexthop action into BCM actions. Remove all actions used during
  // the conversion from the P4ActionFunction. A nexthop action is defined as an
  // action function with exactly the following:
  //   * An ethernet source action.
  //   * An ethernet destination action.
  //   * An egress port action.
  // A nexthop action is invalid if any of the following are true:
  //   * The ethernet source address is 0.
  //   * The ethernet destination address is 0.
  //   * The egress port is the CPU port.
  //   * The egress port is unknown.
  ::util::Status ConvertNexthopAction(
      P4ActionFunction* action_function,
      std::vector<BcmAction>* bcm_actions) const;

  // Perform complex action conversion for CommonFlowEntry to BcmFlowEntry.
  // Complex actions involve a many-to-many or many-to-one translation of P4
  // actions to BCM actions.
  // ConvertComplexP4Actions will remove all actions used during the conversion
  // from the P4ActionFunction.
  ::util::Status ConvertComplexP4Actions(
      P4ActionFunction* action_function,
      std::vector<BcmAction>* bcm_actions) const;

  // Private helper for translating P4 action fields to Bcm actions.
  ::util::Status P4ActionFieldToBcmAction(
      const P4ActionFunction::P4ActionFields& common_action,
      BcmAction* bcm_action) const;

  // Given a CommonFlowEntry, returns the corresponding BCM table type (BCM
  // specific).
  ::util::StatusOr<BcmFlowEntry::BcmTableType> GetBcmTableType(
      const CommonFlowEntry& common_flow_entry) const;

  // Returns a mutable reference to a flow table.
  // Returns ERR_ENTRY_NOT_FOUND if the lookup fails.
  util::StatusOr<BcmFlowTable*> GetMutableFlowTable(uint32 table_id);

  // Returns a constant reference to a flow table.
  // Returns ERR_ENTRY_NOT_FOUND if the lookup fails.
  util::StatusOr<const BcmFlowTable*> GetConstantFlowTable(
      uint32 table_id) const;

  // Returns true if this BcmTableManager has a table with the given table id.
  bool HasTable(uint32 table_id) const;

  // Returns true if the given table id refers to a known ACL table.
  bool IsAclTable(uint32 table_id) const;

  // ***************************************************************************
  // Port/trunk Maps
  // ***************************************************************************
  // Map from singleton port ID to its corresponding BCM logical_port on the
  // node/ASIC managed by this class. Set by PushChassisConfig().
  std::map<uint64, int> port_id_to_logical_port_;

  // Map from singleton port ID to its corresponding BCM trunk_port on the
  // node/ASIC managed by this class. Set by PushChassisConfig().
  std::map<uint64, int> trunk_id_to_trunk_port_;

  // ***************************************************************************
  // Nexthop Maps
  // ***************************************************************************

  // Map from member_id given by the controller to its corresponding
  // BcmNonMultipathNexthopInfo for the programmed member.
  stratum::gtl::flat_hash_map<uint32, BcmNonMultipathNexthopInfo*>
      member_id_to_nexthop_info_;

  // Map from group_id given by the controller to its corresponding
  // BcmMultipathNexthopInfo for the programmed ECMP/WCMP group.
  stratum::gtl::flat_hash_map<uint32, BcmMultipathNexthopInfo*>
      group_id_to_nexthop_info_;

  // Set of ECMP/WCMP groups programmed on the node.
  ActionProfileMemberSet members_;

  // Set of ECMP/WCMP groups programmed on the node.
  ActionProfileGroupSet groups_;

  // ***************************************************************************
  // Table Maps
  // ***************************************************************************

  // Map of generic flow tables indexed by table id.
  stratum::gtl::flat_hash_map<uint32, BcmFlowTable> generic_flow_tables_;

  // Map of ACL tables indexed by table id.
  stratum::gtl::flat_hash_map<uint32, AclTable> acl_tables_;

  // ***************************************************************************
  // Utilities
  // ***************************************************************************

  // Pointer to BcmChassisManager class to get the most updated node & port
  // maps after the config is pushed. THIS CLASS MUST NOT CALL ANY METHOD WHICH
  // CAN MODIFY THE STATE OF BcmChassisManager OBJECT.
  const BcmChassisManager* bcm_chassis_manager_;  // not owned by this class.

  // Pointer to P4TableMapper. In charge of PI to vender-agnostic entry mapping.
  P4TableMapper* p4_table_mapper_;  // not owned by this class.

  // Logical node ID corresponding to the node/ASIC managed by this class
  // instance. Assigned on PushChassisConfig() and might change during the
  // lifetime of the class.
  uint64 node_id_;

  // Fixed zero-based BCM unit number corresponding to the node/ASIC managed by
  // this class instance. Assigned in the class constructor.
  const int unit_;

  friend class BcmTableManagerTest;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_TABLE_MANAGER_H_
