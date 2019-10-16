// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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

#include <algorithm>
#include <vector>

#include "stratum/hal/lib/bcm/bcm_l3_manager.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "stratum/glue/integral_types.h"
#include "absl/memory/memory.h"
#include "stratum/glue/gtl/map_util.h"

namespace stratum {
namespace hal {
namespace bcm {

BcmL3Manager::BcmL3Manager(BcmSdkInterface* bcm_sdk_interface,
                           BcmTableManager* bcm_table_manager, int unit)
    : router_intf_ref_count_(),
      bcm_sdk_interface_(ABSL_DIE_IF_NULL(bcm_sdk_interface)),
      bcm_table_manager_(ABSL_DIE_IF_NULL(bcm_table_manager)),
      node_id_(0),
      unit_(unit),
      default_drop_intf_(-1) {}

BcmL3Manager::BcmL3Manager()
    : router_intf_ref_count_(),
      bcm_sdk_interface_(nullptr),
      bcm_table_manager_(nullptr),
      node_id_(0),
      unit_(-1),
      default_drop_intf_(-1) {}

BcmL3Manager::~BcmL3Manager() {}

::util::Status BcmL3Manager::PushChassisConfig(const ChassisConfig& config,
                                               uint64 node_id) {
  node_id_ = node_id;  // Save node_id ASAP to ensure all the methods can refer
                       // to correct ID in the messages/errors.

  if (default_drop_intf_ < 0) {
    ASSIGN_OR_RETURN(default_drop_intf_,
                     bcm_sdk_interface_->FindOrCreateL3DropIntf(unit_));
  }
  // TODO(unknown): Any other thing we need to do as part of config push?

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::VerifyChassisConfig(const ChassisConfig& config,
                                                 uint64 node_id) {
  if (node_id == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid node ID.";
  }
  if (node_id_ > 0 && node_id_ != node_id) {
    return MAKE_ERROR(ERR_REBOOT_REQUIRED)
           << "Detected a change in the node_id (" << node_id_ << " vs "
           << node_id << ").";
  }

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::Shutdown() {
  router_intf_ref_count_.clear();
  return ::util::OkStatus();
}

::util::StatusOr<int> BcmL3Manager::FindOrCreateNonMultipathNexthop(
    const BcmNonMultipathNexthop& nexthop) {
  CHECK_RETURN_IF_FALSE(nexthop.unit() == unit_)
      << "Received non-multipath nexthop for unit " << nexthop.unit()
      << " on unit " << unit_ << ".";
  int vlan = nexthop.vlan();
  uint64 src_mac = nexthop.src_mac();
  uint64 dst_mac = nexthop.dst_mac();
  uint32 mpls_label = nexthop.mpls_label();
  uint32 mpls_ttl = nexthop.mpls_ttl();
  int router_intf_id = -1, egress_intf_id = -1;

  // Given the router intf, find or create the egress intf.
  switch (nexthop.type()) {
    case BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT: {
      int logical_port = nexthop.logical_port();
      if (logical_port == 0 && src_mac == 0 && dst_mac == 0) {
        ASSIGN_OR_RETURN(
            egress_intf_id,
            bcm_sdk_interface_->FindOrCreateL3CpuEgressIntf(unit_));
      } else if (logical_port >= 0 && src_mac > 0 && dst_mac > 0 && mpls_label == 0) {
        ASSIGN_OR_RETURN(
            router_intf_id,
            bcm_sdk_interface_->FindOrCreateL3RouterIntf(unit_, src_mac, vlan));
        ASSIGN_OR_RETURN(
            egress_intf_id,
            bcm_sdk_interface_->FindOrCreateL3PortEgressIntf(
                unit_, dst_mac, logical_port, vlan, router_intf_id));
      } else if (logical_port >= 0 && src_mac > 0 && dst_mac > 0 && mpls_label > 0 && mpls_ttl > 0) {
        // MPLS encap next hop
        // TODO(max): separate L3_EIF from TNL_MPLS_ENCAP creation?
        ASSIGN_OR_RETURN(
            router_intf_id,
            bcm_sdk_interface_->FindOrCreateL3MplsRouterIntf(
                unit_, src_mac, vlan, mpls_label, mpls_ttl));
        ASSIGN_OR_RETURN(
            egress_intf_id,
            bcm_sdk_interface_->FindOrCreateL3MplsEgressIntf(
                unit_, dst_mac, logical_port, router_intf_id));
      } else if (logical_port >= 0 && src_mac > 0 && dst_mac > 0 && mpls_label > 0 && mpls_ttl == 0) {
        // MPLS transit next hop
        ASSIGN_OR_RETURN(
          router_intf_id,
          bcm_sdk_interface_->FindOrCreateL3RouterIntf(unit_, src_mac, vlan));
        ASSIGN_OR_RETURN(
            egress_intf_id,
            bcm_sdk_interface_->FindOrCreateL3MplsTransitEgressIntf(
                unit_, dst_mac, logical_port, router_intf_id, mpls_label));
      } else {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Invalid nexthop of type NEXTHOP_TYPE_PORT: "
               << nexthop.ShortDebugString() << ".";
      }
      break;
    }
    case BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK: {
      int trunk_port = nexthop.trunk_port();
      if (trunk_port > 0 && src_mac > 0 && dst_mac > 0) {
        ASSIGN_OR_RETURN(
            router_intf_id,
            bcm_sdk_interface_->FindOrCreateL3RouterIntf(unit_, src_mac, vlan));
        ASSIGN_OR_RETURN(egress_intf_id,
                         bcm_sdk_interface_->FindOrCreateL3TrunkEgressIntf(
                             unit_, dst_mac, trunk_port, vlan, router_intf_id));
      } else {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Invalid nexthop of type NEXTHOP_TYPE_TRUNK: "
               << nexthop.ShortDebugString() << ".";
      }
      break;
    }
    case BcmNonMultipathNexthop::NEXTHOP_TYPE_DROP: {
      if (src_mac == 0 && dst_mac == 0) {
        ASSIGN_OR_RETURN(egress_intf_id,
                         bcm_sdk_interface_->FindOrCreateL3DropIntf(unit_));
      } else {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Invalid nexthop of type NEXTHOP_TYPE_DROP: "
               << nexthop.ShortDebugString() << ".";
      }
      break;
    }
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid nexthop type: "
             << BcmNonMultipathNexthop::Type_Name(nexthop.type())
             << ", found in " << nexthop.ShortDebugString() << ".";
  }

  if (egress_intf_id <= 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid egress_intf_id found for "
                                         << nexthop.ShortDebugString() << ".";
  }

  // Update ref count for the router intf. For the CPU and DROP egress intfs,
  // SDK internally allocates a router intf. We don care about those router
  // intfs in the code.
  if (router_intf_id > 0) RETURN_IF_ERROR(IncrementRefCount(router_intf_id));

  return egress_intf_id;
}

::util::StatusOr<int> BcmL3Manager::FindOrCreateMultipathNexthop(
    const BcmMultipathNexthop& nexthop) {
  CHECK_RETURN_IF_FALSE(nexthop.unit() == unit_)
      << "Received multipath nexthop for unit " << nexthop.unit() << " on unit "
      << unit_ << ".";
  ASSIGN_OR_RETURN(std::vector<int> member_ids, FindEcmpGroupMembers(nexthop));
  // Now this is a hack to work around an issue with BCM SDK. BCM SDK rejects
  // groups with one member. If we detect we have a group with one member, we
  // duplicate the members. This will not affect the functionality of the
  // group.
  // TODO(unknown): This needs to be revisted. We are talking to Broadcom
  // about this. http://b/75337931 is tracking this.
  if (member_ids.size() == 1) {
    VLOG(1) << "Got a group with only one member: " << member_ids[0] << ".";
    member_ids.push_back(member_ids[0]);
  }
  ASSIGN_OR_RETURN(
      int egress_intf_id,
      bcm_sdk_interface_->FindOrCreateEcmpEgressIntf(unit_, member_ids));
  if (egress_intf_id <= 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "No egress_intf_id found for "
                                         << nexthop.ShortDebugString() << ".";
  }

  return egress_intf_id;
}

::util::Status BcmL3Manager::ModifyNonMultipathNexthop(
    int egress_intf_id, const BcmNonMultipathNexthop& nexthop) {
  if (egress_intf_id <= 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid egress_intf_id: " << egress_intf_id << ".";
  }
  CHECK_RETURN_IF_FALSE(nexthop.unit() == unit_)
      << "Received non-multipath nexthop for unit " << nexthop.unit()
      << " on unit " << unit_ << ".";
  int vlan = nexthop.vlan();
  uint64 src_mac = nexthop.src_mac();
  uint64 dst_mac = nexthop.dst_mac();
  uint32 mpls_label = nexthop.mpls_label();
  uint32 mpls_ttl = nexthop.mpls_ttl();
  int old_router_intf_id = -1, new_router_intf_id = -1;

  // First find the old router intf the given egress intf is using. If the old
  // egress intf was for a DROP or CPU trap nexthop, this will return a
  // negative value, in which case we understand no router intf was created.
  ASSIGN_OR_RETURN(
      old_router_intf_id,
      bcm_sdk_interface_->FindRouterIntfFromEgressIntf(unit_, egress_intf_id));

  // Now update the egress intf.
  switch (nexthop.type()) {
    case BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT: {
      int logical_port = nexthop.logical_port();
      if (logical_port == 0 && src_mac == 0 && dst_mac == 0) {
        RETURN_IF_ERROR(
            bcm_sdk_interface_->ModifyL3CpuEgressIntf(unit_, egress_intf_id));
      } else if (logical_port >= 0 && src_mac > 0 && dst_mac > 0 && mpls_label == 0) {
        ASSIGN_OR_RETURN(
            new_router_intf_id,
            bcm_sdk_interface_->FindOrCreateL3RouterIntf(unit_, src_mac, vlan));
        RETURN_IF_ERROR(bcm_sdk_interface_->ModifyL3PortEgressIntf(
            unit_, egress_intf_id, dst_mac, logical_port, vlan,
            new_router_intf_id));
      } else if (logical_port >= 0 && src_mac > 0 && dst_mac > 0 && mpls_label > 0 && mpls_ttl > 0) {
        ASSIGN_OR_RETURN(
            new_router_intf_id,
            bcm_sdk_interface_->FindOrCreateL3MplsRouterIntf(
                unit_, src_mac, vlan, mpls_label, mpls_ttl));
        RETURN_IF_ERROR(bcm_sdk_interface_->ModifyL3MplsEgressIntf(
            unit_, egress_intf_id, dst_mac, logical_port,
            new_router_intf_id));
      } else {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Invalid nexthop of type NEXTHOP_TYPE_PORT: "
               << nexthop.ShortDebugString() << ".";
      }
      break;
    }
    case BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK: {
      int trunk_port = nexthop.trunk_port();
      if (trunk_port > 0 && src_mac > 0 && dst_mac > 0) {
        ASSIGN_OR_RETURN(
            new_router_intf_id,
            bcm_sdk_interface_->FindOrCreateL3RouterIntf(unit_, src_mac, vlan));
        RETURN_IF_ERROR(bcm_sdk_interface_->ModifyL3TrunkEgressIntf(
            unit_, egress_intf_id, dst_mac, trunk_port, vlan,
            new_router_intf_id));
      } else {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Invalid nexthop of type NEXTHOP_TYPE_TRUNK: "
               << nexthop.ShortDebugString() << ".";
      }
      break;
    }
    case BcmNonMultipathNexthop::NEXTHOP_TYPE_DROP: {
      if (src_mac == 0 && dst_mac == 0) {
        RETURN_IF_ERROR(
            bcm_sdk_interface_->ModifyL3DropIntf(unit_, egress_intf_id));
      } else {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Invalid nexthop of type NEXTHOP_TYPE_DROP: "
               << nexthop.ShortDebugString() << ".";
      }
      break;
    }
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid nexthop type: "
             << BcmNonMultipathNexthop::Type_Name(nexthop.type())
             << ", found in " << nexthop.ShortDebugString() << ".";
  }

  // Update ref count for the router intf.
  if (new_router_intf_id > 0) {
    RETURN_IF_ERROR(IncrementRefCount(new_router_intf_id));
  }
  if (old_router_intf_id > 0) {
    RETURN_IF_ERROR(DecrementRefCount(old_router_intf_id));
  }

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::ModifyMultipathNexthop(
    int egress_intf_id, const BcmMultipathNexthop& nexthop) {
  if (egress_intf_id <= 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid egress_intf_id: " << egress_intf_id << ".";
  }
  CHECK_RETURN_IF_FALSE(nexthop.unit() == unit_)
      << "Received multipath nexthop for unit " << nexthop.unit() << " on unit "
      << unit_ << ".";
  ASSIGN_OR_RETURN(std::vector<int> member_ids, FindEcmpGroupMembers(nexthop));
  RETURN_IF_ERROR(bcm_sdk_interface_->ModifyEcmpEgressIntf(
      unit_, egress_intf_id, member_ids));

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::DeleteNonMultipathNexthop(int egress_intf_id) {
  if (egress_intf_id <= 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid egress_intf_id: " << egress_intf_id << ".";
  }

  // First find the old router intf the given egress intf is using. If the old
  // egress intf was for a DROP or CPU trap nexthop, this will return a
  // negative value, in which case we understand no router intf was created.
  ASSIGN_OR_RETURN(
      int router_intf_id,
      bcm_sdk_interface_->FindRouterIntfFromEgressIntf(unit_, egress_intf_id));
  RETURN_IF_ERROR(
      bcm_sdk_interface_->DeleteL3EgressIntf(unit_, egress_intf_id));

  // Update ref count for the router intf.
  if (router_intf_id > 0) RETURN_IF_ERROR(DecrementRefCount(router_intf_id));

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::DeleteMultipathNexthop(int egress_intf_id) {
  if (egress_intf_id <= 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid egress_intf_id: " << egress_intf_id << ".";
  }
  RETURN_IF_ERROR(
      bcm_sdk_interface_->DeleteEcmpEgressIntf(unit_, egress_intf_id));

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::InsertTableEntry(
    const ::p4::v1::TableEntry& entry) {
  BcmFlowEntry bcm_flow_entry;
  RETURN_IF_ERROR(bcm_table_manager_->FillBcmFlowEntry(
      entry, ::p4::v1::Update::INSERT, &bcm_flow_entry));
  if (bcm_flow_entry.bcm_table_type() == BcmFlowEntry::BCM_TABLE_MPLS) {
    RETURN_IF_ERROR(InsertMplsFlow(bcm_flow_entry));
  } else {
    RETURN_IF_ERROR(InsertLpmOrHostFlow(bcm_flow_entry));
  }
  RETURN_IF_ERROR(bcm_table_manager_->AddTableEntry(entry));

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::InsertLpmOrHostFlow(
    const BcmFlowEntry& bcm_flow_entry) {
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.unit() == unit_)
      << "Received L3 flow for unit " << bcm_flow_entry.unit() << " on unit "
      << unit_ << ".";
  const auto bcm_table_type = bcm_flow_entry.bcm_table_type();
  LpmOrHostKey key;
  LpmOrHostActionParams action_params;
  RETURN_IF_ERROR(ExtractLpmOrHostKey(bcm_flow_entry, &key));
  RETURN_IF_ERROR(ExtractLpmOrHostActionParams(bcm_flow_entry, &action_params));
  switch (bcm_table_type) {
    case BcmFlowEntry::BCM_TABLE_IPV4_LPM:
      return bcm_sdk_interface_->AddL3RouteIpv4(
          unit_, key.vrf, key.subnet_ipv4, key.mask_ipv4,
          action_params.class_id, action_params.egress_intf_id,
          action_params.is_intf_multipath);
    case BcmFlowEntry::BCM_TABLE_IPV4_HOST:
      return bcm_sdk_interface_->AddL3HostIpv4(unit_, key.vrf, key.subnet_ipv4,
                                               action_params.class_id,
                                               action_params.egress_intf_id);
    case BcmFlowEntry::BCM_TABLE_IPV6_LPM:
      return bcm_sdk_interface_->AddL3RouteIpv6(
          unit_, key.vrf, key.subnet_ipv6, key.mask_ipv6,
          action_params.class_id, action_params.egress_intf_id,
          action_params.is_intf_multipath);
    case BcmFlowEntry::BCM_TABLE_IPV6_HOST:
      return bcm_sdk_interface_->AddL3HostIpv6(unit_, key.vrf, key.subnet_ipv6,
                                               action_params.class_id,
                                               action_params.egress_intf_id);
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid table_id: "
             << BcmFlowEntry::BcmTableType_Name(bcm_table_type) << ", found in "
             << bcm_flow_entry.ShortDebugString() << ".";
  }
}

::util::Status BcmL3Manager::ModifyTableEntry(
    const ::p4::v1::TableEntry& entry) {
  BcmFlowEntry bcm_flow_entry;
  RETURN_IF_ERROR(bcm_table_manager_->FillBcmFlowEntry(
      entry, ::p4::v1::Update::MODIFY, &bcm_flow_entry));
  RETURN_IF_ERROR(ModifyLpmOrHostFlow(bcm_flow_entry));
  RETURN_IF_ERROR(bcm_table_manager_->UpdateTableEntry(entry));

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::ModifyLpmOrHostFlow(
    const BcmFlowEntry& bcm_flow_entry) {
  const auto bcm_table_type = bcm_flow_entry.bcm_table_type();
  int unit = bcm_flow_entry.unit();
  LpmOrHostKey key;
  LpmOrHostActionParams action_params;
  RETURN_IF_ERROR(ExtractLpmOrHostKey(bcm_flow_entry, &key));
  RETURN_IF_ERROR(ExtractLpmOrHostActionParams(bcm_flow_entry, &action_params));
  switch (bcm_table_type) {
    case BcmFlowEntry::BCM_TABLE_IPV4_LPM:
      return bcm_sdk_interface_->ModifyL3RouteIpv4(
          unit, key.vrf, key.subnet_ipv4, key.mask_ipv4, action_params.class_id,
          action_params.egress_intf_id, action_params.is_intf_multipath);
    case BcmFlowEntry::BCM_TABLE_IPV4_HOST:
      return bcm_sdk_interface_->ModifyL3HostIpv4(
          unit, key.vrf, key.subnet_ipv4, action_params.class_id,
          action_params.egress_intf_id);
    case BcmFlowEntry::BCM_TABLE_IPV6_LPM:
      return bcm_sdk_interface_->ModifyL3RouteIpv6(
          unit, key.vrf, key.subnet_ipv6, key.mask_ipv6, action_params.class_id,
          action_params.egress_intf_id, action_params.is_intf_multipath);
    case BcmFlowEntry::BCM_TABLE_IPV6_HOST:
      return bcm_sdk_interface_->ModifyL3HostIpv6(
          unit, key.vrf, key.subnet_ipv6, action_params.class_id,
          action_params.egress_intf_id);
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid bcm_table_type: "
             << BcmFlowEntry::BcmTableType_Name(bcm_table_type) << ", found in "
             << bcm_flow_entry.ShortDebugString() << ".";
  }
}

::util::Status BcmL3Manager::DeleteTableEntry(
    const ::p4::v1::TableEntry& entry) {
  BcmFlowEntry bcm_flow_entry;
  RETURN_IF_ERROR(bcm_table_manager_->FillBcmFlowEntry(
      entry, ::p4::v1::Update::DELETE, &bcm_flow_entry));
  RETURN_IF_ERROR(DeleteLpmOrHostFlow(bcm_flow_entry));
  RETURN_IF_ERROR(bcm_table_manager_->DeleteTableEntry(entry));

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::UpdateMultipathGroupsForPort(uint32 port_id) {
  // Generate map from BCM multipath group id to data for all groups which
  // reference the given port.
  ASSIGN_OR_RETURN(
      auto nexthops,
      bcm_table_manager_->FillBcmMultipathNexthopsWithPort(port_id));
  for (const auto& nexthop : nexthops) {
    RETURN_IF_ERROR(ModifyMultipathNexthop(nexthop.first, nexthop.second));
  }
  return ::util::OkStatus();
}

::util::Status BcmL3Manager::DeleteLpmOrHostFlow(
    const BcmFlowEntry& bcm_flow_entry) {
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.unit() == unit_)
      << "Received L3 flow for unit " << bcm_flow_entry.unit() << " on unit "
      << unit_ << ".";
  const auto bcm_table_type = bcm_flow_entry.bcm_table_type();
  LpmOrHostKey key;
  RETURN_IF_ERROR(ExtractLpmOrHostKey(bcm_flow_entry, &key));
  switch (bcm_table_type) {
    case BcmFlowEntry::BCM_TABLE_IPV4_LPM:
      return bcm_sdk_interface_->DeleteL3RouteIpv4(
          unit_, key.vrf, key.subnet_ipv4, key.mask_ipv4);
    case BcmFlowEntry::BCM_TABLE_IPV4_HOST:
      return bcm_sdk_interface_->DeleteL3HostIpv4(unit_, key.vrf,
                                                  key.subnet_ipv4);
    case BcmFlowEntry::BCM_TABLE_IPV6_LPM:
      return bcm_sdk_interface_->DeleteL3RouteIpv6(
          unit_, key.vrf, key.subnet_ipv6, key.mask_ipv6);
    case BcmFlowEntry::BCM_TABLE_IPV6_HOST:
      return bcm_sdk_interface_->DeleteL3HostIpv6(unit_, key.vrf,
                                                  key.subnet_ipv6);
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid bcm_table_type: "
             << BcmFlowEntry::BcmTableType_Name(bcm_table_type) << ", found in "
             << bcm_flow_entry.ShortDebugString() << ".";
  }
}

::util::Status BcmL3Manager::InsertMplsFlow(const BcmFlowEntry& bcm_flow_entry) {
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.unit() == unit_)
      << "Received Mpls flow for unit " << bcm_flow_entry.unit() << " on unit "
      << unit_ << ".";
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.bcm_table_type() ==
                        BcmFlowEntry::BCM_TABLE_MPLS);

  MplsKey key;
  MplsActionParams action_params;
  RETURN_IF_ERROR(ExtractMplsKey(bcm_flow_entry, &key));
  RETURN_IF_ERROR(ExtractMplsActionParams(bcm_flow_entry, &action_params));

  return bcm_sdk_interface_->AddMplsRoute(unit_, key.port, key.mpls_label,
      action_params.egress_intf_id);
}

::util::Status BcmL3Manager::ModifyMplsFlow(const BcmFlowEntry& bcm_flow_entry) {
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.unit() == unit_)
      << "Received Mpls flow for unit " << bcm_flow_entry.unit() << " on unit "
      << unit_ << ".";
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.bcm_table_type() ==
                        BcmFlowEntry::BCM_TABLE_MPLS);

  return MAKE_ERROR(ERR_UNIMPLEMENTED) << "not implemented";
}

::util::Status BcmL3Manager::DeleteMplsFlow(const BcmFlowEntry& bcm_flow_entry) {
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.unit() == unit_)
      << "Received Mpls flow for unit " << bcm_flow_entry.unit() << " on unit "
      << unit_ << ".";
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.bcm_table_type() ==
                        BcmFlowEntry::BCM_TABLE_MPLS);

  return MAKE_ERROR(ERR_UNIMPLEMENTED) << "not implemented";
}

std::unique_ptr<BcmL3Manager> BcmL3Manager::CreateInstance(
    BcmSdkInterface* bcm_sdk_interface, BcmTableManager* bcm_table_manager,
    int unit) {
  return absl::WrapUnique(
      new BcmL3Manager(bcm_sdk_interface, bcm_table_manager, unit));
}

::util::Status BcmL3Manager::ExtractLpmOrHostKey(
    const BcmFlowEntry& bcm_flow_entry, LpmOrHostKey* key) {
  if (key == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null key!";
  }

  const auto bcm_table_type = bcm_flow_entry.bcm_table_type();
  // Find subnet (and mask) and VRF.
  if (bcm_flow_entry.fields_size() > 2) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Expected at most two fields of type IPV4_DST/IPV6_DST or VRF: "
           << bcm_flow_entry.ShortDebugString() << ".";
  }
  switch (bcm_table_type) {
    case BcmFlowEntry::BCM_TABLE_IPV4_LPM:
    case BcmFlowEntry::BCM_TABLE_IPV4_HOST: {
      for (const auto& field : bcm_flow_entry.fields()) {
        if (field.type() == BcmField::IPV4_DST) {
          key->subnet_ipv4 = field.value().u32();
          key->mask_ipv4 = field.mask().u32();
        } else if (field.type() == BcmField::VRF) {
          key->vrf = static_cast<int>(field.value().u32());
        } else {
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Invalid field type. Expecting IPV4_DST or VRF types only: "
                 << bcm_flow_entry.ShortDebugString() << ".";
        }
        // Validations. Having a mask does not make sense for host routes.
        if (key->mask_ipv4 != 0 &&
            bcm_table_type == BcmFlowEntry::BCM_TABLE_IPV4_HOST) {
          return MAKE_ERROR(ERR_INVALID_PARAM)
                  << "Must not specify mask on host dst routes "
                  << "IP: " << bcm_flow_entry.ShortDebugString() << ".";
        }
      }
      break;
    }
    case BcmFlowEntry::BCM_TABLE_IPV6_LPM:
    case BcmFlowEntry::BCM_TABLE_IPV6_HOST: {
      for (const auto& field : bcm_flow_entry.fields()) {
        if (field.type() == BcmField::IPV6_DST ||
            field.type() == BcmField::IPV6_DST_UPPER_64) {
          key->subnet_ipv6 = field.value().b();
          key->mask_ipv6 = field.mask().b();
        } else if (field.type() == BcmField::VRF) {
          key->vrf = static_cast<int>(field.value().u32());
        } else {
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Invalid field type. Expecting IPV6_DST or VRF types only: "
                 << bcm_flow_entry.ShortDebugString();
        }
        // Validations.
        if (key->subnet_ipv6.empty() ||
            bcm_table_type == BcmFlowEntry::BCM_TABLE_IPV6_HOST) {
          if (!key->mask_ipv6.empty()) {
            return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Must not specify mask when subnet is 0 or a host dst "
                   << "IP: " << bcm_flow_entry.ShortDebugString() << ".";
          }
        }
      }
      break;
    }
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid bcm_table_type: "
             << BcmFlowEntry::BcmTableType_Name(bcm_table_type) << ", found in "
             << bcm_flow_entry.ShortDebugString() << ".";
  }

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::ExtractLpmOrHostActionParams(
    const BcmFlowEntry& bcm_flow_entry, LpmOrHostActionParams* action_params) {
  if (action_params == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null action_params!";
  }

  // Find the egress_intf_id and class_id. When programming L3 LPM or host flows
  // BCM support setting the class_id for packets as well. Although controller
  // does not use it at the moment.
  if (bcm_flow_entry.actions_size() > 2) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Expected at most two actions of type OUTPUT_{PORT,TRUNK,GROUP} "
           << "or SET_L3_DST_CLASS_ID: " << bcm_flow_entry.ShortDebugString()
           << ".";
  }
  for (const auto& action : bcm_flow_entry.actions()) {
    switch (action.type()) {
      case BcmAction::DROP:
      case BcmAction::OUTPUT_PORT:
      case BcmAction::OUTPUT_TRUNK: {
        // Here we have the following cases:
        // 1- We get the egress_intf_id (happens when controller directly points
        //    to a member which is already created).
        // 2- We get the src_mac, dst_mac, port/trunk. Generally in this case
        //    We need to find or create an egress intf first and then program
        //    the flow. But this means the stack needs to internally keep track
        //    of this egress object and do the necessary cleanup when needed.
        //    This is complex and error prone. It is much better to have the
        //    controller handle this by programming the member first. So we
        //    do not support this case.
        if (action.params_size() == 1 &&
            action.params(0).type() == BcmAction::Param::EGRESS_INTF_ID) {
          action_params->egress_intf_id =
              static_cast<int>(action.params(0).value().u32());
        } else {
          uint64 src_mac = 0, dst_mac = 0;
          int logical_port = 0, trunk_port = 0;
          for (const auto& param : action.params()) {
            if (param.type() == BcmAction::Param::ETH_SRC) {
              src_mac = param.value().u64();
            } else if (param.type() == BcmAction::Param::ETH_DST) {
              dst_mac = param.value().u64();
            } else if (param.type() == BcmAction::Param::LOGICAL_PORT) {
              logical_port = param.value().u32();
            } else if (param.type() == BcmAction::Param::TRUNK_PORT) {
              trunk_port = param.value().u32();
            }
          }
          if (action.type() == BcmAction::DROP && src_mac > 0 && dst_mac > 0 &&
              logical_port == 0 && trunk_port == 0) {
            return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                   << "Flow action required defining a new drop egress intf "
                   << "(src_mac: " << src_mac << ", dst_mac: " << dst_mac
                   << "). This is not supported.";
          } else if (action.type() == BcmAction::OUTPUT_PORT && src_mac > 0 &&
                     dst_mac > 0 && logical_port > 0 && trunk_port == 0) {
            return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                   << "Flow action required defining a new port egress intf "
                   << "(src_mac: " << src_mac << ", dst_mac: " << dst_mac
                   << ", logical_port: " << logical_port
                   << "). This is not supported.";
          } else if (action.type() == BcmAction::OUTPUT_TRUNK && src_mac > 0 &&
                     dst_mac > 0 && logical_port == 0 && trunk_port > 0) {
            return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
                   << "Flow action required defining a new trunk egress intf "
                   << "(src_mac: " << src_mac << ", dst_mac: " << dst_mac
                   << ", trunk_port: " << trunk_port
                   << "). This is not supported.";
          } else {
            return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Invalid action parameters for an action of type "
                   << "DROP or OUTPUT_{PORT,TRUNK}: "
                   << bcm_flow_entry.ShortDebugString() << ".";
          }
        }
        break;
      }
      case BcmAction::OUTPUT_L3: {
        if (action.params_size() != 1 ||
            action.params(0).type() != BcmAction::Param::EGRESS_INTF_ID) {
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Expects only one parameter of type EGRESS_INTF_ID for "
                 << "action of type OUTPUT_L3: "
                 << bcm_flow_entry.ShortDebugString() << ".";
        }
        action_params->egress_intf_id =
            static_cast<int>(action.params(0).value().u32());
        action_params->is_intf_multipath = true;
        break;
      }
      case BcmAction::SET_L3_DST_CLASS_ID: {
        if (action.params_size() != 1 ||
            action.params(0).type() != BcmAction::Param::L3_DST_CLASS_ID) {
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Expects only one parameter of type L3_DST_CLASS_ID for "
                 << "action of type SET_L3_DST_CLASS_ID: "
                 << bcm_flow_entry.ShortDebugString() << ".";
        }
        action_params->class_id =
            static_cast<int>(action.params(0).value().u32());
        if (action_params->class_id <= 0) {
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Invalid class_id for action of type SET_L3_DST_CLASS_ID: "
                 << bcm_flow_entry.ShortDebugString() << ".";
        }
        break;
      }
      default:
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Invalid action type. Expecting OUTPUT_{PORT,TRUNK,GROUP} or "
               << "SET_L3_DST_CLASS_ID types: "
               << bcm_flow_entry.ShortDebugString() << ".";
    }
  }

  if (action_params->egress_intf_id <= 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Could not resolve an egress_intf_id for "
           << bcm_flow_entry.ShortDebugString() << ".";
  }

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::ExtractMplsKey(const BcmFlowEntry& bcm_flow_entry,
                                            MplsKey* key) {
  if (key == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null key!";
  }
  CHECK_RETURN_IF_FALSE(bcm_flow_entry.bcm_table_type() ==
                        BcmFlowEntry::BCM_TABLE_MPLS);
  if (bcm_flow_entry.fields_size() != 2) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
        << "Expected at exactly two fields of type MPLS_LABEL label and IN_PORT: "
        << bcm_flow_entry.ShortDebugString() << ".";
  }
  for (const auto& field : bcm_flow_entry.fields()) {
    if (field.type() == BcmField::MPLS_LABEL) {
      key->mpls_label = field.value().u32();
    } else if (field.type() == BcmField::IN_PORT) {
      key->port =field.value().u32();
    } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
          << "Invalid field type. Expecting MPLS_LABEL and IN_PORT types only: "
          << bcm_flow_entry.ShortDebugString() << ".";
    }
  }
  // Validations
  if (!key->mpls_label) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Missing Mpls label key in: "
        << bcm_flow_entry.ShortDebugString() << ".";
  }
  if (!key->port) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Missing port key in: "
        << bcm_flow_entry.ShortDebugString() << ".";
  }

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::ExtractMplsActionParams(
      const BcmFlowEntry& bcm_flow_entry, MplsActionParams* action_params) {
  if (action_params == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null action_params!";
  }

  // Find the egress_intf_id.
  if (bcm_flow_entry.actions_size() > 1) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
        << "Expected at most 1 action of type OUTPUT_{PORT,TRUNK,L3}: "
        << bcm_flow_entry.ShortDebugString() << ".";
  }
  for (const auto& action : bcm_flow_entry.actions()) {
    switch (action.type()) {
      case BcmAction::OUTPUT_L3:
      case BcmAction::OUTPUT_PORT:
      case BcmAction::OUTPUT_TRUNK: {
        // We only support the simple case where the egress interface is already
        // created by the controller.
        if (action.params_size() == 1 &&
            action.params(0).type() == BcmAction::Param::EGRESS_INTF_ID) {
          action_params->egress_intf_id =
              static_cast<int>(action.params(0).value().u32());
          if (action.type() == BcmAction::OUTPUT_L3) {
            action_params->is_intf_multipath = true;
          }
        } else {
          return MAKE_ERROR(ERR_INVALID_PARAM)
              << "Invalid action parameters for an action of type "
              << "OUTPUT_{PORT,TRUNK,L3}: "
              << bcm_flow_entry.ShortDebugString() << ".";
        }
        break;
      }
      default:
        return MAKE_ERROR(ERR_INVALID_PARAM)
            << "Invalid action type. Expecting OUTPUT_{PORT,TRUNK,L3} types: "
            << bcm_flow_entry.ShortDebugString() << ".";
    }
  }

