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


#include "stratum/hal/lib/common/hal_config_to_oc.h"

#include <iostream>
#include <list>
#include <map>
#include "stratum/glue/logging.h"
#include "stratum/lib/constants.h"
#include "stratum/public/proto/yang_wrappers.pb.h"
#include "absl/strings/substitute.h"
#include "util/gtl/map_util.h"

namespace stratum {
namespace hal {

namespace {
////////////////////////////////////////////////////////////////////////////////
// converts:
//   google::hercules::Node
// into:
//   std::list<oc::Components::Component>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Components::Component>> NodeToComponent(
    const google::hercules::Node& in) {
  std::string linecard_name = absl::Substitute(":lc-$0", in.slot());

  oc::Components::Component linecard;
  linecard.set_type(oc::OpenconfigPlatformTypes::HW_LINECARD);
  linecard.mutable_config()->mutable_name()->set_value(linecard_name);
  linecard.mutable_config()->mutable_slot_id()->set_value(in.slot());
  linecard.mutable_linecard()->mutable_config()->mutable_slot_id()->set_value(
      in.slot());

  oc::Components::Component::Subcomponents::Subcomponent reference;
  reference.mutable_name()->set_value(in.name());
  reference.mutable_config()->mutable_name()->set_value(in.name());

  (*linecard.mutable_subcomponents()->mutable_subcomponent())[in.name()] =
      reference;

  oc::Components::Component node;
  node.set_type(oc::OpenconfigPlatformTypes::HW_NODE);
  node.mutable_config()->mutable_name()->set_value(in.name());
  node.mutable_node()->mutable_config()->mutable_linecard()->set_value(
      linecard_name);
  node.mutable_node()->mutable_config()->mutable_uid()->set_value(in.id());
  node.mutable_node()->mutable_config()->mutable_index()->set_value(in.index());

  oc::Components::Component::Node::Config::ConfigParams config_params;

  *node.mutable_node()->mutable_config()->mutable_config_params() =
      config_params;

  // TODO: There are still a lot of things we are not supporting for
  // nodes, including VLAN configs. Add support for those in the YANG model as
  // well as the proto encodings. Then add support here in the code.

  return std::list<oc::Components::Component>({linecard, node});
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   google::hercules::Chassis
// into:
//   std::list<oc::Components::Component>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Components::Component>> ChassisToComponent(
    const google::hercules::Chassis& in) {
  oc::Components::Component component;

  component.set_type(oc::OpenconfigPlatformTypes::HW_BCM_BASED_CHASSIS);
  component.mutable_config()->mutable_name()->set_value(in.name());
  component.mutable_chassis()->mutable_config()->mutable_name()->set_value(
      in.name());
  oc::Bcm::Chassis::Config bcm_config;
  switch (in.platform()) {
    case google::hercules::Platform::PLT_GENERIC_TRIDENT2:
      break;
    case google::hercules::Platform::PLT_GENERIC_TRIDENT2:
      break;
    case google::hercules::Platform::PLT_GENERIC_TOMAHAWK:
      break;
    case google::hercules::Platform::PLT_P4_SOFT_SWITCH:
      bcm_config.set_platform(oc::Bcm::HerculesChassis::P4_SOFT_SWITCH);
      break;
    case google::hercules::Platform::PLT_MLNX_SN2700:
      bcm_config.set_platform(oc::Bcm::HerculesChassis::MLNX_SN2700);
      break;
    default:
      // Hmm...
      LOG(ERROR) << "Unknow 'platform': " << in.ShortDebugString();
      break;
  }
  component.mutable_chassis()
      ->mutable_config()
      ->mutable_vendor_specific()
      ->PackFrom(bcm_config);
  return std::list<oc::Components::Component>({component});
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   google::hercules::VendorConfig
// into:
//   std::list<oc::Components::Component>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Components::Component>> VendorConfigToComponent(
    const google::hercules::VendorConfig& in) {
  oc::Components::Component component;

  oc::Bcm::Chassis::Config bcm_config;

  for (const auto& entry : in.google_config().node_id_to_knet_config()) {
    oc::Bcm::Chassis::Config::NodeIdToKnetConfig val;

    val.mutable_node_uid()->set_value(entry.first);

    int index = 0;
    for (const auto& limit : entry.second.knet_intf_configs()) {
      oc::Bcm::Chassis::Config::NodeIdToKnetConfig::KnetIntfConfigs vval;
      vval.mutable_id()->set_value(index);
      vval.mutable_vlan()->set_value(limit.vlan());
      vval.mutable_mtu()->set_value(limit.mtu());
      vval.mutable_cpu_queue()->set_value(limit.cpu_queue());

      switch (limit.purpose()) {
        case google::hercules::GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER:
          vval.set_purpose(
              oc::Bcm::HerculesBcmChip::BCM_KNET_IF_PURPOSE_CONTROLLER);
          break;
        case google::hercules::GoogleConfig::BCM_KNET_INTF_PURPOSE_SFLOW:
          vval.set_purpose(oc::Bcm::HerculesBcmChip::BCM_KNET_IF_PURPOSE_SFLOW);
          break;
        case google::hercules::GoogleConfig::BCM_KNET_INTF_PURPOSE_UNKNOWN:
          vval.set_purpose(
              oc::Bcm::HerculesBcmChip::BCM_KNET_IF_PURPOSE_UNKNOWN);
          break;
        default:
          // Hmm...
          LOG(ERROR) << "Unknow 'purpose': " << limit.ShortDebugString();
          break;
      }

      (*val.mutable_knet_intf_configs())[index] = vval;
      ++index;
    }

    (*bcm_config.mutable_node_id_to_knet_config())[entry.first] = val;
  }

  for (const auto& entry : in.google_config().node_id_to_tx_config()) {
    oc::Bcm::Chassis::Config::NodeIdToTxConfig val;

    (*bcm_config.mutable_node_id_to_tx_config())[entry.first] = val;
  }

  for (const auto& entry : in.google_config().node_id_to_rx_config()) {
    oc::Bcm::Chassis::Config::NodeIdToRxConfig val;

    val.mutable_node_uid()->set_value(entry.first);
    val.mutable_max_burst_pkts()->set_value(entry.second.max_burst_pkts());
    val.mutable_rx_pool_bytes_per_pkt()->set_value(
        entry.second.rx_pool_bytes_per_pkt());
    val.mutable_max_pkt_size_bytes()->set_value(
        entry.second.max_pkt_size_bytes());
    val.mutable_pkts_per_chain()->set_value(entry.second.pkts_per_chain());
    val.mutable_max_rate_pps()->set_value(entry.second.max_rate_pps());
    val.mutable_rx_pool_pkt_count()->set_value(
        entry.second.rx_pool_pkt_count());
    val.mutable_use_interrupt()->set_value(entry.second.use_interrupt());

    for (const auto& limit : entry.second.dma_channel_configs()) {
      oc::Bcm::Chassis::Config::NodeIdToRxConfig::DmaChannelConfigs vval;
      vval.mutable_id()->set_value(limit.first);
      vval.mutable_strip_vlan()->set_value(limit.second.strip_vlan());
      vval.mutable_oversized_packets_ok()->set_value(
          limit.second.oversized_packets_ok());
      vval.mutable_no_pkt_parsing()->set_value(limit.second.no_pkt_parsing());
      for (int32_t cos : limit.second.cos_set()) {
        vval.add_cos_set()->set_value(cos);
      }
      vval.mutable_chains()->set_value(limit.second.chains());
      vval.mutable_strip_crc()->set_value(limit.second.strip_crc());

      (*val.mutable_dma_channel_configs())[limit.first] = vval;
    }

    (*bcm_config.mutable_node_id_to_rx_config())[entry.first] = val;
  }

  for (const auto& entry : in.google_config().node_id_to_rate_limit_config()) {
    oc::Bcm::Chassis::Config::NodeIdToRateLimitConfig val;

    val.mutable_node_uid()->set_value(entry.first);

    for (const auto& limit : entry.second.per_cos_rate_limit_configs()) {
      oc::Bcm::Chassis::Config::NodeIdToRateLimitConfig::PerCosRateLimitConfigs
          vval;
      vval.mutable_id()->set_value(limit.first);
      vval.mutable_max_rate_pps()->set_value(limit.second.max_rate_pps());
      vval.mutable_max_burst_pkts()->set_value(limit.second.max_burst_pkts());

      (*val.mutable_per_cos_rate_limit_configs())[limit.first] = vval;
    }

    val.mutable_max_rate_pps()->set_value(entry.second.max_rate_pps());
    val.mutable_max_burst_pkts()->set_value(entry.second.max_rate_pps());

    (*bcm_config.mutable_node_id_to_rate_limit_config())[entry.first] = val;
  }

  component.mutable_chassis()
      ->mutable_config()
      ->mutable_vendor_specific()
      ->PackFrom(bcm_config);

  return std::list<oc::Components::Component>({component});
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   google::hercules::SingletonPort
// into:
//   std::list<oc::Components::Component>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Components::Component>>
SingletonPortToComponents(const google::hercules::SingletonPort& in) {
  std::list<oc::Components::Component> ret;

  std::string transceiver_name =
      absl::Substitute(":txrx-$0/$1", in.slot(), in.port());
  oc::Components::Component transceiver;
  transceiver.set_type(oc::OpenconfigPlatformTypes::HW_TRANSCEIVER);
  transceiver.mutable_config()->mutable_name()->set_value(transceiver_name);

  oc::Components::Component::Transceiver::PhysicalChannels::Channel channel;
  channel.mutable_config()->mutable_index()->set_value(in.channel());
  channel.mutable_index()->set_value(in.channel());
  (*transceiver.mutable_transceiver()
        ->mutable_physical_channels()
        ->mutable_channel())[in.channel()] = channel;

  ret.push_back(transceiver);

  oc::Components::Component::Subcomponents::Subcomponent reference;
  reference.mutable_name()->set_value(transceiver_name);
  reference.mutable_config()->mutable_name()->set_value(transceiver_name);

  oc::Components::Component port;
  port.set_type(oc::OpenconfigPlatformTypes::HW_PORT);
  port.mutable_config()->mutable_slot_id()->set_value(in.slot());
  port.mutable_config()->mutable_port_id()->set_value(in.port());
  port.mutable_config()->mutable_channel_id()->set_value(in.channel());
  port.mutable_config()->mutable_name()->set_value(in.name());

  (*port.mutable_subcomponents()->mutable_subcomponent())[transceiver_name] =
      reference;

  ret.push_back(port);
  return ret;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   google::hercules::SingletonPort
// into:
//   std::list<oc::Interfaces::Interface>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Interfaces::Interface>>
SingletonPortToInterfaces(const google::hercules::SingletonPort& in) {
  oc::Interfaces::Interface singleton;

  singleton.mutable_name()->set_value(in.name());
  // config
  singleton.mutable_config()->set_type(oc::IetfInterfaces::ETHERNET_CSMACD);
  singleton.mutable_config()->mutable_enabled()->set_value(true);
  singleton.mutable_config()->mutable_name()->set_value(in.name());
  singleton.mutable_config()->mutable_hardware_port()->set_value(in.name());
  // ethernet
  singleton.mutable_ethernet()->mutable_config()->set_duplex_mode(
      ::oc::Interfaces::Interface::Ethernet::Config::FULL);
  switch (in.speed_bps()) {
    case 10000000:  // 10Mbps
      singleton.mutable_ethernet()->mutable_config()->set_port_speed(
          oc::OpenconfigIfEthernet::SPEED_10MB);
      break;
    case 100000000:  // 100Mbps
      singleton.mutable_ethernet()->mutable_config()->set_port_speed(
          oc::OpenconfigIfEthernet::SPEED_100MB);
      break;
    case 1000000000:  // 1Gbps
      singleton.mutable_ethernet()->mutable_config()->set_port_speed(
          oc::OpenconfigIfEthernet::SPEED_1GB);
      break;
    case kTenGigBps:  // 10Gbps
      singleton.mutable_ethernet()->mutable_config()->set_port_speed(
          oc::OpenconfigIfEthernet::SPEED_10GB);
      break;
    case kTwentyFiveGigBps:  // 25Gbps
      singleton.mutable_ethernet()->mutable_config()->set_port_speed(
          oc::OpenconfigIfEthernet::SPEED_25GB);
      break;
    case kFortyGigBps:  // 40Gbps
      singleton.mutable_ethernet()->mutable_config()->set_port_speed(
          oc::OpenconfigIfEthernet::SPEED_40GB);
      break;
    case kFiftyGigBps:  // 50Gbps
      singleton.mutable_ethernet()->mutable_config()->set_port_speed(
          oc::OpenconfigIfEthernet::SPEED_50GB);
      break;
    case kHundredGigBps:  // 100Gbps
      singleton.mutable_ethernet()->mutable_config()->set_port_speed(
          oc::OpenconfigIfEthernet::SPEED_100GB);
      break;
    default:
      LOG(ERROR) << "unknown 'speed_bps' " << in.ShortDebugString();
      break;
  }
  singleton.mutable_ethernet()
      ->mutable_config()
      ->mutable_enable_flow_control()
      ->set_value(true);
  singleton.mutable_ethernet()
      ->mutable_config()
      ->mutable_auto_negotiate()
      ->set_value(false);

  // state
  // hold_time
  // hercules_interface
  singleton.mutable_hercules_interface()
      ->mutable_config()
      ->mutable_uid()
      ->set_value(in.id());
  // subinterfaces
  // aggregation
  return std::list<oc::Interfaces::Interface>({singleton});
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   google::hercules::TrunkPOrt
// into:
//   std::list<oc::Components::Component>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Components::Component>> TrunkPortToComponents(
    const google::hercules::TrunkPort& in) {
  std::list<oc::Components::Component> ret;

  oc::Components::Component port;
  port.set_type(oc::OpenconfigPlatformTypes::HW_PORT);
  port.mutable_config()->mutable_name()->set_value(in.name());

  ret.push_back(port);
  return ret;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   google::hercules::TrunkPort
// into:
//   std::list<oc::Interfaces::Interface>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Interfaces::Interface>> TrunkPortToInterfaces(
    const google::hercules::ChassisConfig& root,
    const google::hercules::TrunkPort& in) {
  std::list<oc::Interfaces::Interface> ret;
  oc::Interfaces::Interface trunk;

  trunk.mutable_name()->set_value(in.name());
  // config
  trunk.mutable_config()->set_type(oc::IetfInterfaces::IEEE_8023_AD_LAG);
  trunk.mutable_config()->mutable_enabled()->set_value(true);
  trunk.mutable_config()->mutable_name()->set_value(in.name());
  // ethernet
  // state
  // hold_time
  // hercules_interface
  trunk.mutable_hercules_interface()
      ->mutable_config()
      ->mutable_uid()
      ->set_value(in.id());
  // subinterfaces
  // aggregation
  std::map<int64, std::string> id_to_name;
  for (const auto& hal_singleton : root.singleton_ports()) {
    id_to_name[hal_singleton.id()] = hal_singleton.name();
  }
  for (int64 member_id : in.members()) {
    oc::Interfaces::Interface member;

    std::string* name = gtl::FindOrNull(id_to_name, member_id);
    if (name == nullptr) {
      LOG(ERROR) << "unknown 'members' " << in.ShortDebugString();
      continue;
    }

    member.mutable_name()->set_value(*name);
    member.mutable_config()->mutable_name()->set_value(*name);
    member.mutable_ethernet()
        ->mutable_config()
        ->mutable_aggregate_id()
        ->set_value(in.name());

    ret.push_back(member);
  }
  ret.push_back(trunk);
  return ret;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// converts:
//   google::hercules::ChassisConfig
// into:
//   oc::Device
////////////////////////////////////////////////////////////////////////////////
#define MERGE_ALL_COMPONENTS(list)                                  \
  for (const auto& component : list) {                              \
    (*to.mutable_components()                                       \
          ->mutable_component())[component.config().name().value()] \
        .MergeFrom(component);                                      \
  }

#define MERGE_ALL_INTERFACES(list)                                  \
  for (const auto& interface : list) {                              \
    (*to.mutable_interfaces()                                       \
          ->mutable_interface())[interface.config().name().value()] \
        .MergeFrom(interface);                                      \
  }

::util::StatusOr<oc::Device>
HalConfigToOpenConfigProtoConverter::ChassisConfigToDevice(
    const google::hercules::ChassisConfig& in) {
  oc::Device to;

  // Handle 'description' field.
  // Nothing to do here.

  // Handle 'chassis' field.
  ASSIGN_OR_RETURN(auto components, ChassisToComponent(in.chassis()));
  MERGE_ALL_COMPONENTS(components);

  // Handle 'nodes' repeated field.
  for (const auto& hal_node : in.nodes()) {
    ASSIGN_OR_RETURN(auto nodes, NodeToComponent(hal_node));
    MERGE_ALL_COMPONENTS(nodes);
  }

  // Handle 'singleton_ports' repeated field.
  for (const auto& hal_singleton : in.singleton_ports()) {
    ASSIGN_OR_RETURN(auto components, SingletonPortToComponents(hal_singleton));
    MERGE_ALL_COMPONENTS(components);
    ASSIGN_OR_RETURN(auto interfaces, SingletonPortToInterfaces(hal_singleton));
    MERGE_ALL_INTERFACES(interfaces);
  }

  // Handle 'trunk_ports' repeated field.
  for (const auto& hal_trunk : in.trunk_ports()) {
    ASSIGN_OR_RETURN(auto components, TrunkPortToComponents(hal_trunk));
    MERGE_ALL_COMPONENTS(components);
    ASSIGN_OR_RETURN(auto interfaces, TrunkPortToInterfaces(in, hal_trunk));
    MERGE_ALL_INTERFACES(interfaces);
  }

  // Handle 'port_groups' repeated field.
  // Nothing to do here.

  // Handle 'vendor_config' repeated field.
  oc::Components::Component* vendor =
      &(*to.mutable_components()->mutable_component())[in.chassis().name()];
  ASSIGN_OR_RETURN(auto vendor_components,
                   VendorConfigToComponent(in.vendor_config()));
  for (const auto& component : vendor_components) {
    vendor->MergeFrom(component);
  }

  LOG(INFO) << "Output { " << to.ShortDebugString() << " }";
  return to;
}

}  // namespace hal
}  // namespace stratum
