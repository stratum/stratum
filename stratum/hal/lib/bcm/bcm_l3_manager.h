// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_BCM_BCM_L3_MANAGER_H_
#define STRATUM_HAL_LIB_BCM_BCM_L3_MANAGER_H_

#include <memory>
#include <utility>
#include <string>
#include <vector>

#include "stratum/glue/integral_types.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/bcm_sdk_interface.h"
#include "stratum/hal/lib/bcm/bcm_table_manager.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace bcm {

// This struct encapsulates the key for an IPv4/IPv6 LPM/host flow.
struct LpmOrHostKey {
  // The VRF. If not given, default VRF will be used.
  int vrf;
  // IPv4 subnet/mask.
  uint32 subnet_ipv4;
  uint32 mask_ipv4;
  // IPv6 subnet/mask.
  std::string subnet_ipv6;
  std::string mask_ipv6;
  LpmOrHostKey()
      : vrf(kVrfDefault),
        subnet_ipv4(0),
        mask_ipv4(0),
        subnet_ipv6(""),
        mask_ipv6("") {}
};

// This struct encapsulates the action params for an LPM/host flow.
struct LpmOrHostActionParams {
  // The value of class ID to set in the packet when it matches the LPM/host
  // flow. If non-positive, it will be ignored.
  int class_id;
  // Egress intf ID for the nexthop.
  int egress_intf_id;
  // A boolean determining whether the nexthop is an ECMP/WCMP group
  bool is_intf_multipath;
  LpmOrHostActionParams()
      : class_id(-1), egress_intf_id(-1), is_intf_multipath(false) {}
};

// The "BcmL3Manager" class implements the L3 routing functionality.
class BcmL3Manager {
 public:
  virtual ~BcmL3Manager();

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

  // Performs coldboot shutdown. Note that there is no public Initialize().
  // Initialization is done as part of PushChassisConfig() if the class is not
  // initialized by the time we push config.
  virtual ::util::Status Shutdown();

  // Finds or creates an egress non-multipath nexthop and returns its egress
  // intf ID. Note that it is perfectly OK for multiple group members to point
  // to the same egress intf ID, so we need to make sure if the egress intf
  // is already there we just return its ID without returning error.
  virtual ::util::StatusOr<int> FindOrCreateNonMultipathNexthop(
      const BcmNonMultipathNexthop& nexthop);

  // Finds or creates an egress multipath (ECMP/WCMP) nexthop and returns its
  // egress intf ID. Note that it is perfectly OK for multiple groups to point
  // to the same egress intf ID, so we need to make sure if the egress intf
  // is already there we just return its ID without returning error.
  virtual ::util::StatusOr<int> FindOrCreateMultipathNexthop(
      const BcmMultipathNexthop& nexthop);

  // Modifies an existing egress non-multipath nexthop given its ID. The same
  // egress ID will point to a new nexthop using this method.
  virtual ::util::Status ModifyNonMultipathNexthop(
      int egress_intf_id, const BcmNonMultipathNexthop& nexthop);

  // Modifies an existing egress multipath (ECMP/WCMP) nexthop given its ID
  // with a new set of members given in BcmMultipathNexthop.
  virtual ::util::Status ModifyMultipathNexthop(
      int egress_intf_id, const BcmMultipathNexthop& nexthop);

  // Deletes an egress non-multipath nexthop given its ID.
  virtual ::util::Status DeleteNonMultipathNexthop(int egress_intf_id);

  // Deletes an egress multipath (ECMP/WCMP) nexthop given its ID.
  virtual ::util::Status DeleteMultipathNexthop(int egress_intf_id);

  // Inserts an IPv4/IPv6 L3 LPM/Host flow. The function programs the
  // low level routes into the given unit based on the given P4 TableEntry.
  virtual ::util::Status InsertTableEntry(const ::p4::v1::TableEntry& entry);

