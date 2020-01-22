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

#include "stratum/hal/lib/common/openconfig_converter.h"

#include <iostream>
#include <list>
#include <map>
#include <string>

#include "absl/strings/substitute.h"
#include "github.com/openconfig/ygot/proto/ywrapper/ywrapper.pb.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/lib/constants.h"

namespace stratum {

namespace hal {

namespace {

using namespace openconfig::enums;  // NOLINT

////////////////////////////////////////////////////////////////////////////////
// converts:
//   Node
// to:
//   std::list<openconfig::Device::ComponentKey>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<openconfig::Device::ComponentKey>> NodeToComponent(
    const Node &in) {
  std::string linecard_name = absl::Substitute(":lc-$0", in.slot());
  std::string component_id = std::to_string(in.id());

  openconfig::Device::ComponentKey component_key;
  component_key.set_name(linecard_name);
  auto component = component_key.mutable_component();
  component->mutable_id()->set_value(component_id);
  auto linecard = component->mutable_linecard();
  linecard->mutable_slot_id()->set_value(std::to_string(in.slot()));

  // TODO(unknown): There are still a lot of things we are not supporting for
  // nodes, including VLAN configs. Add support for those in the YANG model as
  // well as the proto encodings. Then add support here in the code.

  return std::list<openconfig::Device::ComponentKey>({component_key});
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   Chassis
// to:
//   std::list<openconfig::Device::ComponentKey>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<openconfig::Component> ChassisToComponent(const Chassis &in) {
  openconfig::Component component;
  auto chassis = component.mutable_chassis();

  // TODO(Yi Tseng): platform from yang model does not fit to platform from
  // the common.proto
  switch (in.platform()) {
    default:
      chassis->set_platform(OPENCONFIGHERCULESPLATFORMPLATFORMTYPE_GENERIC);
      break;
  }
  return component;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   VendorConfig
// to:
//   oc::Bcm::Chassis::Config
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<oc::Bcm::Chassis::Config> VendorConfigToBcmConfig(
    const VendorConfig &in) {
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
              oc::Bcm::StratumBcmChip::BCM_KNET_IF_PURPOSE_CONTROLLER);
          break;
        case GoogleConfig::BCM_KNET_INTF_PURPOSE_SFLOW:
          oc_intf_config.set_purpose(
              oc::Bcm::StratumBcmChip::BCM_KNET_IF_PURPOSE_SFLOW);
          break;
        case GoogleConfig::BCM_KNET_INTF_PURPOSE_UNKNOWN:
          oc_intf_config.set_purpose(
              oc::Bcm::StratumBcmChip::BCM_KNET_IF_PURPOSE_UNKNOWN);
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
  bcm_config.mutable_bcm_chassis_map_id()->set_value(
      in.google_config().bcm_chassis_map_id());
  return bcm_config;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   SingletonPort
// to:
//   std::list<openconfig::Device::ComponentKey>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<openconfig::Device::ComponentKey>>
SingletonPortToComponents(const SingletonPort &in) {
  openconfig::Device::ComponentKey component_key;
  component_key.set_name(in.name());
  auto component = component_key.mutable_component();
  auto transceiver = component->mutable_transceiver();

  switch (in.config_params().fec_mode()) {
    case FEC_MODE_UNKNOWN:
      transceiver->set_fec_mode(OPENCONFIGPLATFORMTYPESFECMODETYPE_UNSET);
      break;
    case FEC_MODE_ON:
      transceiver->set_fec_mode(OPENCONFIGPLATFORMTYPESFECMODETYPE_FEC_ENABLED);
      break;
    case FEC_MODE_OFF:
      transceiver->set_fec_mode(
          OPENCONFIGPLATFORMTYPESFECMODETYPE_FEC_DISABLED);
      break;
    case FEC_MODE_AUTO:
      transceiver->set_fec_mode(OPENCONFIGPLATFORMTYPESFECMODETYPE_FEC_AUTO);
      break;
    default:
      transceiver->set_fec_mode(OPENCONFIGPLATFORMTYPESFECMODETYPE_UNSET);
      break;
  }

  auto channel_key = transceiver->add_channel();

  channel_key->set_index(in.channel());

  auto subcomponent_key = component->add_subcomponent();
  subcomponent_key->set_name(in.name());
  auto port = component->mutable_port();
  port->mutable_port_id()->set_value(in.id());

  // No slot-id from component.port or interface,
  // here we could store the linecard of this port
  auto linecard = component->mutable_linecard();
  linecard->mutable_slot_id()->set_value(std::to_string(in.slot()));

  // No node-id from component.port or interface
  // here we could store the integrated circuit of this port
  auto integrated_circuit = component->mutable_integrated_circuit();
  integrated_circuit->mutable_node_id()->set_value(in.node());

  return std::list<openconfig::Device::ComponentKey>({component_key});
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   SingletonPort
// to:
//   std::list<openconfig::InterfaceKey>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<openconfig::Device::InterfaceKey>>
SingletonPortToInterfaces(const SingletonPort &in) {
  openconfig::Device::InterfaceKey interface_key;
  interface_key.set_name(in.name());
  auto interface = interface_key.mutable_interface();

  // SingletonPort.id -> /interfaces/interface/state/id(ifindex)
  interface->mutable_id()->set_value(in.id());
  interface->mutable_ifindex()->set_value(in.id());

  // SingletonPort.speed_bps -> /interfaces/interface/ethernet/config/port-speed
  switch (in.speed_bps()) {
    case 10000000:  // 10Mbps
      interface->mutable_ethernet()->set_port_speed(
          OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_10MB);
      break;
    case 100000000:  // 100Mbps
      interface->mutable_ethernet()->set_port_speed(
          OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_100MB);
      break;
    case 1000000000:  // 1Gbps
      interface->mutable_ethernet()->set_port_speed(
          OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_1GB);
      break;
    case kTenGigBps:  // 10Gbps
      interface->mutable_ethernet()->set_port_speed(
          OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_10GB);
      break;
    case kTwentyFiveGigBps:  // 25Gbps
      interface->mutable_ethernet()->set_port_speed(
          OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_25GB);
      break;
    case kFortyGigBps:  // 40Gbps
      interface->mutable_ethernet()->set_port_speed(
          OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_40GB);
      break;
    case kFiftyGigBps:  // 50Gbps
      interface->mutable_ethernet()->set_port_speed(
          OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_50GB);
      break;
    case kHundredGigBps:  // 100Gbps
      interface->mutable_ethernet()->set_port_speed(
          OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_100GB);
      break;
    default:
      RETURN_ERROR(ERR_INVALID_PARAM)
          << "unknown 'speed_bps' " << in.ShortDebugString();
  }

  // SingletonPort.config_params.admin_state
  // -> /interfaces/interface/config/enabled
  if (in.config_params().admin_state() != ADMIN_STATE_UNKNOWN) {
    interface->mutable_enabled()->set_value(
        IsAdminStateEnabled(in.config_params().admin_state()));
  }

  // SingletonPort.config_params.autoneg
  // -> /interfaces/interface/ethernet/config/auto-negotiate
  interface->mutable_ethernet()->mutable_auto_negotiate()->set_value(
      IsPortAutonegEnabled(in.config_params().autoneg()));

  // FIXME(Yi Tseng): Should we use other field to store interface channel?
  interface->add_physical_channel()->set_value(in.channel());

  // subinterfaces
  // aggregation
  return std::list<openconfig::Device::InterfaceKey>({interface_key});
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   TrunkPort
// to:
//   std::list<openconfig::Device::ComponentKey>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<openconfig::Device::ComponentKey>>
TrunkPortToComponents(const TrunkPort &in) {
  openconfig::Device::ComponentKey component_key;
  component_key.set_name(in.name());
  auto component = component_key.mutable_component();
  component->mutable_port()->mutable_port_id()->set_value(in.id());
  return std::list<openconfig::Device::ComponentKey>({component_key});
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   TrunkPort
// to:
//   std::list<openconfig::Device::InterfaceKey>
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<std::list<openconfig::Device::InterfaceKey>>
TrunkPortToInterfaces(const ChassisConfig &root, const TrunkPort &in) {
  openconfig::Device::InterfaceKey interface_key;
  interface_key.set_name(in.name());
  auto trunk = interface_key.mutable_interface();

  // SingletonPort.id -> /interfaces/interface/state/id(ifindex)
  trunk->mutable_id()->set_value(in.id());
  trunk->mutable_ifindex()->set_value(in.id());

  // SingletonPort.config_params.admin_state
  // -> /interfaces/interface/config/enabled
  trunk->mutable_enabled()->set_value(
      IsAdminStateEnabled(in.config_params().admin_state()));

  switch (in.type()) {
    case TrunkPort::LACP_TRUNK:
      trunk->mutable_aggregation()->set_lag_type(
          OPENCONFIGIFAGGREGATEAGGREGATIONTYPE_LACP);
      break;
    case TrunkPort::STATIC_TRUNK:
      trunk->mutable_aggregation()->set_lag_type(
          OPENCONFIGIFAGGREGATEAGGREGATIONTYPE_STATIC);
      break;
    default:
      RETURN_ERROR(ERR_INVALID_PARAM) << "unknown trunk type " << in.type();
  }

  std::map<int64, std::string> id_to_name;
  for (const auto &hal_singleton : root.singleton_ports()) {
    id_to_name[hal_singleton.id()] = hal_singleton.name();
  }

  for (int64 member_id : in.members()) {
    std::string *name = gtl::FindOrNull(id_to_name, member_id);
    if (name == nullptr) {
      RETURN_ERROR(ERR_INVALID_PARAM)
          << "unknown 'members' " << in.ShortDebugString();
    }

    auto member = trunk->mutable_aggregation()->add_member();
    member->set_value(*name);
  }
  return std::list<openconfig::Device::InterfaceKey>({interface_key});
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   openconfig::Device + openconfig::Device::ComponentKey
// to:
//   Chassis
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<Chassis> ComponentToChassis(
    const openconfig::Device &device,
    const openconfig::Device::ComponentKey &component_key) {
  Chassis to;
  to.set_name(component_key.name());
  auto component = component_key.component();

  switch (component.chassis().platform()) {
    case OPENCONFIGHERCULESPLATFORMPLATFORMTYPE_GENERIC_TRIDENT_PLUS:
      to.set_platform(PLT_GENERIC_TRIDENT2);
      break;
    case OPENCONFIGHERCULESPLATFORMPLATFORMTYPE_GENERIC_TRIDENT2:
      to.set_platform(PLT_GENERIC_TOMAHAWK);
      break;
    case OPENCONFIGHERCULESPLATFORMPLATFORMTYPE_GENERIC_TOMAHAWK:
      to.set_platform(PLT_GENERIC_TRIDENT_PLUS);
      break;
    case OPENCONFIGHERCULESPLATFORMPLATFORMTYPE_MLNX_SN2700:
      to.set_platform(PLT_MLNX_SN2700);
      break;
    case OPENCONFIGHERCULESPLATFORMPLATFORMTYPE_P4_SOFT_SWITCH:
      to.set_platform(PLT_P4_SOFT_SWITCH);
      break;
    case OPENCONFIGHERCULESPLATFORMPLATFORMTYPE_BAREFOOT_TOFINO:
      to.set_platform(PLT_BAREFOOT_TOFINO);
      break;
    case OPENCONFIGHERCULESPLATFORMPLATFORMTYPE_BAREFOOT_TOFINO2:
      to.set_platform(PLT_BAREFOOT_TOFINO2);
      break;
    default:
      to.set_platform(PLT_UNKNOWN);
      break;
  }
  return to;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   openconfig::Device + openconfig::Device::ComponentKey
// to:
//   Node
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<Node> ComponentToNode(
    const openconfig::Device &device,
    const openconfig::Device::ComponentKey &component_key) {
  Node to;
  auto component = component_key.component();

  to.set_id(std::stoi(component.id().value()));
  to.set_name(component_key.name());

  auto linecard = component.linecard();
  // No need to check if linecard component is present. This method will not be
  // called if it is missing.
  to.set_slot(std::stoi(linecard.slot_id().value()));

  // TODO(Yi): no index defined in the model
  // to.set_index();

  // TODO(unknown): For now by default disable learning on default VLAN.
  // This will eventually come from gNMI.
  auto *vlan_config = to.mutable_config_params()->add_vlan_configs();
  vlan_config->set_block_broadcast(false);
  vlan_config->set_block_known_multicast(false);
  vlan_config->set_block_unknown_multicast(true);
  vlan_config->set_block_unknown_unicast(true);
  vlan_config->set_disable_l2_learning(true);

  // TODO(unknown): There are still a lot of things we are not supporting for
  // nodes, including VLAN configs. Add support for those in the YANG model as
  // well as the proto encodings.  Then add support here in the code.

  return to;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   openconfig::Device::ComponentKey
// to:
//   GoogleConfig
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<GoogleConfig> ComponentToChassisBcmChipSpecific(
    const openconfig::Device::ComponentKey &component_key) {
  auto component = component_key.component();
  GoogleConfig to;

  if (component.chassis().vendor_specific().Is<oc::Bcm::Chassis::Config>()) {
    oc::Bcm::Chassis::Config bcm_specific;
    component.chassis().vendor_specific().UnpackTo(&bcm_specific);

    *to.mutable_bcm_chassis_map_id() =
        bcm_specific.bcm_chassis_map_id().value();

    // map<int32, NodeIdToKnetConfig> node_id_to_knet_config
    for (const auto &entry : bcm_specific.node_id_to_knet_config()) {
      GoogleConfig::BcmKnetConfig conf;
      for (const auto &config : entry.second.knet_intf_configs()) {
        GoogleConfig::BcmKnetConfig::BcmKnetIntfConfig intf;

        switch (config.second.purpose()) {
          case oc::Bcm::StratumBcmChip::BCM_KNET_IF_PURPOSE_CONTROLLER:
            intf.set_purpose(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER);
            break;
          case oc::Bcm::StratumBcmChip::BCM_KNET_IF_PURPOSE_SFLOW:
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
//   openconfig::Device + openconfig::InterfaceKey
// to:
//   TrunkPort
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<TrunkPort> InterfaceToTrunkPort(
    const openconfig::Device &device,
    const openconfig::Device::InterfaceKey &interface_key) {
  TrunkPort to;
  auto interface = interface_key.interface();

  to.set_id(interface.id().value());
  to.set_name(interface_key.name());

  switch (interface.aggregation().lag_type()) {
    case OPENCONFIGIFAGGREGATEAGGREGATIONTYPE_LACP:
      to.set_type(TrunkPort::LACP_TRUNK);
      break;
    case OPENCONFIGIFAGGREGATEAGGREGATIONTYPE_STATIC:
      to.set_type(TrunkPort::STATIC_TRUNK);
      break;
    default:
      break;
  }

  std::map<std::string, int64> name_to_id;
  for (const auto &entry : device.interface()) {
    const auto &interface_name = entry.name();
    const auto &interface_id = entry.interface().id().value();
    name_to_id[interface_name] = interface_id;
  }

  for (const auto &member_name : interface.aggregation().member()) {
    auto id = gtl::FindOrNull(name_to_id, member_name.value());
    if (id == nullptr) {
      LOG(ERROR) << "unknown 'members' " << member_name.value();
      continue;
    }
    to.add_members(*id);
  }

  return to;
}

////////////////////////////////////////////////////////////////////////////////
// converts:
//   openconfig::Device + openconfig::Interfaces::Interface
// to:
//   SingletonPort
////////////////////////////////////////////////////////////////////////////////
::util::StatusOr<SingletonPort> InterfaceToSingletonPort(
    const openconfig::Device &device,
    const openconfig::Device::InterfaceKey &interface_key) {
  SingletonPort to;
  auto &interface = interface_key.interface();
  to.set_id(interface.id().value());
  to.set_name(interface_key.name());

  openconfig::Device::ComponentKey if_component_key;
  for (auto &component_key : device.component()) {
    if (component_key.name() == interface_key.name()) {
      if_component_key.CopyFrom(component_key);
      break;
    }
  }

  if (!if_component_key.has_component()) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Cannot find component for interface " << interface_key.name();
  }

  const auto &if_component = if_component_key.component();

  to.set_slot(std::stoi(if_component.linecard().slot_id().value()));
  to.set_port(if_component.port().port_id().value());
  to.set_node(if_component.integrated_circuit().node_id().value());

  switch (interface.ethernet().port_speed()) {
    case OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_10MB:
      to.set_speed_bps(10000000);
      break;
    case OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_100MB:
      to.set_speed_bps(100000000);
      break;
    case OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_1GB:
      to.set_speed_bps(1000000000);
      break;
    case OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_10GB:
      to.set_speed_bps(kTenGigBps);
      break;
    case OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_25GB:
      to.set_speed_bps(kTwentyFiveGigBps);
      break;
    case OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_40GB:
      to.set_speed_bps(kFortyGigBps);
      break;
    case OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_50GB:
      to.set_speed_bps(kFiftyGigBps);
      break;
    case OPENCONFIGIFETHERNETETHERNETSPEED_SPEED_100GB:
      to.set_speed_bps(kHundredGigBps);
      break;
    default:
      RETURN_ERROR(ERR_INVALID_PARAM)
          << "Invalid interface speed " << interface.ethernet().port_speed();
  }

  auto config_params = to.mutable_config_params();

  if (interface.ethernet().has_auto_negotiate()) {
    if (interface.ethernet().auto_negotiate().value())
      config_params->set_autoneg(TRI_STATE_TRUE);
    else
      config_params->set_autoneg(TRI_STATE_FALSE);
  }

  switch (if_component.transceiver().fec_mode()) {
    case OPENCONFIGPLATFORMTYPESFECMODETYPE_UNSET:
      config_params->set_fec_mode(FEC_MODE_UNKNOWN);
      break;
    case OPENCONFIGPLATFORMTYPESFECMODETYPE_FEC_ENABLED:
      config_params->set_fec_mode(FEC_MODE_ON);
      break;
    case OPENCONFIGPLATFORMTYPESFECMODETYPE_FEC_DISABLED:
      config_params->set_fec_mode(FEC_MODE_OFF);
      break;
    case OPENCONFIGPLATFORMTYPESFECMODETYPE_FEC_AUTO:
      config_params->set_fec_mode(FEC_MODE_AUTO);
      break;
  }

  // FIXME(Yi Tseng): Should we use other field to store interface channel?
  for (auto &channel : interface.physical_channel()) {
    to.set_channel(channel.value());
    break;
  }

  if (interface.has_enabled()) {
    config_params->set_admin_state(interface.enabled().value()
                                       ? ADMIN_STATE_ENABLED
                                       : ADMIN_STATE_DISABLED);
  }

  return to;
}

////////////////////////////////////////////////////////////////////////////////
// Some useful macros used in the code.
////////////////////////////////////////////////////////////////////////////////
#define MERGE_ALL_COMPONENTS(list)               \
  for (const auto &component_key : (list)) {     \
    to.add_component()->CopyFrom(component_key); \
  }

#define MERGE_ALL_INTERFACES(list)               \
  for (const auto &interface_key : (list)) {     \
    to.add_interface()->CopyFrom(interface_key); \
  }

}  // namespace

::util::StatusOr<openconfig::Device>
OpenconfigConverter::ChassisConfigToOcDevice(const ChassisConfig &in) {
  openconfig::Device to;

  // Handle 'description' field.
  // Nothing to do here.

  // Handle 'chassis' field.
  ASSIGN_OR_RETURN(auto chassis_component, ChassisToComponent(in.chassis()));

  if (in.has_vendor_config()) {
    ASSIGN_OR_RETURN(auto vendor_config,
                     VendorConfigToBcmConfig(in.vendor_config()));
    chassis_component.mutable_chassis()->mutable_vendor_specific()->PackFrom(
        vendor_config);
  }

  auto ckey = to.add_component();
  ckey->set_name(in.chassis().name());
  ckey->mutable_component()->CopyFrom(chassis_component);

  //  LOG(INFO) << to.DebugString();

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

  VLOG(1) << "The convetred openconfig::Device proto:\n"
          << to.ShortDebugString();

  return to;
}

::util::StatusOr<ChassisConfig> OpenconfigConverter::OcDeviceToChassisConfig(
    const openconfig::Device &in) {
  ChassisConfig to;

  // Validate the input before doing anything.
  RETURN_IF_ERROR(ValidateOcDeviceProto(in));

  for (const auto &compoennt_key : in.component()) {
    const auto &component = compoennt_key.component();
    if (component.has_chassis()) {
      // Set chassis field.
      ASSIGN_OR_RETURN(*to.mutable_chassis(),
                       ComponentToChassis(in, compoennt_key));
      if (component.chassis().has_vendor_specific()) {
        // Set vendor_config.google_config field.
        ASSIGN_OR_RETURN(*to.mutable_vendor_config()->mutable_google_config(),
                         ComponentToChassisBcmChipSpecific(compoennt_key));
      }
    }
    // There is no type defined in the model, need to determine which type of
    // component by using fields stores in this component
    if (component.has_linecard() && !component.has_port()) {
      ASSIGN_OR_RETURN(*to.add_nodes(), ComponentToNode(in, compoennt_key));
    }
  }

  for (const auto &interface_key : in.interface()) {
    const auto &interface = interface_key.interface();
    if (interface.has_aggregation()) {
      // Trunk port
      ASSIGN_OR_RETURN(*to.add_trunk_ports(),
                       InterfaceToTrunkPort(in, interface_key));
    } else {
      // Singleton port
      ASSIGN_OR_RETURN(*to.add_singleton_ports(),
                       InterfaceToSingletonPort(in, interface_key));
    }
  }

  VLOG(1) << "The converted ChassisConfig proto:\n" << to.ShortDebugString();

  return to;
}

::util::Status OpenconfigConverter::ValidateOcDeviceProto(
    const openconfig::Device &in) {
  bool node_exists = false;
  bool chassis_exists = false;

  // Verify components.
  for (const auto &component_key : in.component()) {
    const auto &component = component_key.component();

    if (component.has_linecard() && !component.has_port()) {
      // A node exists
      node_exists = true;
    }

    if (component.has_chassis()) {
      // Chassis exists
      chassis_exists = true;
    }
  }

  CHECK_RETURN_IF_FALSE(node_exists);
  CHECK_RETURN_IF_FALSE(chassis_exists);

  // Verify interfaces.
  for (const auto &interface_key : in.interface()) {
    // Every interface must stores an id
    CHECK_RETURN_IF_FALSE(interface_key.interface().has_id());
  }

  return ::util::OkStatus();
}

}  // namespace hal

}  // namespace stratum
