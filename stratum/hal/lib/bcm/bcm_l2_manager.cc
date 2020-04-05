// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bcm/bcm_l2_manager.h"

#include <set>

#include "absl/memory/memory.h"
#include "stratum/glue/integral_types.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/public/proto/p4_table_defs.pb.h"

namespace stratum {
namespace hal {
namespace bcm {

constexpr int BcmL2Manager::kRegularMyStationEntryPriority;
constexpr int BcmL2Manager::kL3PromoteMyStationEntryPriority;

BcmL2Manager::BcmL2Manager(BcmChassisRoInterface* bcm_chassis_ro_interface,
                           BcmSdkInterface* bcm_sdk_interface, int unit)
    : my_station_entry_to_station_id_(),
      bcm_chassis_ro_interface_(ABSL_DIE_IF_NULL(bcm_chassis_ro_interface)),
      bcm_sdk_interface_(ABSL_DIE_IF_NULL(bcm_sdk_interface)),
      node_id_(0),
      unit_(unit),
      l2_learning_disabled_for_default_vlan_(false) {}

BcmL2Manager::BcmL2Manager()
    : my_station_entry_to_station_id_(),
      bcm_chassis_ro_interface_(nullptr),
      bcm_sdk_interface_(nullptr),
      node_id_(0),
      unit_(-1),
      l2_learning_disabled_for_default_vlan_(false) {}

BcmL2Manager::~BcmL2Manager() {}

::util::Status BcmL2Manager::PushChassisConfig(const ChassisConfig& config,
                                               uint64 node_id) {
  node_id_ = node_id;  // Save node_id ASAP to ensure all the methods can refer
                       // to correct ID in the messages/errors.
  for (const auto& node : config.nodes()) {
    if (node.id() != node_id) continue;
    if (node.has_config_params()) {
      for (const auto& vlan_config : node.config_params().vlan_configs()) {
        RETURN_IF_ERROR(ConfigureVlan(vlan_config));
      }
      if (node.config_params().has_l2_config()) {
        // Set L2 age timer. If l2_age_duration_sec is not given (default 0)
        // timer will be disabled. If L2 learning is not enabled for a VLAN this
        // value is not important for that specific VLAN.
        RETURN_IF_ERROR(bcm_sdk_interface_->SetL2AgeTimer(
            unit_, node.config_params().l2_config().l2_age_duration_sec()));
      }
    }
    // TODO(unknown): Remove the unused VLANs. Keep track of IDs of the VLANs
    // in the config and remove all the VLANs that are configured and not used
    // (except the default VLAN).
    break;
  }

  return ::util::OkStatus();
}

::util::Status BcmL2Manager::VerifyChassisConfig(const ChassisConfig& config,
                                                 uint64 node_id) {
  if (node_id == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid node ID.";
  }
  if (node_id_ > 0 && node_id_ != node_id) {
    return MAKE_ERROR(ERR_REBOOT_REQUIRED)
           << "Detected a change in the node_id (" << node_id_ << " vs "
           << node_id << ").";
  }
  for (const auto& node : config.nodes()) {
    if (node.id() != node_id) continue;
    if (node.has_config_params()) {
      std::set<int> vlans = {};
      for (const auto& vlan_config : node.config_params().vlan_configs()) {
        int vlan =
            (vlan_config.vlan_id() > 0 ? vlan_config.vlan_id() : kDefaultVlan);
        CHECK_RETURN_IF_FALSE(vlan != kArpVlan)
            << "You specified config for the special ARP vlan " << kArpVlan
            << " on node " << node_id << ". This vlan is a special vlan with "
            << "fixed config which is added/removed based on whether L2 "
            << "learning is disabled for default vlan.";
        CHECK_RETURN_IF_FALSE(vlans.insert(vlan).second)
            << "Config for vlan " << vlan
            << " has been given more than once for node " << node_id << ".";
      }
      if (node.config_params().has_l2_config()) {
        CHECK_RETURN_IF_FALSE(
            node.config_params().l2_config().l2_age_duration_sec() >= 0)
            << "Invalid l2_age_duration_sec for node " << node_id << ": "
            << node.config_params().l2_config().l2_age_duration_sec() << ".";
      }
    }
    break;
  }

  return ::util::OkStatus();
}

::util::Status BcmL2Manager::Shutdown() {
  my_station_entry_to_station_id_.clear();
  return ::util::OkStatus();
}

::util::Status BcmL2Manager::InsertMyStationEntry(
    const BcmFlowEntry& bcm_flow_entry) {
  ASSIGN_OR_RETURN(const MyStationEntry& entry,
                   ValidateAndParseMyStationEntry(bcm_flow_entry));
  // If entry is already added for this unit, return success. If not try to add
  // it and update the my_station_entry_to_station_id_ map.
  if (my_station_entry_to_station_id_.count(entry)) {
    return ::util::OkStatus();
  }
  ASSIGN_OR_RETURN(int station_id,
                   bcm_sdk_interface_->AddMyStationEntry(
                       unit_, entry.priority, entry.vlan, entry.vlan_mask,
                       entry.dst_mac, entry.dst_mac_mask));
  my_station_entry_to_station_id_[entry] = station_id;

  return ::util::OkStatus();
}

::util::Status BcmL2Manager::DeleteMyStationEntry(
    const BcmFlowEntry& bcm_flow_entry) {
  ASSIGN_OR_RETURN(const MyStationEntry& entry,
                   ValidateAndParseMyStationEntry(bcm_flow_entry));
  // If entry has already been removed from this unit, return success. If not,
  // delete it and update the my_station_entry_to_station_id_ map.
  auto it = my_station_entry_to_station_id_.find(entry);
  if (it == my_station_entry_to_station_id_.end()) {
    return ::util::OkStatus();
  }
  RETURN_IF_ERROR(bcm_sdk_interface_->DeleteMyStationEntry(unit_, it->second));
  my_station_entry_to_station_id_.erase(entry);

  return ::util::OkStatus();
}

::util::Status BcmL2Manager::InsertL2Entry(const BcmFlowEntry& bcm_flow_entry) {
  ASSIGN_OR_RETURN(const L2Entry& entry,
                   ValidateAndParseL2Entry(bcm_flow_entry));

  RETURN_IF_ERROR(bcm_sdk_interface_->AddL2Entry(
      unit_, entry.vlan, entry.dst_mac, entry.logical_port, entry.trunk_port,
      entry.l2_mcast_group_id, entry.class_id, entry.copy_to_cpu,
      entry.dst_drop));

  return ::util::OkStatus();
}

// TODO(max): implement ModifyL2Entry if needed

::util::Status BcmL2Manager::DeleteL2Entry(const BcmFlowEntry& bcm_flow_entry) {
  ASSIGN_OR_RETURN(const L2Entry& entry,
                   ValidateAndParseL2Entry(bcm_flow_entry));
  RETURN_IF_ERROR(
      bcm_sdk_interface_->DeleteL2Entry(unit_, entry.vlan, entry.dst_mac));

  return ::util::OkStatus();
}

::util::Status BcmL2Manager::InsertMulticastGroup(
    const BcmFlowEntry& bcm_flow_entry) {
  // We will have two cases here:
  // 1- If MAC is broadcast MAC, we just enable broadcast for that VLAN. If
  //    VLAN does not exist, we add the VLAN containing all the ports. Note
  //    that enabling broadcast and/or creating VLAN can be done via config push
  //    as well. This method will override any setting done via config push.
  // 2- If MAC is not broadcast MAC, we return error as multicast is not
  //    supported yet.

  // TODO(unknown): At the moment this will be called for default/ARP VLAN and
  // broadcast MAC, as part of pushing the static entries in the forwarding
  // pipeline config. Enabling Broadcast MAC for these two are already done as
  // part of config push. So there is nothing to be done. If there is a use
  // case to do this for any other case we need to implement this method.
  ASSIGN_OR_RETURN(const L2MulticastEntry& entry,
                   ValidateAndParseL2MulticastEntry(bcm_flow_entry));
  RETURN_IF_ERROR(bcm_sdk_interface_->AddL2MulticastEntry(
      unit_, entry.priority, entry.vlan, entry.vlan_mask, entry.dst_mac,
      entry.dst_mac_mask, entry.copy_to_cpu, entry.drop,
      entry.l2_mcast_group_id));

  return ::util::OkStatus();
}

::util::Status BcmL2Manager::DeleteMulticastGroup(
    const BcmFlowEntry& bcm_flow_entry) {
  // We will have two cases here:
  // 1- If MAC is broadcast MAC, we just disable broadcast for that VLAN. This
  //    will fail if VLAN does not exist. This calls will also deletes the VLAN
  //    iff it is not the default VLAN. Note that disabling broadcast and/or
  //    deleting the VLAN can be done via config push as well. This method will
  //    override any setting done via config push.
  // 2- If MAC is not broadcast MAC, we return error as multicast is not
  //    supported yet.

  // TODO(unknown): At the moment this call is not even used as we do not
  // disable broadcast for default/ARP VLAN. If there is any other use case,
  // we need to implement this.
  ASSIGN_OR_RETURN(const L2MulticastEntry& entry,
                   ValidateAndParseL2MulticastEntry(bcm_flow_entry));
  RETURN_IF_ERROR(bcm_sdk_interface_->DeleteL2MulticastEntry(
      unit_, entry.vlan, entry.vlan_mask, entry.dst_mac, entry.dst_mac_mask));

  return ::util::OkStatus();
}

std::unique_ptr<BcmL2Manager> BcmL2Manager::CreateInstance(
    BcmChassisRoInterface* bcm_chassis_ro_interface,
    BcmSdkInterface* bcm_sdk_interface, int unit) {
  return absl::WrapUnique(
      new BcmL2Manager(bcm_chassis_ro_interface, bcm_sdk_interface, unit));
}

::util::Status BcmL2Manager::ConfigureVlan(
    const NodeConfigParams::VlanConfig& vlan_config) {
  int vlan = (vlan_config.vlan_id() > 0 ? vlan_config.vlan_id() : kDefaultVlan);
  // Create VLAN if it does not exist. When VLAN is created all the port
  // including CPU will be added to the member ports and all the ports excluding
  // CPU will be added to untagged member ports. Note that this VLAN is not
  // kArpVlan. We have already checked for this in verify stage.
  RETURN_IF_ERROR(bcm_sdk_interface_->AddVlanIfNotFound(unit_, vlan));
  RETURN_IF_ERROR(bcm_sdk_interface_->ConfigureVlanBlock(
      unit_, vlan, vlan_config.block_broadcast(),
      vlan_config.block_known_multicast(),
      vlan_config.block_unknown_multicast(),
      vlan_config.block_unknown_unicast()));
  RETURN_IF_ERROR(bcm_sdk_interface_->ConfigureL2Learning(
      unit_, vlan, vlan_config.disable_l2_learning()));

  if (vlan_config.disable_l2_learning()) {
    // Remove all the previously learnt MACs. If there is nothing learnt, this
    // call is a NOOP.
    RETURN_IF_ERROR(bcm_sdk_interface_->DeleteL2EntriesByVlan(unit_, vlan));
  }

  if (vlan == kDefaultVlan) {
    // Default vlan is a special vlan. If for some reason we diable L2 learning,
    // we need to make sure packets in this vlan, except multicast packet, are
    // all sent to L3 by default. We also need to still create special vlans
    // for applications that still need L2 learning (e.g. ARP). This is
    // fundamental for ensuring switch works as expected.
    MyStationEntry entry(kL3PromoteMyStationEntryPriority, kDefaultVlan, 0xfff,
                         0ULL, kNonMulticastDstMacMask);
    if (vlan_config.disable_l2_learning()) {
      // Add an my station entry for promoting L2 packets to L3 if not added
      // before for default vlan.
      if (!my_station_entry_to_station_id_.count(entry)) {
        ASSIGN_OR_RETURN(
            int station_id,
            bcm_sdk_interface_->AddMyStationEntry(
                unit_, kL3PromoteMyStationEntryPriority, kDefaultVlan, 0xfff,
                0ULL, kNonMulticastDstMacMask));
        my_station_entry_to_station_id_[entry] = station_id;
      }
      // Create a specific vlan for ARP (if it does not exist) where L2
      // learning and broadcast are enabled.
      RETURN_IF_ERROR(bcm_sdk_interface_->AddVlanIfNotFound(unit_, kArpVlan));
      RETURN_IF_ERROR(bcm_sdk_interface_->ConfigureVlanBlock(
          unit_, kArpVlan, false, false, true, true));
      RETURN_IF_ERROR(
          bcm_sdk_interface_->ConfigureL2Learning(unit_, kArpVlan, false));
      l2_learning_disabled_for_default_vlan_ = true;
    } else {
      // Remove the my station entry for promoting L2 packets to L3. We dont
      // need this any more as L2 learning has been enabled for this node.
      auto it = my_station_entry_to_station_id_.find(entry);
      if (it != my_station_entry_to_station_id_.end()) {
        RETURN_IF_ERROR(
            bcm_sdk_interface_->DeleteMyStationEntry(unit_, it->second));
        my_station_entry_to_station_id_.erase(entry);
      }
      // Delete the specific ARP vlan (if it exists).
      RETURN_IF_ERROR(bcm_sdk_interface_->DeleteVlanIfFound(unit_, kArpVlan));
      l2_learning_disabled_for_default_vlan_ = false;
    }
  }

  return ::util::OkStatus();
}

::util::StatusOr<BcmL2Manager::MyStationEntry>
BcmL2Manager::ValidateAndParseMyStationEntry(
    const BcmFlowEntry& bcm_flow_entry) const {
  // Initial validation.
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.bcm_table_type() ==
                        BcmFlowEntry::BCM_TABLE_MY_STATION)
      << "Invalid table_id for node " << node_id_ << ": "
      << BcmFlowEntry::BcmTableType_Name(bcm_flow_entry.bcm_table_type())
      << ", found in " << bcm_flow_entry.ShortDebugString() << ".";
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.unit() == unit_)
      << "Received BcmFlowEntry for wrong unit " << unit_ << " on node "
      << node_id_ << ": " << bcm_flow_entry.ShortDebugString() << ".";
  // We expect no action in bcm_flow_entry.
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.actions_size() == 0)
      << "Received entry with action for node " << node_id_ << ": "
      << bcm_flow_entry.ShortDebugString() << ".";
  // We do not expect any field other than vlan, dst_mac, and their masks.
  int vlan = 0, vlan_mask = 0;
  uint64 dst_mac = 0;
  uint64 dst_mac_mask = 0;  // P4RT specifies missing mask as don't care
  for (const auto& field : bcm_flow_entry.fields()) {
    if (field.type() == BcmField::ETH_DST) {
      dst_mac = field.value().u64();
      if (field.has_mask()) {
        dst_mac_mask = field.mask().u64();
      }
      // We do not expect broadcast MAC as an entry here.
      CHECK_RETURN_IF_FALSE(dst_mac != kBroadcastMac)
          << "Received entry with ETH_DST set to broadcast MAC for node "
          << node_id_ << ": " << bcm_flow_entry.ShortDebugString() << ".";
    } else if (field.type() == BcmField::VLAN_VID) {
      // Note: we should not never translate vlan = 0 to vlan = kDefaultVlan.
      // We let the controller decide on the values.
      vlan = static_cast<int>(field.value().u32());
      if (field.has_mask()) {
        vlan_mask = static_cast<int>(field.mask().u32());
      }
    } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Received fields othere than ETH_DST and VLAN_VID for node "
             << node_id_ << ": " << bcm_flow_entry.ShortDebugString() << ".";
    }
  }
  if (vlan > 0 && vlan_mask == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Detected vlan > 0 while vlan_mask is either not given or is 0 "
           << "for node " << node_id_ << ": "
           << bcm_flow_entry.ShortDebugString() << ".";
  }
  // If controller tries to program a flow which is exactly the same as the
  // L3 promote entry, we use kL3PromoteMyStationEntryPriority as the priority.
  // For any other case, we use kRegularMyStationEntryPriority as the priority.
  int priority = (vlan == kDefaultVlan && vlan_mask == 0xfff &&
                  dst_mac == 0ULL && dst_mac_mask == kNonMulticastDstMacMask)
                     ? kL3PromoteMyStationEntryPriority
                     : kRegularMyStationEntryPriority;

  return MyStationEntry(priority, vlan, vlan_mask, dst_mac, dst_mac_mask);
}

