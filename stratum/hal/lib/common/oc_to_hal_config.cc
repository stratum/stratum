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


#include "stratum/hal/lib/common/oc_to_hal_config.h"

#include <iostream>
#include "stratum/lib/constants.h"
#include "stratum/public/proto/yang_wrappers.pb.h"
#include "util/gtl/map_util.h"

namespace stratum {
namespace hal {

namespace {
////////////////////////////////////////////////////////////////////////////////
// converts:
//   oc::Device + oc::Components::Component
// into:
//   stratum::Chassis
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<stratum::Chassis> ComponentToChassis(
    const oc::Device &device, const oc::Components::Component &component) {
  stratum::Chassis to;

  to.set_name(component.chassis().config().name().value());

  if (component.chassis()
          .config()
          .vendor_specific()
          .Is<oc::Bcm::Chassis::Config>()) {
    oc::Bcm::Chassis::Config bcm_specific;
    component.chassis().config().vendor_specific().UnpackTo(&bcm_specific);
    switch (bcm_specific.platform()) {
      default:
        break;
    }
  }

  return to;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   oc::Device + oc::Components::Component
// into:
//   stratum::Node
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<stratum::Node> ComponentToNode(
    const oc::Device &device, const oc::Components::Component &component) {
  stratum::Node to;

  to.set_id(component.node().config().uid().value());
  to.set_name(component.config().name().value());
  // No need to check if linecard component is present. This method will not be
  // called if it is missing.
  to.set_slot(device.components()
                  .component()
                  .at(component.node().config().linecard().value())
                  .linecard()
                  .config()
                  .slot_id()
                  .value());
  to.set_index(component.node().config().index().value());

  // TODO: For now by default disable learning on default VLAN.
  // This will eventually come from gNMI.
  auto* vlan_config = to.mutable_config_params()->add_vlan_configs();
  vlan_config->set_block_broadcast(false);
  vlan_config->set_block_known_multicast(false);
  vlan_config->set_block_unknown_multicast(true);
  vlan_config->set_block_unknown_unicast(true);
  vlan_config->set_disable_l2_learning(true);

  // TODO: There are still a lot of things we are not supporting for
  // nodes, including VLAN configs. Add support for those in the YANG model as
  // well as the proto encodings.  Then add support here in the code.

  return to;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   oc::Device + oc::Components::Component
// into:
//   stratum::GoogleConfig
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<stratum::GoogleConfig>
ComponentToChassisBcmChipSpecific(const oc::Device &device,
                                  const oc::Components::Component &component) {
  stratum::GoogleConfig to;

  if (component.chassis()
          .config()
          .vendor_specific()
          .Is<oc::Bcm::Chassis::Config>()) {
    oc::Bcm::Chassis::Config bcm_specific;
    component.chassis().config().vendor_specific().UnpackTo(&bcm_specific);

    *to.mutable_bcm_chassis_map_id() =
        bcm_specific.bcm_chassis_map_id().value();

    // map<int32, NodeIdToKnetConfig> node_id_to_knet_config
    for (const auto &entry : bcm_specific.node_id_to_knet_config()) {
      stratum::GoogleConfig::BcmKnetConfig conf;
      for (const auto &config : entry.second.knet_intf_configs()) {
        stratum::GoogleConfig::BcmKnetConfig::BcmKnetIntfConfig intf;

        switch (config.second.purpose()) {
          case oc::Bcm::HerculesBcmChip::BCM_KNET_IF_PURPOSE_CONTROLLER:
            intf.set_purpose(stratum::GoogleConfig::
                                 BCM_KNET_INTF_PURPOSE_CONTROLLER);
            break;
          case oc::Bcm::HerculesBcmChip::BCM_KNET_IF_PURPOSE_SFLOW:
            intf.set_purpose(
                stratum::GoogleConfig::BCM_KNET_INTF_PURPOSE_SFLOW);
            break;
          default:
            break;
        }
        intf.set_mtu(config.second.mtu().value());
        intf.set_cpu_queue(config.second.cpu_queue().value());
        intf.set_vlan(config.second.vlan().value());

        *conf.add_knet_intf_configs() = intf;
      }

      (*to.mutable_node_id_to_knet_config())[entry.first] = conf;
    }

    // map<int32, NodeIdToTxConfig> node_id_to_tx_config
    for (const auto &entry : bcm_specific.node_id_to_tx_config()) {
      stratum::GoogleConfig::BcmTxConfig config;
      (*to.mutable_node_id_to_tx_config())[entry.first] = config;
    }

    // map<int32, NodeIdToRxConfig> node_id_to_rx_config
    for (const auto &entry : bcm_specific.node_id_to_rx_config()) {
      stratum::GoogleConfig::BcmRxConfig conf;

      conf.set_rx_pool_pkt_count(entry.second.rx_pool_pkt_count().value());
      conf.set_rx_pool_bytes_per_pkt(
          entry.second.rx_pool_bytes_per_pkt().value());
      conf.set_max_pkt_size_bytes(entry.second.max_pkt_size_bytes().value());
      conf.set_pkts_per_chain(entry.second.pkts_per_chain().value());
      conf.set_max_rate_pps(entry.second.max_rate_pps().value());
      conf.set_max_burst_pkts(entry.second.max_burst_pkts().value());
      conf.set_use_interrupt(entry.second.use_interrupt().value());

      for (const auto &config : entry.second.dma_channel_configs()) {
        stratum::GoogleConfig::BcmRxConfig::BcmDmaChannelConfig a;

        a.set_chains(config.second.chains().value());
        a.set_strip_crc(config.second.strip_crc().value());
        a.set_strip_vlan(config.second.strip_vlan().value());
        a.set_oversized_packets_ok(
            config.second.oversized_packets_ok().value());
        a.set_no_pkt_parsing(config.second.no_pkt_parsing().value());

        for (const auto &cos_set : config.second.cos_set()) {
          a.add_cos_set(cos_set.value());
        }

        (*conf.mutable_dma_channel_configs())[entry.first] = a;
      }

      (*to.mutable_node_id_to_rx_config())[entry.first] = conf;
    }

    // map<uint64, BcmRateLimitConfig> node_id_to_rate_limit_config
    for (const auto &entry : bcm_specific.node_id_to_rate_limit_config()) {
      stratum::GoogleConfig::BcmRateLimitConfig conf;

      conf.set_max_rate_pps(entry.second.max_rate_pps().value());
      conf.set_max_burst_pkts(entry.second.max_burst_pkts().value());
      for (const auto &config : entry.second.per_cos_rate_limit_configs()) {
        stratum::GoogleConfig::BcmRateLimitConfig::
            BcmPerCosRateLimitConfig per_cos;

        per_cos.set_max_rate_pps(config.second.max_rate_pps().value());
        per_cos.set_max_burst_pkts(config.second.max_burst_pkts().value());

        (*conf.mutable_per_cos_rate_limit_configs())[config.first] = per_cos;
      }

      (*to.mutable_node_id_to_rate_limit_config())[entry.first] = conf;
    }
  }
  return to;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   oc::Device + oc::Interfaces::Interface
// into:
//   stratum::TrunkPort
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<stratum::TrunkPort> InterfaceToTrunkPort(
    const oc::Device &device, const oc::Interfaces::Interface &interface) {
  stratum::TrunkPort to;

  to.set_id(interface.hercules_interface().config().uid().value());
  to.set_name(interface.config().name().value());

  switch (interface.aggregation().config().lag_type()) {
    case oc::OpenconfigIfAggregate::AGGREGATION_TYPE_LACP:
      to.set_type(stratum::TrunkPort::LACP_TRUNK);
      break;
    case oc::OpenconfigIfAggregate::AGGREGATION_TYPE_STATIC:
      to.set_type(stratum::TrunkPort::STATIC_TRUNK);
      break;
    default:
      break;
  }

  for (const auto &entry : device.interfaces().interface()) {
    const oc::Interfaces::Interface &i = entry.second;
    if (i.config().type() != oc::IetfInterfaces::ETHERNET_CSMACD ||
        i.ethernet().config().aggregate_id().value() !=
            interface.config().name().value()) {
      continue;
    }
    to.add_members(i.hercules_interface().config().uid().value());
  }

  return to;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   oc::Device + oc::Interfaces::Interface
// into:
//   stratum::SingletonPort
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<stratum::SingletonPort> InterfaceToSingletonPort(
    const oc::Device &device, const oc::Interfaces::Interface &interface) {
  stratum::SingletonPort to;

  to.set_id(interface.hercules_interface().config().uid().value());
  to.set_name(interface.config().name().value());

  const oc::Components::Component *port =
      gtl::FindOrNull(device.components().component(),
                      interface.config().hardware_port().value());
  // No need to check for nullptr as this method will not be called if the
  // hardware_port component is not present.
  to.set_slot(port->config().slot_id().value());
  to.set_port(port->config().port_id().value());
  to.set_channel(port->config().channel_id().value());
  // TODO: This is temporary till we have the correct proto and have
  // the map from the port to nodes. This will eventually come from gNMI.
  to.set_node(1);

  switch (interface.ethernet().config().port_speed()) {
    case oc::OpenconfigIfEthernet::SPEED_10MB:
      to.set_speed_bps(10000000);
      break;
    case oc::OpenconfigIfEthernet::SPEED_100MB:
      to.set_speed_bps(100000000);
      break;
    case oc::OpenconfigIfEthernet::SPEED_1GB:
      to.set_speed_bps(1000000000);
      break;
    case oc::OpenconfigIfEthernet::SPEED_10GB:
      to.set_speed_bps(kTenGigBps);
      break;
    case oc::OpenconfigIfEthernet::SPEED_25GB:
      to.set_speed_bps(kTwentyFiveGigBps);
      break;
    case oc::OpenconfigIfEthernet::SPEED_40GB:
      to.set_speed_bps(kFortyGigBps);
      break;
    case oc::OpenconfigIfEthernet::SPEED_50GB:
      to.set_speed_bps(kFiftyGigBps);
      break;
    case oc::OpenconfigIfEthernet::SPEED_100GB:
      to.set_speed_bps(kHundredGigBps);
      break;
    default:
      break;
  }

  return to;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// converts:
//   oc::Device
// into:
//   stratum::ChassisConfig
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<stratum::ChassisConfig>
OpenConfigToHalConfigProtoConverter::DeviceToChassisConfig(
    const oc::Device &in) {
  stratum::ChassisConfig to;

  // Check if the input protobuf is correct. Any found problem is logged inside
  // IsCorrectProtoDevice() method.
  if (!IsCorrectProtoDevice(in)) {
    // Ooops... Cannot be safely processed!
    return to;
  }

  for (const auto &entry : in.components().component()) {
    const oc::Components::Component &component = entry.second;

    switch (component.type()) {
      case oc::OpenconfigPlatformTypes::HW_BCM_BASED_CHASSIS: {
        // Set chassis field.
        ASSIGN_OR_RETURN(*to.mutable_chassis(),
                         ComponentToChassis(in, component));
        // Set vendor_config.google_config field.
        ASSIGN_OR_RETURN(*to.mutable_vendor_config()->mutable_google_config(),
                         ComponentToChassisBcmChipSpecific(in, component));
      } break;

      case oc::OpenconfigPlatformTypes::HW_NODE: {
        // Create nodes elements.
        ASSIGN_OR_RETURN(*to.add_nodes(), ComponentToNode(in, component));
      } break;

      default:
        break;
    }
  }

  // Create singleton_ports elements.
  for (const auto &entry : in.interfaces().interface()) {
    const oc::Interfaces::Interface &interface = entry.second;
    if (interface.config().type() != oc::IetfInterfaces::ETHERNET_CSMACD) {
      continue;
    }
    ASSIGN_OR_RETURN(*to.add_singleton_ports(),
                     InterfaceToSingletonPort(in, interface));
  }

  // Create trunk_ports elements.
  for (const auto &entry : in.interfaces().interface()) {
    const oc::Interfaces::Interface &interface = entry.second;
    if (interface.config().type() != oc::IetfInterfaces::IEEE_8023_AD_LAG) {
      continue;
    }
    ASSIGN_OR_RETURN(*to.add_trunk_ports(),
                     InterfaceToTrunkPort(in, interface));
  }

  VLOG(1) << "Converted ChassisConfig:\n" << to.ShortDebugString();
  return to;
}

////////////////////////////////////////////////////////////////////////////////
//
// Checks if oc:Device proto is consistent
//
////////////////////////////////////////////////////////////////////////////////
bool OpenConfigToHalConfigProtoConverter::IsCorrectProtoDevice(
    const oc::Device &in) {
  // Verify components.
  for (const auto &entry : in.components().component()) {
    const oc::Components::Component &component = entry.second;

    switch (component.type()) {
      case oc::OpenconfigPlatformTypes::HW_NODE:
        // A node.
        if (gtl::FindOrNull(in.components().component(),
                            component.node().config().linecard().value()) ==
            nullptr) {
          LOG(ERROR) << "unknown 'linecard'" << component.ShortDebugString();
          return false;
        }
        break;

      default:
        break;
    }
  }

  // Verify interfaces.
  for (const auto &entry : in.interfaces().interface()) {
    const oc::Interfaces::Interface &interface = entry.second;
    switch (interface.config().type()) {
      case oc::IetfInterfaces::ETHERNET_CSMACD:
        // Regular Ethernet interface.
        if (gtl::FindOrNull(in.components().component(),
                            interface.config().hardware_port().value()) ==
            nullptr) {
          LOG(ERROR) << "unknown 'hardware_port'"
                     << interface.ShortDebugString();
          return false;
        }
        break;

      case oc::IetfInterfaces::IEEE_8023_AD_LAG:
        // Trunk interface.
        break;

      default:
        break;
    }
  }

  // No problems/inconsistencies found!
  return true;
}

}  // namespace hal
}  // namespace stratum
