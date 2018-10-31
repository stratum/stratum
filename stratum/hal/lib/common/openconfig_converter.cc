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

#include "stratum/hal/lib/common/openconfig_converter.h"

#include <iostream>
#include <list>
#include "stratum/glue/logging.h"
#include "stratum/public/proto/ywrapper.pb.h"
#include "stratum/lib/constants.h"
#include "absl/strings/substitute.h"
#include "stratum/glue/gtl/map_util.h"

namespace stratum {

namespace hal {

namespace {

////////////////////////////////////////////////////////////////////////////////
// converts:
//   Node
// to:
//   std::list<oc::Components::Component>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Components::Component>> NodeToComponent(
    const Node &in) {
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

  // TODO(aghaffar): There are still a lot of things we are not supporting for
  // nodes, including VLAN configs. Add support for those in the YANG model as
  // well as the proto encodings. Then add support here in the code.

  return std::list<oc::Components::Component>({linecard, node});
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   Chassis
// to:
//   std::list<oc::Components::Component>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Components::Component>> ChassisToComponent(
    const Chassis &in) {
  oc::Components::Component component;

  component.set_type(oc::OpenconfigPlatformTypes::HW_BCM_BASED_CHASSIS);
  component.mutable_config()->mutable_name()->set_value(in.name());
  component.mutable_chassis()->mutable_config()->mutable_name()->set_value(
      in.name());
  oc::Bcm::Chassis::Config bcm_config;
  switch (in.platform()) {
    case Platform::PLT_GENERIC_TRIDENT2:
      break;
    case Platform::PLT_GENERIC_TRIDENT2:
      break;
    case Platform::PLT_GENERIC_TOMAHAWK:
      break;
    case Platform::PLT_P4_SOFT_SWITCH:
      bcm_config.set_platform(oc::Bcm::HerculesChassis::P4_SOFT_SWITCH);
      break;
    case Platform::PLT_MLNX_SN2700:
      bcm_config.set_platform(oc::Bcm::HerculesChassis::MLNX_SN2700);
      break;
    default:
      // Hmm...
      LOG(ERROR) << "Unknown 'platform': " << in.ShortDebugString();
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
//   VendorConfig
// to:
//   std::list<oc::Components::Component>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Components::Component>> VendorConfigToComponent(
    const VendorConfig &in) {
  oc::Components::Component component;

  oc::Bcm::Chassis::Config bcm_config;

  for (const auto &entry : in.google_config().node_id_to_knet_config()) {
    oc::Bcm::Chassis::Config::NodeIdToKnetConfig oc_knet_cfg;
    oc_knet_cfg.mutable_node_uid()->set_value(entry.first);
    int index = 0;
    for (const auto &intf_config : entry.second.knet_intf_configs()) {
      oc::Bcm::Chassis::Config::NodeIdToKnetConfig::KnetIntfConfigs
          oc_intf_config;
      oc_intf_config.mutable_id()->set_value(index);
      oc_intf_config.mutable_vlan()->set_value(intf_config.vlan());
      oc_intf_config.mutable_mtu()->set_value(intf_config.mtu());
      oc_intf_config.mutable_cpu_queue()->set_value(intf_config.cpu_queue());

      switch (intf_config.purpose()) {
        case GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER:
          oc_intf_config.set_purpose(
              oc::Bcm::HerculesBcmChip::BCM_KNET_IF_PURPOSE_CONTROLLER);
          break;
        case GoogleConfig::BCM_KNET_INTF_PURPOSE_SFLOW:
          oc_intf_config.set_purpose(
              oc::Bcm::HerculesBcmChip::BCM_KNET_IF_PURPOSE_SFLOW);
          break;
        case GoogleConfig::BCM_KNET_INTF_PURPOSE_UNKNOWN:
          oc_intf_config.set_purpose(
              oc::Bcm::HerculesBcmChip::BCM_KNET_IF_PURPOSE_UNKNOWN);
          break;
        default:
          // Hmm...
          LOG(ERROR) << "Unknown 'purpose': " << intf_config.ShortDebugString();
          break;
      }
      (*oc_knet_cfg.mutable_knet_intf_configs())[index] = oc_intf_config;
      ++index;
    }
    (*bcm_config.mutable_node_id_to_knet_config())[entry.first] = oc_knet_cfg;
  }

  for (const auto &entry : in.google_config().node_id_to_tx_config()) {
    oc::Bcm::Chassis::Config::NodeIdToTxConfig oc_tx_cfg;
    // Nothing to do at the moment for TX config.
    (*bcm_config.mutable_node_id_to_tx_config())[entry.first] = oc_tx_cfg;
  }

  for (const auto &entry : in.google_config().node_id_to_rx_config()) {
    oc::Bcm::Chassis::Config::NodeIdToRxConfig oc_rx_cfg;
    oc_rx_cfg.mutable_node_uid()->set_value(entry.first);
    oc_rx_cfg.mutable_max_burst_pkts()->set_value(
        entry.second.max_burst_pkts());
    oc_rx_cfg.mutable_rx_pool_bytes_per_pkt()->set_value(
        entry.second.rx_pool_bytes_per_pkt());
    oc_rx_cfg.mutable_max_pkt_size_bytes()->set_value(
        entry.second.max_pkt_size_bytes());
    oc_rx_cfg.mutable_pkts_per_chain()->set_value(
        entry.second.pkts_per_chain());
    oc_rx_cfg.mutable_max_rate_pps()->set_value(entry.second.max_rate_pps());
    oc_rx_cfg.mutable_rx_pool_pkt_count()->set_value(
        entry.second.rx_pool_pkt_count());
    oc_rx_cfg.mutable_use_interrupt()->set_value(entry.second.use_interrupt());
    for (const auto &limit : entry.second.dma_channel_configs()) {
      oc::Bcm::Chassis::Config::NodeIdToRxConfig::DmaChannelConfigs
          oc_dma_ch_cfg;
      oc_dma_ch_cfg.mutable_id()->set_value(limit.first);
      oc_dma_ch_cfg.mutable_strip_vlan()->set_value(limit.second.strip_vlan());
      oc_dma_ch_cfg.mutable_oversized_packets_ok()->set_value(
          limit.second.oversized_packets_ok());
      oc_dma_ch_cfg.mutable_no_pkt_parsing()->set_value(
          limit.second.no_pkt_parsing());
      for (int32_t cos : limit.second.cos_set()) {
        oc_dma_ch_cfg.add_cos_set()->set_value(cos);
      }
      oc_dma_ch_cfg.mutable_chains()->set_value(limit.second.chains());
      oc_dma_ch_cfg.mutable_strip_crc()->set_value(limit.second.strip_crc());
      (*oc_rx_cfg.mutable_dma_channel_configs())[limit.first] = oc_dma_ch_cfg;
    }
    (*bcm_config.mutable_node_id_to_rx_config())[entry.first] = oc_rx_cfg;
  }

  for (const auto &entry : in.google_config().node_id_to_rate_limit_config()) {
    oc::Bcm::Chassis::Config::NodeIdToRateLimitConfig oc_rate_limit_cfg;
    oc_rate_limit_cfg.mutable_node_uid()->set_value(entry.first);
    for (const auto &limit : entry.second.per_cos_rate_limit_configs()) {
      oc::Bcm::Chassis::Config::NodeIdToRateLimitConfig::PerCosRateLimitConfigs
          oc_per_cos_cfg;
      oc_per_cos_cfg.mutable_id()->set_value(limit.first);
      oc_per_cos_cfg.mutable_max_rate_pps()->set_value(
          limit.second.max_rate_pps());
      oc_per_cos_cfg.mutable_max_burst_pkts()->set_value(
          limit.second.max_burst_pkts());
      (*oc_rate_limit_cfg.mutable_per_cos_rate_limit_configs())[limit.first] =
          oc_per_cos_cfg;
    }
    oc_rate_limit_cfg.mutable_max_rate_pps()->set_value(
        entry.second.max_rate_pps());
    oc_rate_limit_cfg.mutable_max_burst_pkts()->set_value(
        entry.second.max_rate_pps());
    (*bcm_config.mutable_node_id_to_rate_limit_config())[entry.first] =
        oc_rate_limit_cfg;
  }

  component.mutable_chassis()
      ->mutable_config()
      ->mutable_vendor_specific()
      ->PackFrom(bcm_config);

  return std::list<oc::Components::Component>({component});
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   SingletonPort
// to:
//   std::list<oc::Components::Component>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Components::Component>>
SingletonPortToComponents(const SingletonPort &in) {
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
//   SingletonPort
// to:
//   std::list<oc::Interfaces::Interface>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Interfaces::Interface>>
SingletonPortToInterfaces(const SingletonPort &in) {
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
//   TrunkPort
// to:
//   std::list<oc::Components::Component>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Components::Component>> TrunkPortToComponents(
    const TrunkPort &in) {
  std::list<oc::Components::Component> ret;

  oc::Components::Component port;
  port.set_type(oc::OpenconfigPlatformTypes::HW_PORT);
  port.mutable_config()->mutable_name()->set_value(in.name());

  ret.push_back(port);
  return ret;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   TrunkPort
// to:
//   std::list<oc::Interfaces::Interface>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<oc::Interfaces::Interface>> TrunkPortToInterfaces(
    const ChassisConfig &root, const TrunkPort &in) {
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
  for (const auto &hal_singleton : root.singleton_ports()) {
    id_to_name[hal_singleton.id()] = hal_singleton.name();
  }
  for (int64 member_id : in.members()) {
    oc::Interfaces::Interface member;

    std::string *name = gtl::FindOrNull(id_to_name, member_id);
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

////////////////////////////////////////////////////////////////////////////////
// converts:
//   oc::Device + oc::Components::Component
// to:
//   Chassis
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<Chassis> ComponentToChassis(
    const oc::Device &device, const oc::Components::Component &component) {
  Chassis to;

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
// to:
//   Node
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<Node> ComponentToNode(
    const oc::Device &device, const oc::Components::Component &component) {
  Node to;

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

  // TODO(aghaffar): For now by default disable learning on default VLAN.
  // This will eventually come from gNMI.
  auto *vlan_config = to.mutable_config_params()->add_vlan_configs();
  vlan_config->set_block_broadcast(false);
  vlan_config->set_block_known_multicast(false);
  vlan_config->set_block_unknown_multicast(true);
  vlan_config->set_block_unknown_unicast(true);
  vlan_config->set_disable_l2_learning(true);

  // TODO(aghaffar): There are still a lot of things we are not supporting for
  // nodes, including VLAN configs. Add support for those in the YANG model as
  // well as the proto encodings.  Then add support here in the code.

  return to;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   oc::Device + oc::Components::Component
// to:
//   GoogleConfig
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<GoogleConfig> ComponentToChassisBcmChipSpecific(
    const oc::Device &device, const oc::Components::Component &component) {
  GoogleConfig to;

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
      GoogleConfig::BcmKnetConfig conf;
      for (const auto &config : entry.second.knet_intf_configs()) {
        GoogleConfig::BcmKnetConfig::BcmKnetIntfConfig intf;

        switch (config.second.purpose()) {
          case oc::Bcm::HerculesBcmChip::BCM_KNET_IF_PURPOSE_CONTROLLER:
            intf.set_purpose(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER);
            break;
          case oc::Bcm::HerculesBcmChip::BCM_KNET_IF_PURPOSE_SFLOW:
            intf.set_purpose(GoogleConfig::BCM_KNET_INTF_PURPOSE_SFLOW);
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
      GoogleConfig::BcmTxConfig config;
      (*to.mutable_node_id_to_tx_config())[entry.first] = config;
    }

    // map<int32, NodeIdToRxConfig> node_id_to_rx_config
    for (const auto &entry : bcm_specific.node_id_to_rx_config()) {
      GoogleConfig::BcmRxConfig conf;

      conf.set_rx_pool_pkt_count(entry.second.rx_pool_pkt_count().value());
      conf.set_rx_pool_bytes_per_pkt(
          entry.second.rx_pool_bytes_per_pkt().value());
      conf.set_max_pkt_size_bytes(entry.second.max_pkt_size_bytes().value());
      conf.set_pkts_per_chain(entry.second.pkts_per_chain().value());
      conf.set_max_rate_pps(entry.second.max_rate_pps().value());
      conf.set_max_burst_pkts(entry.second.max_burst_pkts().value());
      conf.set_use_interrupt(entry.second.use_interrupt().value());

      for (const auto &config : entry.second.dma_channel_configs()) {
        GoogleConfig::BcmRxConfig::BcmDmaChannelConfig a;

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
      GoogleConfig::BcmRateLimitConfig conf;

      conf.set_max_rate_pps(entry.second.max_rate_pps().value());
      conf.set_max_burst_pkts(entry.second.max_burst_pkts().value());
      for (const auto &config : entry.second.per_cos_rate_limit_configs()) {
        GoogleConfig::BcmRateLimitConfig::BcmPerCosRateLimitConfig per_cos;

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
// to:
//   TrunkPort
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<TrunkPort> InterfaceToTrunkPort(
    const oc::Device &device, const oc::Interfaces::Interface &interface) {
  TrunkPort to;

  to.set_id(interface.hercules_interface().config().uid().value());
  to.set_name(interface.config().name().value());

  switch (interface.aggregation().config().lag_type()) {
    case oc::OpenconfigIfAggregate::AGGREGATION_TYPE_LACP:
      to.set_type(TrunkPort::LACP_TRUNK);
      break;
    case oc::OpenconfigIfAggregate::AGGREGATION_TYPE_STATIC:
      to.set_type(TrunkPort::STATIC_TRUNK);
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
// to:
//   SingletonPort
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<SingletonPort> InterfaceToSingletonPort(
    const oc::Device &device, const oc::Interfaces::Interface &interface) {
  SingletonPort to;

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
  // TODO(aghaffar): This is temporary till we have the correct proto and have
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

////////////////////////////////////////////////////////////////////////////////
// Some useful macros used in the code.
////////////////////////////////////////////////////////////////////////////////
#define MERGE_ALL_COMPONENTS(list)                                  \
  for (const auto &component : list) {                              \
    (*to.mutable_components()                                       \
          ->mutable_component())[component.config().name().value()] \
        .MergeFrom(component);                                      \
  }

#define MERGE_ALL_INTERFACES(list)                                  \
  for (const auto &interface : list) {                              \
    (*to.mutable_interfaces()                                       \
          ->mutable_interface())[interface.config().name().value()] \
        .MergeFrom(interface);                                      \
  }

}  // namespace

::util::StatusOr<oc::Device> OpenconfigConverter::ChassisConfigToOcDevice(
    const ChassisConfig &in) {
  oc::Device to;

  // Handle 'description' field.
  // Nothing to do here.

  // Handle 'chassis' field.
  ASSIGN_OR_RETURN(auto components, ChassisToComponent(in.chassis()));
  MERGE_ALL_COMPONENTS(components);

  // Handle 'nodes' repeated field.
  for (const auto &hal_node : in.nodes()) {
    ASSIGN_OR_RETURN(auto nodes, NodeToComponent(hal_node));
    MERGE_ALL_COMPONENTS(nodes);
  }

  // Handle 'singleton_ports' repeated field.
  for (const auto &hal_singleton : in.singleton_ports()) {
    ASSIGN_OR_RETURN(auto components, SingletonPortToComponents(hal_singleton));
    MERGE_ALL_COMPONENTS(components);
    ASSIGN_OR_RETURN(auto interfaces, SingletonPortToInterfaces(hal_singleton));
    MERGE_ALL_INTERFACES(interfaces);
  }

  // Handle 'trunk_ports' repeated field.
  for (const auto &hal_trunk : in.trunk_ports()) {
    ASSIGN_OR_RETURN(auto components, TrunkPortToComponents(hal_trunk));
    MERGE_ALL_COMPONENTS(components);
    ASSIGN_OR_RETURN(auto interfaces, TrunkPortToInterfaces(in, hal_trunk));
    MERGE_ALL_INTERFACES(interfaces);
  }

  // Handle 'port_groups' repeated field.
  // Nothing to do here.

  // Handle 'vendor_config' repeated field.
  oc::Components::Component *vendor =
      &(*to.mutable_components()->mutable_component())[in.chassis().name()];
  ASSIGN_OR_RETURN(auto vendor_components,
                   VendorConfigToComponent(in.vendor_config()));
  for (const auto &component : vendor_components) {
    vendor->MergeFrom(component);
  }

  VLOG(1) << "The convetred oc::Device proto:\n" << to.ShortDebugString();

  return to;
}

::util::StatusOr<ChassisConfig> OpenconfigConverter::OcDeviceToChassisConfig(
    const oc::Device &in) {
  ChassisConfig to;

  // Validate the input before doing anything.
  RETURN_IF_ERROR(ValidateOcDeviceProto(in));

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

  VLOG(1) << "The converted ChassisConfig proto:\n" << to.ShortDebugString();

  return to;
}

::util::Status OpenconfigConverter::ValidateOcDeviceProto(
    const oc::Device &in) {
  // Verify components.
  for (const auto &entry : in.components().component()) {
    const oc::Components::Component &component = entry.second;
    switch (component.type()) {
      case oc::OpenconfigPlatformTypes::HW_NODE:
        // A node.
        CHECK_RETURN_IF_FALSE(
            gtl::FindOrNull(in.components().component(),
                            component.node().config().linecard().value()) !=
            nullptr)
            << "Unknown linecard: " << component.ShortDebugString();
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
        CHECK_RETURN_IF_FALSE(
            gtl::FindOrNull(in.components().component(),
                            interface.config().hardware_port().value()) !=
            nullptr)
            << "Unknown hardware_port: " << interface.ShortDebugString();
        break;
      case oc::IetfInterfaces::IEEE_8023_AD_LAG:
        // Trunk interface.
        break;
      default:
        break;
    }
  }

  return ::util::OkStatus();
}

}  // namespace hal

}  // namespace stratum