::util::StatusOr<BcmL2Manager::L2Entry> BcmL2Manager::ValidateAndParseL2Entry(
    const BcmFlowEntry& bcm_flow_entry) const {
  // Initial validation.
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.bcm_table_type() ==
                        BcmFlowEntry::BCM_TABLE_L2_UNICAST)
      << "Invalid table_id for node " << node_id_ << ": "
      << BcmFlowEntry::BcmTableType_Name(bcm_flow_entry.bcm_table_type())
      << ", found in " << bcm_flow_entry.ShortDebugString() << ".";
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.unit() == unit_)
      << "Received BcmFlowEntry for wrong unit " << unit_ << " on node "
      << node_id_ << ": " << bcm_flow_entry.ShortDebugString() << ".";

  CHECK_RETURN_IF_FALSE(bcm_flow_entry.fields_size() <= 2)
      << "Received BcmFlowEntry with missing fields: "
      << bcm_flow_entry.ShortDebugString() << ".";

  int vlan = 0;
  uint64 dst_mac = 0;
  for (const auto& field : bcm_flow_entry.fields()) {
    if (field.type() == BcmField::ETH_DST) {
      dst_mac = field.value().u64();
      // L2 FDB is exact match
      CHECK_RETURN_IF_FALSE(!field.has_mask())
          << "Received entry with "
          << "ETH_DST mask for L2 FDB for node " << node_id_ << ": "
          << bcm_flow_entry.ShortDebugString() << ".";
    } else if (field.type() == BcmField::VLAN_VID) {
      // Note: we should not never translate vlan = 0 to vlan = kDefaultVlan.
      // We let the controller decide on the values.
      vlan = static_cast<int>(field.value().u32());
      CHECK_RETURN_IF_FALSE(!field.has_mask())
          << "Received entry with "
          << "VLAN_VID mask for L2 FDB for node " << node_id_ << ": "
          << bcm_flow_entry.ShortDebugString() << ".";
    } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Received fields other than ETH_DST and VLAN_VID for node "
             << node_id_ << ": " << bcm_flow_entry.ShortDebugString() << ".";
    }
  }

  int logical_port = 0;
  int trunk_port = 0;
  int l2_mcast_group_id = 0;
  int class_id = 0;
  bool copy_to_cpu = false;
  bool dst_drop = false;
  for (const auto& action : bcm_flow_entry.actions()) {
    switch (action.type()) {
      case BcmAction::DROP: {
        if (action.params_size() != 0) {
          MAKE_ERROR(ERR_INVALID_PARAM)
              << "Invalid drop action. "
              << "Expected no parameters in: " << action.ShortDebugString();
        }
        dst_drop = true;
        break;
      }
      case BcmAction::OUTPUT_PORT:
      case BcmAction::OUTPUT_TRUNK:
      case BcmAction::SET_L2_MCAST_GROUP: {
        if (action.params_size() != 1) {
          MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid action. "
                                        << "Expected exactly one parameter in: "
                                        << action.ShortDebugString();
        }
        for (const auto& param : action.params()) {
          if (param.type() == BcmAction::Param::LOGICAL_PORT) {
            logical_port = param.value().u32();
          } else if (param.type() == BcmAction::Param::TRUNK_PORT) {
            trunk_port = param.value().u32();
          } else if (param.type() == BcmAction::Param::L2_MCAST_GROUP_ID) {
            l2_mcast_group_id = param.value().u32();
          } else {
            return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Invalid action parameter "
                   << "in: " << action.ShortDebugString();
          }
        }
        break;
      }
      case BcmAction::SET_VFP_DST_CLASS_ID: {
        CHECK_RETURN_IF_FALSE(action.params_size() == 1 ||
                              action.params(0).type() !=
                                  BcmAction::Param::VFP_DST_CLASS_ID)
            << "Expected exactly one parameter of type VFP_DST_CLASS_ID for "
            << "action of type SET_VFP_DST_CLASS_ID: "
            << bcm_flow_entry.ShortDebugString() << ".";
        class_id = action.params(0).value().u32();
      }
      default:
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Invalid action type: " << bcm_flow_entry.ShortDebugString()
               << ".";
    }
  }

  return L2Entry(vlan, dst_mac, logical_port, trunk_port, l2_mcast_group_id,
                 class_id, copy_to_cpu, dst_drop);
}