  if (action_params->egress_intf_id <= 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
        << "Could not resolve an egress_intf_id for "
        << bcm_flow_entry.ShortDebugString() << ".";
  }

  return ::util::OkStatus();
}

::util::StatusOr<std::vector<int>> BcmL3Manager::FindEcmpGroupMembers(
    const BcmMultipathNexthop& nexthop) {
  // If this group has no members, it has been pruned due to member singleton or
  // trunk ports being down or blocked. Add the default drop interface in that
  // case.
  if (!nexthop.members_size()) return std::vector<int>(1, default_drop_intf_);
  std::vector<int> member_ids;
  for (const auto& member : nexthop.members()) {
    if (member.weight() == 0) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Zero weight: " << nexthop.ShortDebugString() << ".";
    }
    if (member.egress_intf_id() <= 0) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid member egress_intf_id: " << nexthop.ShortDebugString()
             << ".";
    }
    for (size_t i = 0; i < member.weight(); ++i) {
      member_ids.push_back(member.egress_intf_id());
    }
  }
  std::sort(member_ids.begin(), member_ids.end());  // sort the member ids

  return member_ids;
}

::util::Status BcmL3Manager::IncrementRefCount(int router_intf_id) {
  router_intf_ref_count_[router_intf_id]++;

  return ::util::OkStatus();
}

::util::Status BcmL3Manager::DecrementRefCount(int router_intf_id) {
  uint32* ref_count = gtl::FindOrNull(router_intf_ref_count_, router_intf_id);
  CHECK_RETURN_IF_FALSE(ref_count != nullptr)
      << "Inconsistent state. router_intf_id: " << router_intf_id
      << " not in router_intf_ref_count_ map.";
  CHECK_RETURN_IF_FALSE(*ref_count > 0)
      << "Inconsistent state. router_intf_id: " << router_intf_id
      << " has zero ref count.";
  (*ref_count)--;
  if (*ref_count == 0) {
    // No egress intf is using this router intf. It can be cleaned up.
    RETURN_IF_ERROR(
        bcm_sdk_interface_->DeleteL3RouterIntf(unit_, router_intf_id));
    router_intf_ref_count_.erase(router_intf_id);
  }

  return ::util::OkStatus();
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
