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


#ifndef STRATUM_HAL_LIB_BCM_BCM_L2_MANAGER_H_
#define STRATUM_HAL_LIB_BCM_BCM_L2_MANAGER_H_

#include <map>
#include <memory>
#include <tuple>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"
#include "stratum/hal/lib/bcm/bcm_sdk_interface.h"
#include "stratum/public/proto/hal.grpc.pb.h"
#include "stratum/glue/net_util/integral_types.h"
#include "absl/base/thread_annotations.h"

namespace stratum {
namespace hal {
namespace bcm {

// The "BcmL2Manager" class implements the L2 routing functionality.
class BcmL2Manager {
 public:
  virtual ~BcmL2Manager();

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

  // Inserts a MAC address to My Station TCAM. Will not fail if the entry
  // already exists.
  virtual ::util::Status InsertMyStationEntry(
      const BcmFlowEntry& bcm_flow_entry);

  // Deletes a MAC address from My Station TCAM. Fails if the entry to remove
  // does not exist.
  virtual ::util::Status DeleteMyStationEntry(
      const BcmFlowEntry& bcm_flow_entry);

  // Creates an L2 multicast or broadcast group. Each multicast or broadcast
  // group is specified by a multicast_group_id given by an action of type
  // SET_L2_MCAST_GROUP which has an action param of type L2_MCAST_GROUP_ID.
  // This multicast_group_id is then used by P4 runtime for adding/modifying/
  // removing members for L2 multicast group. The stack handles broadcast and
  // multicast groups differently, although P4 handles them similarly:
  // - If dst_mac == kBroadcastMac: This is a broadcast group. We check that
  //   vlan_id > 0 and vlan_id == multicast_group_id (i.e. broadcast uses the
  //   vlan_id as its multicast_group_id). In this case we try to create a
  //   VLAN based on the given vlan_id and enable broadcast for that. This vlan
  //   by default has all the ports in it. It should be noted that:
  //   1) Creating broadcast group for vlan_id = kDefaultVlan is NOOP.
  //   2) Creating broadcast group for vlan_id = kArpVlan is NOOP if L2 learning
  //      has been disabled for default VLAN and not permitted otherwise.
  //      kArpVlan is a special VLAN and is treated differently.
  // - If dst_mac != kBroadcastMac: This is a multicast group. We check that
  //   multicast_group_id > 0 and it is unique (i.e. used as id for any
  //   multicast or broadcast group). For the case of Broadcom, vlan_id can be
  //   arbitrary. When we specify the members for multicast group and the packet
  //   is routed to that group, the packet will be casted to members of the
  //   multicast group which are also part of the VLAN that the packet is part
  //   of. Anyways in this case we create a multicast group on BCM (i.e. we
  //   add an entry in the multicast table) using the multicast_group_id as
  //   `l2mc_index` of the entry directly. Since we dont know the ports that
  //   are part of this multicast group, the pbmp initially is empty. We will
  //   wait for P4Runtime to add members to the group later. Also, in order to
  //   route the packets to the multcat group, an entry with the given dst_mac
  //   is added to the L2 table which points to the multicast group index.
  // In both cases internal maps are updated to keep track of the groups added.
  virtual ::util::Status InsertMulticastGroup(
      const BcmFlowEntry& bcm_flow_entry);

  // Deletes an already created multicast or broadcast group given its
  // multicast_group_id given by an action of type SET_L2_MCAST_GROUP which has
  // an action param of type L2_MCAST_GROUP_ID. The stack handles broadcast and
  // multicast groups differently, although P4 handles them similarly:
  // - If dst_mac == kBroadcastMac: This is a broadcast group. We check that
  //   vlan_id > 0 and vlan_id == multicast_group_id (i.e. broadcast uses the
  //   vlan_id as its multicast_group_id). We then try to remove the VLAN based
  //   on the given vlan_id. If such VLAN does not exist we return error. It
  //   should be noted that:
  //   1) Deleting broadcast group for vlan_id = kDefaultVlan is not permitted.
  //   2) Creating broadcast group for vlan_id = kArpVlan is NOOP if L2 learning
  //      has been enabled for default VLAN and not permitted otherwise.
  //      kArpVlan is a special VLAN and is treated differently.
  // - If dst_mac != kBroadcastMac: This is a multicast group. We check that
  //   multicast_group_id > 0. For the case of Broadcom, vlan_id is arbitrary.
  //   The group is then deleted based on the given multicast_group_id from
  //   internal maps and the corresponding HW resources are freed.
  virtual ::util::Status DeleteMulticastGroup(
      const BcmFlowEntry& bcm_flow_entry);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BcmL2Manager> CreateInstance(
      BcmChassisManager* bcm_chassis_manager,
      BcmSdkInterface* bcm_sdk_interface, int unit);

  // BcmL2Manager is neither copyable nor movable.
  BcmL2Manager(const BcmL2Manager&) = delete;
  BcmL2Manager& operator=(const BcmL2Manager&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BcmL2Manager();

 private:
  static constexpr int kRegularMyStationEntryPriority = 100;
  static constexpr int kPromotedMyStationEntryPriority = 1;

  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BcmL2Manager(BcmChassisManager* bcm_chassis_manager,
               BcmSdkInterface* bcm_sdk_interface, int unit);

  // Configure a given VLAN based on the VlanConfig proto received from the
  // pushed config. Will not be called if there is not VlanConfig. vlan_id = 0
  // in the input VlanConfig proto is assumed to be the default VLAN.
  ::util::Status ConfigureVlan(const NodeConfigParams::VlanConfig& vlan_config);

  // Helper to validate a BcmFlowEntry given to update my station TCAM. Returns
  // the tuple of (vlan, dst_mac, priority) after successful parsing of the
  // BcmFlowEntry.
  ::util::StatusOr<std::tuple<int, uint64, int>> ValidateAndParseMyStationEntry(
      const BcmFlowEntry& bcm_flow_entry) const;

  // Map from (vlan, dst_mac, priority) of the entries added to my station TCAM
  // of all the units to their corresponding station ID returned by SDK.
  std::map<std::tuple<int, uint64, int>, int>
      vlan_dst_mac_priority_to_station_id_;

  // Pointer to BcmChassisManager class to get the most updated node & port
  // maps after the config is pushed. THIS CLASS MUST NOT CALL ANY METHOD WHICH
  // CAN MODIFY THE STATE OF BcmChassisManager OBJECT.
  BcmChassisManager* bcm_chassis_manager_;  // not owned by this class.

  // Pointer to a BcmSdkInterface implementation that wraps all the SDK calls.
  BcmSdkInterface* bcm_sdk_interface_;  // not owned by this class.

  // Logical node ID corresponding to the node/ASIC managed by this class
  // instance. Assigned on PushChassisConfig() and might change during the
  // lifetime of the class.
  uint64 node_id_;

  // Fixed zero-based BCM unit number corresponding to the node/ASIC managed by
  // this class instance. Assigned in the class constructor.
  const int unit_;

  // A boolean that shows whether L2 learning has been disabled for default
  // VLAN and the special ARP VLAN has been created. This is set/reset as part
  // of config push only.
  bool l2_learning_disabled_for_default_vlan_;

  friend class BcmL2ManagerTest;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_L2_MANAGER_H_
