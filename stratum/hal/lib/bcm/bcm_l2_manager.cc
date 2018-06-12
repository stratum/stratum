// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "stratum/hal/lib/bcm/bcm_l2_manager.h"

#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "stratum/glue/integral_types.h"
#include "absl/memory/memory.h"

namespace stratum {
namespace hal {
namespace bcm {

constexpr int BcmL2Manager::kRegularMyStationEntryPriority;
constexpr int BcmL2Manager::kPromotedMyStationEntryPriority;

BcmL2Manager::BcmL2Manager(BcmChassisManager* bcm_chassis_manager,
                           BcmSdkInterface* bcm_sdk_interface, int unit)
    : vlan_dst_mac_priority_to_station_id_(),
      bcm_chassis_manager_(CHECK_NOTNULL(bcm_chassis_manager)),
      bcm_sdk_interface_(CHECK_NOTNULL(bcm_sdk_interface)),
      node_id_(0),
      unit_(unit),
      l2_learning_disabled_for_default_vlan_(false) {}

BcmL2Manager::BcmL2Manager()
    : vlan_dst_mac_priority_to_station_id_(),
      bcm_chassis_manager_(nullptr),
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
    // TODO: Remove the unused VLANs. Keep track of IDs of the VLANs
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
  vlan_dst_mac_priority_to_station_id_.clear();
  return ::util::OkStatus();
}

::util::Status BcmL2Manager::InsertMyStationEntry(
    const BcmFlowEntry& bcm_flow_entry) {
  ASSIGN_OR_RETURN(auto vlan_dst_mac_priority,
                   ValidateAndParseMyStationEntry(bcm_flow_entry));
  // If (vlan, dst_mac, priority) tuple is already added for the given unit,
  // return. If not try to add it and update the internal
  // vlan_dst_mac_priority_to_station_id_ map.
  if (vlan_dst_mac_priority_to_station_id_.count(vlan_dst_mac_priority)) {
    return ::util::OkStatus();
  }
  ASSIGN_OR_RETURN(int station_id,
                   bcm_sdk_interface_->AddMyStationEntry(
                       unit_, std::get<0>(vlan_dst_mac_priority),
                       std::get<1>(vlan_dst_mac_priority),
                       std::get<2>(vlan_dst_mac_priority)));
  vlan_dst_mac_priority_to_station_id_[vlan_dst_mac_priority] = station_id;

  return ::util::OkStatus();
}

::util::Status BcmL2Manager::DeleteMyStationEntry(
    const BcmFlowEntry& bcm_flow_entry) {
  ASSIGN_OR_RETURN(auto vlan_dst_mac_priority,
                   ValidateAndParseMyStationEntry(bcm_flow_entry));
  // If (vlan, dst_mac, priority) tuple is not known for this unit, return
  // error. If not, delete it and update the internal
  // vlan_dst_mac_priority_to_station_id_ map.
  CHECK_RETURN_IF_FALSE(
      vlan_dst_mac_priority_to_station_id_.count(vlan_dst_mac_priority))
      << "Unknown (vlan, dst_mac, priority) in my station TCAM for node "
      << node_id_ << ": (" << std::get<0>(vlan_dst_mac_priority) << ", "
      << std::get<1>(vlan_dst_mac_priority) << ", "
      << std::get<2>(vlan_dst_mac_priority) << "), found in "
      << bcm_flow_entry.ShortDebugString() << ".";
  int station_id = vlan_dst_mac_priority_to_station_id_[vlan_dst_mac_priority];
  RETURN_IF_ERROR(bcm_sdk_interface_->DeleteMyStationEntry(unit_, station_id));
  vlan_dst_mac_priority_to_station_id_.erase(vlan_dst_mac_priority);

  return ::util::OkStatus();
}

::util::Status BcmL2Manager::InsertMulticastGroup(
    const BcmFlowEntry& bcm_flow_entry) {
  // We will have two cases here:
  // 1- If MAC is broadcast MAC, we just enable broadcast for that VLAN. This
  //    will fail if VLAN does not exist.
  // 2- If MAC is not broadcast MAC, we do not support the case.

  // TODO: At the moment this will be called for default/ARP VLAN and
  // broadcast MAC, as part of pushing the static entries in the forwarding
  // pipeline config. Enabling Broadcast MAC for these two are already done as
  // part of config push. So there is nothing to be done. If there is a use
  // case to do this for any other case we need to implement this method.
  return ::util::OkStatus();
}

::util::Status BcmL2Manager::DeleteMulticastGroup(
    const BcmFlowEntry& bcm_flow_entry) {
  // We will have two cases here:
  // 1- If MAC is broadcast MAC, we just disable broadcast for that VLAN. This
  //    will fail if VLAN does not exist.
  // 2- If MAC is not broadcast MAC, we do not support the case.

  // TODO: At the moment this call is not even used as we do not
  // disable broadcast for default/ARP VLAN. If there is any other use case,
  // we need to implemen this.
  return ::util::OkStatus();
}

std::unique_ptr<BcmL2Manager> BcmL2Manager::CreateInstance(
    BcmChassisManager* bcm_chassis_manager, BcmSdkInterface* bcm_sdk_interface,
    int unit) {
  return absl::WrapUnique(
      new BcmL2Manager(bcm_chassis_manager, bcm_sdk_interface, unit));
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
    // we need to make sure packets in this vlan are all sent to L3 by default.
    // We also need to still create special vlans for applications that still
    // need L2 learning (e.g. ARP). This is fundamental for ensuring switch
    // works as expected.
    std::tuple<int, uint64, int> vlan_dst_mac_priority =
        std::make_tuple(kDefaultVlan, 0ULL, kPromotedMyStationEntryPriority);
    if (vlan_config.disable_l2_learning()) {
      // Add an my station entry for promoting L2 packets to L3 if not added
      // before for default vlan.
      if (!vlan_dst_mac_priority_to_station_id_.count(vlan_dst_mac_priority)) {
        ASSIGN_OR_RETURN(int station_id, bcm_sdk_interface_->AddMyStationEntry(
                                             unit_, kDefaultVlan, 0ULL,
                                             kPromotedMyStationEntryPriority));
        vlan_dst_mac_priority_to_station_id_[vlan_dst_mac_priority] =
            station_id;
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
      auto it =
          vlan_dst_mac_priority_to_station_id_.find(vlan_dst_mac_priority);
      if (it != vlan_dst_mac_priority_to_station_id_.end()) {
        RETURN_IF_ERROR(
            bcm_sdk_interface_->DeleteMyStationEntry(unit_, it->second));
        vlan_dst_mac_priority_to_station_id_.erase(vlan_dst_mac_priority);
      }
      // Delete the specific ARP vlan (if it exists).
      RETURN_IF_ERROR(bcm_sdk_interface_->DeleteVlan(unit_, kArpVlan));
      l2_learning_disabled_for_default_vlan_ = false;
    }
  }

  return ::util::OkStatus();
}

::util::StatusOr<std::tuple<int, uint64, int>>
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
  // We do not expect any field other than vlan or dst_mac.
  int vlan = 0;
  uint64 dst_mac = 0;
  for (const auto& field : bcm_flow_entry.fields()) {
    if (field.type() == BcmField::ETH_DST) {
      dst_mac = field.value().u64();
      // We do not expect broadcast MAC as an entry here.
      CHECK_RETURN_IF_FALSE(dst_mac != kBroadcastMac)
          << "Received entry with ETH_DST set to broadcast MAC for node "
          << node_id_ << ": " << bcm_flow_entry.ShortDebugString() << ".";
    } else if (field.type() == BcmField::VLAN_VID) {
      vlan = static_cast<int>(field.value().u32());
    } else {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Received fields othere than ETH_DST and VLAN_VID for node "
             << node_id_ << ": " << bcm_flow_entry.ShortDebugString() << ".";
    }
  }
  // NOTE: We intentionally do not translate vlan = 0 to vlan = kDefaultVlan.
  // Vlan = 0 is translated to "all" vlan when programming my station entries.
  // However if controller explicitly uses kDefaultVlan and tries to program
  // all 0 in my station, we try to pick the same priority that we use for
  // L3 promote entry.
  int priority = (vlan == kDefaultVlan && dst_mac == 0ULL)
                     ? kPromotedMyStationEntryPriority
                     : kRegularMyStationEntryPriority;

  return std::make_tuple(vlan, dst_mac, priority);
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
