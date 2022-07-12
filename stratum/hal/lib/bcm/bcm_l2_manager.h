// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BCM_BCM_L2_MANAGER_H_
#define STRATUM_HAL_LIB_BCM_BCM_L2_MANAGER_H_

#include <map>
#include <memory>
#include <tuple>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/strings/str_cat.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/bcm_chassis_ro_interface.h"
#include "stratum/hal/lib/bcm/bcm_sdk_interface.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/constants.h"

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

  // Inserts a MAC address + VLAN into the L2 FDB.
  virtual ::util::Status InsertL2Entry(const BcmFlowEntry& bcm_flow_entry);

  // Deletes a MAC address + VLAN from the L2 FDB. Fails if the entry does not
  // exists.
  virtual ::util::Status DeleteL2Entry(const BcmFlowEntry& bcm_flow_entry);

  virtual ::util::Status InsertL2VlanEntry(const BcmFlowEntry& bcm_flow_entry);

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
      BcmChassisRoInterface* bcm_chassis_ro_interface,
      BcmSdkInterface* bcm_sdk_interface, int unit);

  // BcmL2Manager is neither copyable nor movable.
  BcmL2Manager(const BcmL2Manager&) = delete;
  BcmL2Manager& operator=(const BcmL2Manager&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BcmL2Manager();

 private:
  //  An struct that encapsulates a my station TCAM entry.
  struct MyStationEntry {
    // The priority of the my station entry. We use two values:
    // 1- kL3PromoteMyStationEntryPriority: The priority used for the L3 promote
    //    entry for default VLAN (vlan = kDefaultVlan, vlan_mask = 0xfff,
    //    dst_mac = 0x0, dst_mac_mask = 0x010000000000).
    // 2- kRegularMyStationEntryPriority: The priority used for all the my
    //    station entries which typically match a specific dst MAC.
    // The default value is -1 to point to a uninitialized/invalid case. We
    // require the priority to be explicitly given for each entry.
    int priority;
    // The VLAN for my station entry and it's corresponding mask (12 bit max).
    // Typical valid entries are:
    // 1- A positive value with mask = 0xfff: A specific VLAN.
    // 2- A zero value with zero mask: all VLANs.
    // If vlan is not given for a my station TCAM entry, we assume the entry is
    // applied to all VLANs. Therefore we use zero as the default value of
    // vlan and vlan_mask.
    int vlan;
    int vlan_mask;
    // The dst MAC for the station entry and it's corresponding mask. Typical
    // valid entries are:
    // 1- A positive value with mask = 0xffffffffffff: A specific dst MAC.
    // 2- A zero value with mask 0x010000000000: all dst MAC, except multicast
    //    MAC, for cases where multicast are not allowed (for example for L3
    //    promote entry for default VLAN).
    // The default value for dst_mac_mask is 0x0 so that when mask
    // is not given the entry becomes a wildcard match.
    uint64 dst_mac;
    uint64 dst_mac_mask;
    MyStationEntry()
        : priority(-1),
          vlan(0),
          vlan_mask(0),
          dst_mac(0),
          dst_mac_mask(0xffffffffffffULL) {}
    MyStationEntry(int _priority, int _vlan, int _vlan_mask, uint64 _dst_mac,
                   uint64 _dst_mac_mask)
        : priority(_priority),
          vlan(_vlan),
          vlan_mask(_vlan_mask),
          dst_mac(_dst_mac),
          dst_mac_mask(_dst_mac_mask) {}
    bool operator<(const MyStationEntry& other) const {
      return (priority < other.priority ||
              (priority == other.priority &&
               (vlan < other.vlan ||
                (vlan == other.vlan &&
                 (vlan_mask < other.vlan_mask ||
                  (vlan_mask == other.vlan_mask &&
                   (dst_mac < other.dst_mac ||
                    (dst_mac == other.dst_mac &&
                     (dst_mac_mask < other.dst_mac_mask)))))))));
    }
    bool operator==(const MyStationEntry& other) const {
      return (priority == other.priority && vlan == other.vlan &&
              vlan_mask == other.vlan_mask && dst_mac == other.dst_mac &&
              dst_mac_mask == other.dst_mac_mask);
    }
    std::string ToString() const {
      return absl::StrCat("(priority:", priority, ", vlan:", vlan,
                          ", vlan_mask:", absl::Hex(vlan_mask),
                          ", dst_mac:", absl::Hex(dst_mac),
                          ", dst_mac_mask:", absl::Hex(dst_mac_mask), ")");
    }
  };

  // A struct that encapsulates a L2 FDB hash entry. Corresponds to the
  // L2_FDB_VLAN table.
  struct L2Entry {
    int vlan;
    uint64 dst_mac;
    int logical_port;
    int trunk_port;
    int l2_mcast_group_id;
    int class_id;
    bool copy_to_cpu;
    bool dst_drop;
    L2Entry()
        : vlan(0),
          dst_mac(0),
          logical_port(0),
          trunk_port(0),
          l2_mcast_group_id(0),
          class_id(0),
          copy_to_cpu(false),
          dst_drop(false) {}
    L2Entry(int _vlan, uint64 _dst_mac, int _logical_port, int _trunk_port,
            int _l2_mcast_group_id, int _class_id, bool _copy_to_cpu,
            bool _dst_drop)
        : vlan(_vlan),
          dst_mac(_dst_mac),
          logical_port(_logical_port),
          trunk_port(_trunk_port),
          l2_mcast_group_id(_l2_mcast_group_id),
          class_id(_class_id),
          copy_to_cpu(_copy_to_cpu),
          dst_drop(_dst_drop) {}
  };

  // A struct that encapsulates a L2 multicast entry. This is mapped to the
  // L2_MY_STATION table at the moment.
  struct L2MulticastEntry {
    int priority;
    int vlan;
    int vlan_mask;
    uint64 dst_mac;
    uint64 dst_mac_mask;
    bool copy_to_cpu;
    bool drop;
    uint8 l2_mcast_group_id;
    L2MulticastEntry()
        : priority(0),
          vlan(0),
          vlan_mask(0),
          dst_mac(0),
          dst_mac_mask(0),
          copy_to_cpu(false),
          drop(false),
          l2_mcast_group_id(0) {}
    L2MulticastEntry(int _priority, int _vlan, int _vlan_mask, uint64 _dst_mac,
                     uint64 _dst_mac_mask, bool _copy_to_cpu, bool _drop,
                     uint8 _l2_mcast_group_id)
        : priority(_priority),
          vlan(_vlan),
          vlan_mask(_vlan_mask),
          dst_mac(_dst_mac),
          dst_mac_mask(_dst_mac_mask),
          copy_to_cpu(_copy_to_cpu),
          drop(_drop),
          l2_mcast_group_id(_l2_mcast_group_id) {}
  };

  // The priority used for all the my station entries which are typically
  // match a specific dst MAC.
  static constexpr int kRegularMyStationEntryPriority = 100;
  // The priority used for the L3 promote entry for default VLAN.
  static constexpr int kL3PromoteMyStationEntryPriority = 1;
  // The priority used for software multicast entries.
  static constexpr int kSoftwareMulticastMyStationEntryPriority = 2;

  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BcmL2Manager(BcmChassisRoInterface* bcm_chassis_ro_interface,
               BcmSdkInterface* bcm_sdk_interface, int unit);

  // Configure a given VLAN based on the VlanConfig proto received from the
  // pushed config. Will not be called if there is not VlanConfig. vlan_id = 0
  // in the input VlanConfig proto is assumed to be the default VLAN.
  ::util::Status ConfigureVlan(const NodeConfigParams::VlanConfig& vlan_config);

  // Helper to validate a BcmFlowEntry given to update my station TCAM. Returns
  // a MyStationEntry struct corresponding to the entry after successful
  // parsing of the BcmFlowEntry.
  ::util::StatusOr<MyStationEntry> ValidateAndParseMyStationEntry(
      const BcmFlowEntry& bcm_flow_entry) const;

  // Helper to validate a BcmFlowEntry given to update the L2 FDB. Returns a
  // L2Entry struct corresponding to the entry after successful parsing of the
  // BcmFlowEntry.
  ::util::StatusOr<L2Entry> ValidateAndParseL2Entry(
      const BcmFlowEntry& bcm_flow_entry) const;

  // Helper to validate a BcmFlowEntry given to update the L2 multicast table.
  // Returns a L2MulticastEntry to the entry after successful
  // parsing of the BcmFlowEntry.
  ::util::StatusOr<L2MulticastEntry> ValidateAndParseL2MulticastEntry(
      const BcmFlowEntry& bcm_flow_entry) const;

  // Map from MyStationEntry structs, corresponding to the entries added to my
  // station TCAM, to their corresponding station ID returned by SDK.
  std::map<MyStationEntry, int> my_station_entry_to_station_id_;

  // Pointer to BcmChassisRoInterface class to get the most updated node & port
  // maps after the config is pushed. THIS CLASS MUST NOT CALL ANY METHOD WHICH
  // CAN MODIFY THE STATE OF BcmChassisRoInterface OBJECT.
  BcmChassisRoInterface* bcm_chassis_ro_interface_;  // not owned by this class.

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