::util::StatusOr<BcmL2Manager::L2MulticastEntry>
BcmL2Manager::ValidateAndParseL2MulticastEntry(
    const BcmFlowEntry& bcm_flow_entry) const {
  // Initial validation.
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.bcm_table_type() ==
                        BcmFlowEntry::BCM_TABLE_L2_MULTICAST)
      << "Invalid table_type for node " << node_id_ << ": "
      << BcmFlowEntry::BcmTableType_Name(bcm_flow_entry.bcm_table_type())
      << ", found in " << bcm_flow_entry.ShortDebugString() << ".";
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.unit() == unit_)
      << "Received BcmFlowEntry for wrong unit " << unit_ << " on node "
      << node_id_ << ": " << bcm_flow_entry.ShortDebugString() << ".";
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.fields_size() == 2)
      << "Received BcmFlowEntry with missing fields: "
      << bcm_flow_entry.ShortDebugString() << ".";
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.actions_size() <= 2)
      << "Received entry with more than 2 actions for node " << node_id_ << ": "
      << bcm_flow_entry.ShortDebugString() << ".";

  int vlan = 0;
  int vlan_mask = 0;
  uint64 dst_mac = 0;
  uint64 dst_mac_mask = 0;
  for (const auto& field : bcm_flow_entry.fields()) {
    if (field.type() == BcmField::ETH_DST) {
      dst_mac = field.value().u64();
      if (field.has_mask()) {
        dst_mac_mask = field.mask().u64();
      } else {
        dst_mac_mask = kBroadcastMac;
      }
      CHECK_RETURN_IF_FALSE(dst_mac_mask == kBroadcastMac)
          << "Received invalid ethernet destination MAC address mask. "
          << "Current implementation of L2 multicast only allows exact "
          << "matches: " << bcm_flow_entry.ShortDebugString() << ".";
    } else if (field.type() == BcmField::VLAN_VID) {
      // Note: we should not never translate vlan = 0 to vlan = kDefaultVlan.
      // We let the controller decide on the values.
      vlan = static_cast<int>(field.value().u32());
      if (field.has_mask()) {
        vlan_mask = static_cast<int>(field.mask().u32());
      }
    } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Received fields other than ETH_DST and VLAN_VID for node "
             << node_id_ << ": " << bcm_flow_entry.ShortDebugString() << ".";
    }
  }

  bool copy_to_cpu = false;
  bool drop = false;
  int l2_mcast_group_id = 0;
  for (const auto& action : bcm_flow_entry.actions()) {
    switch (action.type()) {
      case BcmAction::DROP: {
        if (action.params_size() != 0) {
          MAKE_ERROR(ERR_INVALID_PARAM)
              << "Invalid drop action. "
              << "Expected no parameters in: " << action.ShortDebugString();
        }
        drop = true;
        break;
      }
      case BcmAction::COPY_TO_CPU: {
        CHECK_RETURN_IF_FALSE(action.params_size() == 0)
            << "Expected no parameters for action of type COPY_TO_CPU: "
            << bcm_flow_entry.ShortDebugString() << ".";
        copy_to_cpu = true;
        break;
      }
      case BcmAction::SET_L2_MCAST_GROUP: {
        if (action.params_size() != 1) {
          MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid action. "
                                        << "Expected exactly one parameter in: "
                                        << action.ShortDebugString();
        }
        for (const auto& param : action.params()) {
          if (param.type() == BcmAction::Param::L2_MCAST_GROUP_ID) {
            l2_mcast_group_id = param.value().u32();
          } else {
            return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Invalid action parameter "
                   << "in: " << action.ShortDebugString();
          }
        }
        break;
      }
      default:
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Invalid action type: " << bcm_flow_entry.ShortDebugString()
               << ".";
    }
  }

  int priority = bcm_flow_entry.priority() > 0
                     ? bcm_flow_entry.priority()
                     : kSoftwareMulticastMyStationEntryPriority;

  return L2MulticastEntry(priority, vlan, vlan_mask, dst_mac, dst_mac_mask,
                          copy_to_cpu, drop, l2_mcast_group_id);
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
