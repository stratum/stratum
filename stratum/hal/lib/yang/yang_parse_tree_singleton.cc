// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Implements the YangParseTreePaths::AddSubtreeInterfaceFromSingleton()
// method and its supporting functions.

#include "absl/strings/str_format.h"
#include "gnmi/gnmi.pb.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/gnmi_publisher.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/hal/lib/yang/yang_parse_tree.h"
#include "stratum/hal/lib/yang/yang_parse_tree_component.h"
#include "stratum/hal/lib/yang/yang_parse_tree_helpers.h"
#include "stratum/hal/lib/yang/yang_parse_tree_paths.h"

namespace stratum {
namespace hal {

using namespace stratum::hal::yang::component;
using namespace stratum::hal::yang::helpers;

namespace {

////////////////////////////////////////////////////////////////////////////////
// /lacp/interfaces/interface[name=<name>]/state/system-id-mac
void SetUpLacpInterfacesInterfaceStateSystemIdMac(uint64 node_id,
                                                  uint32 port_id,
                                                  TreeNode* node,
                                                  YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::lacp_router_mac,
                       &DataResponse::has_lacp_router_mac,
                       &DataRequest::Request::mutable_lacp_router_mac,
                       &MacAddress::mac_address, MacAddressToYangString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortLacpRouterMacChangedEvent::GetSystemIdMac,
      MacAddressToYangString);
  auto register_functor = RegisterFunc<PortLacpRouterMacChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/state/mac-address
void SetUpInterfacesInterfaceEthernetStateMacAddress(uint64 node_id,
                                                     uint32 port_id,
                                                     TreeNode* node,
                                                     YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::mac_address,
                       &DataResponse::has_mac_address,
                       &DataRequest::Request::mutable_mac_address,
                       &MacAddress::mac_address, MacAddressToYangString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortMacAddressChangedEvent::GetMacAddress,
      MacAddressToYangString);
  auto register_functor = RegisterFunc<PortMacAddressChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/config/port-speed
void SetUpInterfacesInterfaceEthernetConfigPortSpeed(uint64 node_id,
                                                     uint32 port_id,
                                                     uint64 speed_bps,
                                                     TreeNode* node,
                                                     YangParseTree* tree) {
  auto poll_functor = [speed_bps](const GnmiEvent& event,
                                  const ::gnmi::Path& path,
                                  GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when it
    // was configured!
    return SendResponse(GetResponse(path, ConvertSpeedBpsToString(speed_bps)),
                        stream);
  };
  auto on_set_functor =
      [node_id, port_id, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    const gnmi::TypedValue* typed_val =
        dynamic_cast<const gnmi::TypedValue*>(&val);
    if (typed_val == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "not a TypedValue message!";
    }
    std::string speed_string = typed_val->string_val();
    uint64 speed_bps = ConvertStringToSpeedBps(speed_string);
    if (speed_bps == 0) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "wrong value!";
    }

    // Set the value.
    auto status = SetValue(node_id, port_id, tree,
                           &SetRequest::Request::Port::mutable_port_speed,
                           &PortSpeed::set_speed_bps, speed_bps);
    if (status != ::util::OkStatus()) {
      return status;
    }

    // Update the chassis config
    ChassisConfig* new_config = config->writable();
    for (auto& singleton_port : *new_config->mutable_singleton_ports()) {
      if (singleton_port.node() == node_id && singleton_port.id() == port_id) {
        singleton_port.set_speed_bps(speed_bps);
        break;
      }
    }

    // Update the YANG parse tree.
    auto poll_functor = [speed_string](const GnmiEvent& event,
                                       const ::gnmi::Path& path,
                                       GnmiSubscribeStream* stream) {
      // This leaf represents configuration data. Return what was known when
      // it was configured!
      return SendResponse(GetResponse(path, speed_string), stream);
    };
    node->SetOnTimerHandler(poll_functor)->SetOnPollHandler(poll_functor);

    return ::util::OkStatus();
  };
  auto register_functor = RegisterFunc<PortSpeedBpsChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortSpeedBpsChangedEvent::GetSpeedBps,
      ConvertSpeedBpsToString);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/config/auto-negotiate
void SetUpInterfacesInterfaceEthernetConfigAutoNegotiate(uint64 node_id,
                                                         uint32 port_id,
                                                         bool autoneg_status,
                                                         TreeNode* node,
                                                         YangParseTree* tree) {
  auto poll_functor = [autoneg_status](const GnmiEvent& event,
                                       const ::gnmi::Path& path,
                                       GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when it
    // was configured!
    return SendResponse(GetResponse(path, autoneg_status), stream);
  };
  auto on_set_functor =
      [node_id, port_id, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    const gnmi::TypedValue* typed_val =
        dynamic_cast<const gnmi::TypedValue*>(&val);
    if (typed_val == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "not a TypedValue message!";
    }
    bool autoneg_bool = typed_val->bool_val();
    TriState autoneg_status =
        autoneg_bool ? TriState::TRI_STATE_TRUE : TriState::TRI_STATE_FALSE;

    // Set the value.
    auto status = SetValue(node_id, port_id, tree,
                           &SetRequest::Request::Port::mutable_autoneg_status,
                           &AutonegotiationStatus::set_state, autoneg_status);
    if (status != ::util::OkStatus()) {
      return status;
    }

    // Update the chassis config
    ChassisConfig* new_config = config->writable();
    for (auto& singleton_port : *new_config->mutable_singleton_ports()) {
      if (singleton_port.node() == node_id && singleton_port.id() == port_id) {
        singleton_port.mutable_config_params()->set_autoneg(autoneg_status);
        break;
      }
    }

    // Update the YANG parse tree.
    auto poll_functor = [autoneg_bool](const GnmiEvent& event,
                                       const ::gnmi::Path& path,
                                       GnmiSubscribeStream* stream) {
      // This leaf represents configuration data. Return what was known when
      // it was configured!
      return SendResponse(GetResponse(path, autoneg_bool), stream);
    };
    node->SetOnTimerHandler(poll_functor)->SetOnPollHandler(poll_functor);

    return ::util::OkStatus();
  };
  auto register_functor = RegisterFunc<PortAutonegChangedEvent>();
  auto on_change_functor =
      GetOnChangeFunctor(node_id, port_id, &PortAutonegChangedEvent::GetState,
                         IsPortAutonegEnabled);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/config/enabled
//
void SetUpInterfacesInterfaceConfigEnabled(const bool state, uint64 node_id,
                                           uint32 port_id, TreeNode* node,
                                           YangParseTree* tree) {
  auto poll_functor = [state](const GnmiEvent& event, const ::gnmi::Path& path,
                              GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when it
    // was configured!
    return SendResponse(GetResponse(path, state), stream);
  };
  auto on_set_functor =
      [node_id, port_id, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    const gnmi::TypedValue* typed_val =
        dynamic_cast<const gnmi::TypedValue*>(&val);
    if (typed_val == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "not a TypedValue message!";
    }
    bool state_bool = typed_val->bool_val();
    AdminState typed_state = state_bool ? AdminState::ADMIN_STATE_ENABLED
                                        : AdminState::ADMIN_STATE_DISABLED;

    // Set the value.
    auto status = SetValue(node_id, port_id, tree,
                           &SetRequest::Request::Port::mutable_admin_status,
                           &AdminStatus::set_state, typed_state);
    if (status != ::util::OkStatus()) {
      return status;
    }

    // Update the chassis config
    ChassisConfig* new_config = config->writable();
    for (auto& singleton_port : *new_config->mutable_singleton_ports()) {
      if (singleton_port.node() == node_id && singleton_port.id() == port_id) {
        singleton_port.mutable_config_params()->set_admin_state(typed_state);
        break;
      }
    }

    // Update the YANG parse tree.
    auto poll_functor = [state_bool](const GnmiEvent& event,
                                     const ::gnmi::Path& path,
                                     GnmiSubscribeStream* stream) {
      // This leaf represents configuration data. Return what was known when
      // it was configured!
      return SendResponse(GetResponse(path, state_bool), stream);
    };
    node->SetOnTimerHandler(poll_functor)->SetOnPollHandler(poll_functor);

    return ::util::OkStatus();
  };
  auto register_functor = RegisterFunc<PortAdminStateChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortAdminStateChangedEvent::GetNewState,
      IsAdminStateEnabled);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/config/loopback-mode
//
void SetUpInterfacesInterfaceConfigLoopbackMode(const bool loopback,
                                                uint64 node_id, uint32 port_id,
                                                TreeNode* node,
                                                YangParseTree* tree) {
  auto poll_functor = [loopback](const GnmiEvent& event,
                                 const ::gnmi::Path& path,
                                 GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when
    // it was configured!
    return SendResponse(GetResponse(path, loopback), stream);
  };
  auto on_set_functor =
      [node_id, port_id, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    const gnmi::TypedValue* typed_val =
        dynamic_cast<const gnmi::TypedValue*>(&val);
    if (typed_val == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "not a TypedValue message!";
    }
    bool state_bool = typed_val->bool_val();
    LoopbackState typed_state = state_bool ? LoopbackState::LOOPBACK_STATE_MAC
                                           : LoopbackState::LOOPBACK_STATE_NONE;

    // Update the hardware.
    auto status = SetValue(node_id, port_id, tree,
                           &SetRequest::Request::Port::mutable_loopback_status,
                           &LoopbackStatus::set_state, typed_state);
    if (status != ::util::OkStatus()) {
      return status;
    }

    // Update the chassis config
    ChassisConfig* new_config = config->writable();
    for (auto& singleton_port : *new_config->mutable_singleton_ports()) {
      if (singleton_port.node() == node_id && singleton_port.id() == port_id) {
        singleton_port.mutable_config_params()->set_loopback_mode(typed_state);
        break;
      }
    }

    // Update the YANG parse tree.
    auto poll_functor = [state_bool](const GnmiEvent& event,
                                     const ::gnmi::Path& path,
                                     GnmiSubscribeStream* stream) {
      // This leaf represents configuration data. Return what was known when
      // it was configured!
      return SendResponse(GetResponse(path, state_bool), stream);
    };
    node->SetOnTimerHandler(poll_functor)->SetOnPollHandler(poll_functor);

    return ::util::OkStatus();
  };
  auto register_functor = RegisterFunc<PortLoopbackStateChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortLoopbackStateChangedEvent::GetNewState,
      IsLoopbackStateEnabled);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/config/mac-address
void SetUpInterfacesInterfaceEthernetConfigMacAddress(uint64 node_id,
                                                      uint32 port_id,
                                                      uint64 mac_address,
                                                      TreeNode* node,
                                                      YangParseTree* tree) {
  auto poll_functor = [mac_address](const GnmiEvent& event,
                                    const ::gnmi::Path& path,
                                    GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when it
    // was configured!
    return SendResponse(GetResponse(path, MacAddressToYangString(mac_address)),
                        stream);
  };
  auto on_change_functor = UnsupportedFunc();
  auto on_set_functor =
      [node_id, port_id, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    const gnmi::TypedValue* typed_val =
        dynamic_cast<const gnmi::TypedValue*>(&val);
    if (typed_val == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "not a TypedValue message!";
    }
    std::string mac_address_string = typed_val->string_val();
    if (!IsMacAddressValid(mac_address_string)) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "wrong value!";
    }

    uint64 mac_address = YangStringToMacAddress(mac_address_string);
    // Set the value.
    auto status = SetValue(node_id, port_id, tree,
                           &SetRequest::Request::Port::mutable_mac_address,
                           &MacAddress::set_mac_address, mac_address);
    if (status != ::util::OkStatus()) {
      return status;
    }

    // Update the chassis config
    ChassisConfig* new_config = config->writable();
    for (auto& singleton_port : *new_config->mutable_singleton_ports()) {
      if (singleton_port.node() == node_id && singleton_port.id() == port_id) {
        singleton_port.mutable_config_params()
            ->mutable_mac_address()
            ->set_mac_address(mac_address);
        break;
      }
    }

    // Update the YANG parse tree.
    auto poll_functor = [mac_address](const GnmiEvent& event,
                                      const ::gnmi::Path& path,
                                      GnmiSubscribeStream* stream) {
      // This leaf represents configuration data. Return what was known when it
      // was configured!
      return SendResponse(
          GetResponse(path, MacAddressToYangString(mac_address)), stream);
    };
    node->SetOnTimerHandler(poll_functor)->SetOnPollHandler(poll_functor);

    // Trigger change notification.
    tree->SendNotification(GnmiEventPtr(
        new PortMacAddressChangedEvent(node_id, port_id, mac_address)));

    return ::util::OkStatus();
  };
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/transceiver/state/present
void SetUpComponentsComponentTransceiverStatePresent(TreeNode* node,
                                                     YangParseTree* tree,
                                                     uint64 node_id,
                                                     uint32 port_id) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::front_panel_port_info,
      &DataResponse::has_front_panel_port_info,
      &DataRequest::Request::mutable_front_panel_port_info,
      &FrontPanelPortInfo::hw_state, ConvertHwStateToPresentString);
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/transceiver/state/serial-no
void SetUpComponentsComponentTransceiverStateSerialNo(TreeNode* node,
                                                      YangParseTree* tree,
                                                      uint64 node_id,
                                                      uint32 port_id) {
  auto poll_functor = [tree, node_id, port_id](const GnmiEvent& event,
                                               const ::gnmi::Path& path,
                                               GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_front_panel_port_info();
    request->set_node_id(node_id);
    request->set_port_id(port_id);

    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_front_panel_port_info()) return false;
      resp = in.front_panel_port_info().serial_number();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no
    // way to notify the controller that something went wrong.
    // The error is logged when it is created.
    tree->GetSwitchInterface()
        ->RetrieveValue(node_id, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    return SendResponse(GetResponse(path, resp), stream);
  };

  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/transceiver/state/vendor
void SetUpComponentsComponentTransceiverStateVendor(TreeNode* node,
                                                    YangParseTree* tree,
                                                    uint64 node_id,
                                                    uint32 port_id) {
  auto poll_functor = [tree, node_id, port_id](const GnmiEvent& event,
                                               const ::gnmi::Path& path,
                                               GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_front_panel_port_info();
    request->set_node_id(node_id);
    request->set_port_id(port_id);

    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_front_panel_port_info()) return false;
      resp = in.front_panel_port_info().vendor_name();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is
    // logged when it is created.
    tree->GetSwitchInterface()
        ->RetrieveValue(node_id, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    return SendResponse(GetResponse(path, resp), stream);
  };

  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/transceiver/state/vendor-part
void SetUpComponentsComponentTransceiverStateVendorPart(TreeNode* node,
                                                        YangParseTree* tree,
                                                        uint64 node_id,
                                                        uint32 port_id) {
  auto poll_functor = [tree, node_id, port_id](const GnmiEvent& event,
                                               const ::gnmi::Path& path,
                                               GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_front_panel_port_info();
    request->set_node_id(node_id);
    request->set_port_id(port_id);

    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_front_panel_port_info()) return false;
      resp = in.front_panel_port_info().part_number();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no
    // way to notify the controller that something went wrong.
    // The error is logged when it is created.
    tree->GetSwitchInterface()
        ->RetrieveValue(node_id, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    return SendResponse(GetResponse(path, resp), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/transceiver/state/form-factor
void SetUpComponentsComponentTransceiverStateFormFactor(TreeNode* node,
                                                        YangParseTree* tree,
                                                        uint64 node_id,
                                                        uint32 port_id) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::front_panel_port_info,
      &DataResponse::has_front_panel_port_info,
      &DataRequest::Request::mutable_front_panel_port_info,
      &FrontPanelPortInfo::media_type, ConvertMediaTypeToString);
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

}  // namespace

////////////////////////////////////////
//  AddSubtreeInterfaceFromSingleton  //
////////////////////////////////////////

void YangParseTreePaths::AddSubtreeInterfaceFromSingleton(
    const SingletonPort& singleton, const NodeConfigParams& node_config,
    YangParseTree* tree) {
  const std::string& name =
      singleton.name().empty()
          ? absl::StrFormat("%d/%d/%d", singleton.slot(), singleton.port(),
                            singleton.channel())
          : singleton.name();
  uint64 node_id = singleton.node();
  uint32 port_id = singleton.id();
  TreeNode* node =
      AddSubtreeInterface(name, node_id, port_id, node_config, tree);

  node = tree->AddNode(GetPath("lacp")("interfaces")(
      "interface", name)("state")("system-id-mac")());
  SetUpLacpInterfacesInterfaceStateSystemIdMac(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("state")("mac-address")());
  SetUpInterfacesInterfaceEthernetStateMacAddress(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("config")("port-speed")());
  SetUpInterfacesInterfaceEthernetConfigPortSpeed(
      node_id, port_id, singleton.speed_bps(), node, tree);
  bool port_auto_neg_enabled = false;
  bool port_enabled = false;
  bool loopback_enabled = false;
  uint64 mac_address = kDummyMacAddress;
  if (singleton.has_config_params()) {
    port_auto_neg_enabled =
        IsPortAutonegEnabled(singleton.config_params().autoneg());
    port_enabled = IsAdminStateEnabled(singleton.config_params().admin_state());
    if (singleton.config_params().has_mac_address()) {
      mac_address = singleton.config_params().mac_address().mac_address();
    }
    loopback_enabled =
        IsLoopbackStateEnabled(singleton.config_params().loopback_mode());
  }

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("config")("auto-negotiate")());
  SetUpInterfacesInterfaceEthernetConfigAutoNegotiate(
      node_id, port_id, port_auto_neg_enabled, node, tree);
  node = tree->AddNode(
      GetPath("interfaces")("interface", name)("config")("enabled")());
  SetUpInterfacesInterfaceConfigEnabled(port_enabled, node_id, port_id, node,
                                        tree);

  node = tree->AddNode(
      GetPath("interfaces")("interface", name)("config")("loopback-mode")());
  SetUpInterfacesInterfaceConfigLoopbackMode(loopback_enabled, node_id, port_id,
                                             node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("config")("mac-address")());
  SetUpInterfacesInterfaceEthernetConfigMacAddress(node_id, port_id,
                                                   mac_address, node, tree);

  // Paths for transceiver
  node = tree->AddNode(GetPath("components")(
      "component", name)("transceiver")("state")("present")());
  SetUpComponentsComponentTransceiverStatePresent(node, tree, node_id, port_id);
  node = tree->AddNode(GetPath("components")(
      "component", name)("transceiver")("state")("serial-no")());
  SetUpComponentsComponentTransceiverStateSerialNo(node, tree, node_id,
                                                   port_id);

  node = tree->AddNode(GetPath("components")(
      "component", name)("transceiver")("state")("vendor")());
  SetUpComponentsComponentTransceiverStateVendor(node, tree, node_id, port_id);

  node = tree->AddNode(GetPath("components")(
      "component", name)("transceiver")("state")("vendor-part")());
  SetUpComponentsComponentTransceiverStateVendorPart(node, tree, node_id,
                                                     port_id);

  node = tree->AddNode(GetPath("components")(
      "component", name)("transceiver")("state")("form-factor")());
  SetUpComponentsComponentTransceiverStateFormFactor(node, tree, node_id,
                                                     port_id);

  node = tree->AddNode(
      GetPath("components")("component", name)("state")("description")());
  SetUpComponentsComponentStateDescription(singleton.name(), node);
}

}  // namespace hal
}  // namespace stratum