  // Modifies an IPv4/IPv6 L3 LPM/Host flow. The function programs the
  // low level routes into the given unit based on the given P4 TableEntry. The
  // fields populated in P4 TableEntry are the same as the ones populated when
  // adding the flow in InsertLpmOrHostFlow().
  virtual ::util::Status ModifyTableEntry(const ::p4::v1::TableEntry& entry);

  // Deletes an IPv4/IPv6 L3 LPM/Host flow. The fields populated in the
  // P4 TableEntry define the key for the flow (the egress_intf_id or class_id
  // not needed).
  virtual ::util::Status DeleteTableEntry(const ::p4::v1::TableEntry& entry);

  // Updates any ECMP/WCMP groups which include a member pointing to the given
  // singleton port. Adds or removes the port to or from all groups referencing
  // it based on whether the port is UP or not, respectively. In the case that
  // a group becomes empty, a drop egress interface will be substituted in
  // as the SDK does not support ECMP groups programmed with no nexthops.
  virtual ::util::Status UpdateMultipathGroupsForPort(uint32 port_id);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BcmL3Manager> CreateInstance(
      BcmSdkInterface* bcm_sdk_interface, BcmTableManager* bcm_table_manager,
      int unit);

  // BcmL3Manager is neither copyable nor movable.
  BcmL3Manager(const BcmL3Manager&) = delete;
  BcmL3Manager& operator=(const BcmL3Manager&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BcmL3Manager();

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BcmL3Manager(BcmSdkInterface* bcm_sdk_interface,
               BcmTableManager* bcm_table_manager, int unit);

  // Inserts an IPv4/IPv6 L3 LPM/Host flow. The function programs the
  // low level routes into the given unit based on the given BcmFlowEntry.
  ::util::Status InsertLpmOrHostFlow(const BcmFlowEntry& bcm_flow_entry);

  // Modifies an IPv4/IPv6 L3 LPM/Host flow. The function programs the
  // low level routes into the given unit based on the given BcmFlowEntry. The
  // fields populated in BcmFlowEntry are the same as the ones populated when
  // adding the flow in InsertLpmOrHostFlow().
  ::util::Status ModifyLpmOrHostFlow(const BcmFlowEntry& bcm_flow_entry);

  // Deletes an IPv4/IPv6 L3 LPM/Host flow. The fields populated in BcmFlowEntry
  // define the key for the flow (the egress_intf_id or class_id not needed).
  ::util::Status DeleteLpmOrHostFlow(const BcmFlowEntry& bcm_flow_entry);

  // Helper to extract IPv4/IPv6 L3 LPM/Host flow keys given BcmFlowEntry.
  ::util::Status ExtractLpmOrHostKey(const BcmFlowEntry& bcm_flow_entry,
                                     LpmOrHostKey* key);

  // Helper to extract IPv4/IPv6 L3 LPM/Host flow actions given BcmFlowEntry.
  ::util::Status ExtractLpmOrHostActionParams(
      const BcmFlowEntry& bcm_flow_entry, LpmOrHostActionParams* action_params);

  // A helper to find the sorted vector of the member egress intf ids of an
  // ECMP group. The output vector is going to have the following format:
  // [a,...,a,b,...,b,c,...,c,...] where each egress intf id is repeated based
  // on its weight.
  ::util::StatusOr<std::vector<int>> FindEcmpGroupMembers(
      const BcmMultipathNexthop& nexthop);

  // Helpers for incrementing/decrementing the ref count for a router intf. In
  // case router intf has zero ref count, DecrementRefCount() will cleanup the
  // router intf from SDK as well.
  ::util::Status IncrementRefCount(int router_intf_id);
  ::util::Status DecrementRefCount(int router_intf_id);

  // Map from router_intf_id to ref counts (the number egress intfs pointing to
  // this router intf).
  // TODO(unknown): We keep this map as there is no good way to get this
  // directly from SDK. Investigate.
  absl::flat_hash_map<int, uint32> router_intf_ref_count_;

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

  // Default L3 drop interface ID used as a placeholder when an ECMP group has
  // less than 2 active members due to port down events.
  int default_drop_intf_;

  friend class BcmL3ManagerTest;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_L3_MANAGER_H_
