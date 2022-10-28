// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Interface setup functions for YangParseTreePaths. Used by the
// AddSubtreeInterface() and AddSubtreeInterfaceFromTrunk() methods.

#include "stratum/hal/lib/yang/yang_parse_tree_interface.h"

#include "gnmi/gnmi.pb.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/yang/yang_parse_tree_helpers.h"
#include "stratum/hal/lib/yang/yang_parse_tree.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/public/proto/error.pb.h"

namespace stratum {
namespace hal {
namespace yang {
namespace interface {

using namespace stratum::hal::yang::helpers;

namespace {

// A helper function that creates a functor that reads a counter from
// DataResponse::Counters proto buffer.
// 'func_ptr' points to protobuf field accessor method that reads the counter
// data from the DataResponse proto received from SwitchInterface, i.e.,
// "&DataResponse::PortCounters::message", where message field in
// DataResponse::Counters.
TreeNodeEventHandler GetPollCounterFunctor(
    uint64 node_id, uint32 port_id,
    uint64 (PortCounters::*func_ptr)() const,
    YangParseTree* tree) {
  return [tree, node_id, port_id, func_ptr](const GnmiEvent& event,
                                            const ::gnmi::Path& path,
                                            GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_port_counters();
    request->set_node_id(node_id);
    request->set_port_id(port_id);

    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    uint64 resp = 0;
    DataResponseWriter writer(
        [&resp, func_ptr](const DataResponse& in) -> bool {
          if (!in.has_port_counters()) return false;
          resp = (in.port_counters().*func_ptr)();
          return true;
        });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is logged when
    // it is created.
    auto status = tree->GetSwitchInterface()->RetrieveValue(
        node_id, req, &writer, /* details= */ nullptr);
    return SendResponse(GetResponse(path, resp), stream);
  };
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/last-change
void SetUpInterfacesInterfaceStateLastChange(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::oper_status,
                       &DataResponse::has_oper_status,
                       &DataRequest::Request::mutable_oper_status,
                       &OperStatus::time_last_changed);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortOperStateChangedEvent::GetTimeLastChanged);
  auto register_functor = RegisterFunc<PortOperStateChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/ifindex
void SetUpInterfacesInterfaceStateIfindex(
    uint32 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  // Returns the port ID for the interface to be used by P4Runtime.
  auto on_poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::sdn_port_id,
      &DataResponse::has_sdn_port_id,
      &DataRequest::Request::mutable_sdn_port_id, &SdnPortId::port_id);
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(on_poll_functor)
      ->SetOnPollHandler(on_poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/name
void SetUpInterfacesInterfaceStateName(
    const std::string& name, TreeNode* node) {
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler([name](const GnmiEvent& event,
                                 const ::gnmi::Path& path,
                                 GnmiSubscribeStream* stream) {
        return SendResponse(GetResponse(path, name), stream);
      })
      ->SetOnPollHandler([name](const GnmiEvent& event,
                                const ::gnmi::Path& path,
                                GnmiSubscribeStream* stream) {
        return SendResponse(GetResponse(path, name), stream);
      })
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/oper-status
void SetUpInterfacesInterfaceStateOperStatus(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::oper_status,
                       &DataResponse::has_oper_status,
                       &DataRequest::Request::mutable_oper_status,
                       &OperStatus::state, ConvertPortStateToString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortOperStateChangedEvent::GetNewState,
      ConvertPortStateToString);
  auto register_functor = RegisterFunc<PortOperStateChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/admin-status
void SetUpInterfacesInterfaceStateAdminStatus(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::admin_status,
                       &DataResponse::has_admin_status,
                       &DataRequest::Request::mutable_admin_status,
                       &AdminStatus::state, ConvertAdminStateToString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortAdminStateChangedEvent::GetNewState,
      ConvertAdminStateToString);
  auto register_functor = RegisterFunc<PortAdminStateChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/loopback-mode
//
void SetUpInterfacesInterfaceStateLoopbackMode(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::loopback_status,
                       &DataResponse::has_loopback_status,
                       &DataRequest::Request::mutable_loopback_status,
                       &LoopbackStatus::state, IsLoopbackStateEnabled);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortLoopbackStateChangedEvent::GetNewState,
      IsLoopbackStateEnabled);
  auto register_functor = RegisterFunc<PortLoopbackStateChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/hardware-port
void SetUpInterfacesInterfaceStateHardwarePort(
    const std::string& name, TreeNode* node, YangParseTree* tree) {
  // This leaf is a reference to the /components/component[name=<name>]/name
  // leaf. We return the name directly here, as it is the same.
  auto poll_functor = [name](const GnmiEvent& event, const ::gnmi::Path& path,
                             GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, name), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/state/port-speed
void SetUpInterfacesInterfaceEthernetStatePortSpeed(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::port_speed,
      &DataResponse::has_port_speed, &DataRequest::Request::mutable_port_speed,
      &PortSpeed::speed_bps, ConvertSpeedBpsToString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortSpeedBpsChangedEvent::GetSpeedBps,
      ConvertSpeedBpsToString);
  auto register_functor = RegisterFunc<PortSpeedBpsChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/state/negotiated-port-speed
void SetUpInterfacesInterfaceEthernetStateNegotiatedPortSpeed(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::negotiated_port_speed,
      &DataResponse::has_negotiated_port_speed,
      &DataRequest::Request::mutable_negotiated_port_speed,
      &PortSpeed::speed_bps, ConvertSpeedBpsToString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id,
      &PortNegotiatedSpeedBpsChangedEvent::GetNegotiatedSpeedBps,
      ConvertSpeedBpsToString);
  auto register_functor = RegisterFunc<PortNegotiatedSpeedBpsChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-octets
void SetUpInterfacesInterfaceStateCountersInOctets(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetPollCounterFunctor(node_id, port_id, &PortCounters::in_octets, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInOctets);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/out-octets
void SetUpInterfacesInterfaceStateCountersOutOctets(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetPollCounterFunctor(node_id, port_id, &PortCounters::out_octets, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetOutOctets);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-unicast-pkts
void SetUpInterfacesInterfaceStateCountersInUnicastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::in_unicast_pkts, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInUnicastPkts);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/out-unicast-pkts
void SetUpInterfacesInterfaceStateCountersOutUnicastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::out_unicast_pkts, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetOutUnicastPkts);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-broadcast-pkts
void SetUpInterfacesInterfaceStateCountersInBroadcastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::in_broadcast_pkts, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInBroadcastPkts);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/out-broadcast-pkts
void SetUpInterfacesInterfaceStateCountersOutBroadcastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::out_broadcast_pkts, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetOutBroadcastPkts);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-multicast-pkts
void SetUpInterfacesInterfaceStateCountersInMulticastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::in_multicast_pkts, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInMulticastPkts);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/out-multicast-pkts
void SetUpInterfacesInterfaceStateCountersOutMulticastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::out_multicast_pkts, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetOutMulticastPkts);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-discards
void SetUpInterfacesInterfaceStateCountersInDiscards(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetPollCounterFunctor(node_id, port_id, &PortCounters::in_discards, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInDiscards);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/out-discards
void SetUpInterfacesInterfaceStateCountersOutDiscards(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(node_id, port_id,
                                            &PortCounters::out_discards, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetOutDiscards);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-unknown-protos
void SetUpInterfacesInterfaceStateCountersInUnknownProtos(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::in_unknown_protos, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInUnknownProtos);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-errors
void SetUpInterfacesInterfaceStateCountersInErrors(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetPollCounterFunctor(node_id, port_id, &PortCounters::in_errors, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInErrors);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/out-errors
void SetUpInterfacesInterfaceStateCountersOutErrors(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetPollCounterFunctor(node_id, port_id, &PortCounters::out_errors, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetOutErrors);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-fcs-errors
void SetUpInterfacesInterfaceStateCountersInFcsErrors(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(node_id, port_id,
                                            &PortCounters::in_fcs_errors, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInFcsErrors);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /lacp/interfaces/interface[name=<name>]/state/system-priority
void SetUpLacpInterfacesInterfaceStateSystemPriority(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::lacp_system_priority,
      &DataResponse::has_lacp_system_priority,
      &DataRequest::Request::mutable_lacp_system_priority,
      &SystemPriority::priority);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortLacpSystemPriorityChangedEvent::GetSystemPriority);
  auto register_functor = RegisterFunc<PortLacpSystemPriorityChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/config/health-indicator
//
void SetUpInterfacesInterfaceConfigHealthIndicator(
    const std::string& state, uint64 node_id, uint64 port_id,
    TreeNode* node, YangParseTree* tree) {
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
    std::string state_string = typed_val->string_val();
    HealthState typed_state;
    if (state_string == "BAD") {
      typed_state = HealthState::HEALTH_STATE_BAD;
    } else if (state_string == "GOOD") {
      typed_state = HealthState::HEALTH_STATE_GOOD;
    } else if (state_string == "UNKNOWN") {
      typed_state = HealthState::HEALTH_STATE_UNKNOWN;
    } else {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "wrong value!";
    }

    // Set the value.
    auto status = SetValue(node_id, port_id, tree,
                           &SetRequest::Request::Port::mutable_health_indicator,
                           &HealthIndicator::set_state, typed_state);
    if (status != ::util::OkStatus()) {
      return status;
    }

    // Update the YANG parse tree.
    auto poll_functor = [state_string](const GnmiEvent& event,
                                       const ::gnmi::Path& path,
                                       GnmiSubscribeStream* stream) {
      // This leaf represents configuration data. Return what was known when
      // it was configured!
      return SendResponse(GetResponse(path, state_string), stream);
    };
    node->SetOnTimerHandler(poll_functor)->SetOnPollHandler(poll_functor);

    // Trigger change notification.
    tree->SendNotification(GnmiEventPtr(
        new PortHealthIndicatorChangedEvent(node_id, port_id, typed_state)));

    return ::util::OkStatus();
  };
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortHealthIndicatorChangedEvent::GetState,
      ConvertHealthStateToString);
  auto register_functor = RegisterFunc<PortHealthIndicatorChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/health-indicator
//
void SetUpInterfacesInterfaceStateHealthIndicator(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::health_indicator,
                       &DataResponse::has_health_indicator,
                       &DataRequest::Request::mutable_health_indicator,
                       &HealthIndicator::state, ConvertHealthStateToString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortHealthIndicatorChangedEvent::GetState,
      ConvertHealthStateToString);
  auto register_functor = RegisterFunc<PortHealthIndicatorChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/config/forwarding-viable
void SetUpInterfacesInterfaceEthernetConfigForwardingViability(
    uint64 node_id, uint32 port_id, bool forwarding_viability, TreeNode* node,
    YangParseTree* tree) {
  auto poll_functor = [forwarding_viability](const GnmiEvent& event,
                                             const ::gnmi::Path& path,
                                             GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when it
    // was configured!
    return SendResponse(GetResponse(path, forwarding_viability), stream);
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
    TrunkMemberBlockState new_forwarding_viability =
        typed_val->bool_val() ? TRUNK_MEMBER_BLOCK_STATE_FORWARDING
                              : TRUNK_MEMBER_BLOCK_STATE_BLOCKED;
    auto status =
        SetValue(node_id, port_id, tree,
                 &SetRequest::Request::Port::mutable_forwarding_viability,
                 &ForwardingViability::set_state, new_forwarding_viability);

    if (status != ::util::OkStatus()) {
      return status;
    }

    // Update the YANG parse tree.
    auto poll_functor = [new_forwarding_viability](
                            const GnmiEvent& event, const ::gnmi::Path& path,
                            GnmiSubscribeStream* stream) {
      return SendResponse(
          GetResponse(
              path,
              ConvertTrunkMemberBlockStateToBool(new_forwarding_viability)),
          stream);
    };
    node->SetOnTimerHandler(poll_functor)->SetOnPollHandler(poll_functor);

    return ::util::OkStatus();
  };

  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/state/forwarding-viable
void SetUpInterfacesInterfaceEthernetStateForwardingViability(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::forwarding_viability,
      &DataResponse::has_forwarding_viability,
      &DataRequest::Request::mutable_forwarding_viability,
      &ForwardingViability::state, ConvertTrunkMemberBlockStateToBool);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortForwardingViabilityChangedEvent::GetState,
      ConvertTrunkMemberBlockStateToBool);
  auto register_functor = RegisterFunc<PortForwardingViabilityChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/state/auto-negotiate
void SetUpInterfacesInterfaceEthernetStateAutoNegotiate(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::autoneg_status,
                       &DataResponse::has_autoneg_status,
                       &DataRequest::Request::mutable_autoneg_status,
                       &AutonegotiationStatus::state, IsPortAutonegEnabled);
  auto on_change_functor =
      GetOnChangeFunctor(node_id, port_id, &PortAutonegChangedEvent::GetState,
                         IsPortAutonegEnabled);
  auto register_functor = RegisterFunc<PortAutonegChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/interfaces/interface[name=<name>]
//                    /output/queues/queue[name=<name>]/state/name
void SetUpQosInterfacesInterfaceOutputQueuesQueueStateName(
    const std::string& name, TreeNode* node) {
  auto poll_functor = [name](const GnmiEvent& event, const ::gnmi::Path& path,
                             GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, name), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/interfaces/interface[name=<name>]
//                    /output/queues/queue[name=<name>]/state/id
void SetUpQosInterfacesInterfaceOutputQueuesQueueStateId(
    uint64 node_id, uint32 port_id, uint32 queue_id,
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, queue_id, tree, &DataResponse::port_qos_counters,
      &DataResponse::has_port_qos_counters,
      &DataRequest::Request::mutable_port_qos_counters,
      &PortQosCounters::queue_id);
  auto register_functor = RegisterFunc<PortQosCountersChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, queue_id, &PortQosCountersChangedEvent::GetQueueId);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/interfaces/interface[name=<name>]
//                    /output/queues/queue[name=<name>]/state/transmit-pkts
void SetUpQosInterfacesInterfaceOutputQueuesQueueStateTransmitPkts(
    uint64 node_id, uint32 port_id, uint32 queue_id, TreeNode* node,
    YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, queue_id, tree, &DataResponse::port_qos_counters,
      &DataResponse::has_port_qos_counters,
      &DataRequest::Request::mutable_port_qos_counters,
      &PortQosCounters::out_pkts);
  auto register_functor = RegisterFunc<PortQosCountersChangedEvent>();
  auto on_change_functor =
      GetOnChangeFunctor(node_id, port_id, queue_id,
                         &PortQosCountersChangedEvent::GetTransmitPkts);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/interfaces/interface[name=<name>]
//                    /output/queues/queue[name=<name>]/state/transmit-octets
void SetUpQosInterfacesInterfaceOutputQueuesQueueStateTransmitOctets(
    uint64 node_id, uint32 port_id, uint32 queue_id, TreeNode* node,
    YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, queue_id, tree, &DataResponse::port_qos_counters,
      &DataResponse::has_port_qos_counters,
      &DataRequest::Request::mutable_port_qos_counters,
      &PortQosCounters::out_octets);
  auto register_functor = RegisterFunc<PortQosCountersChangedEvent>();
  auto on_change_functor =
      GetOnChangeFunctor(node_id, port_id, queue_id,
                         &PortQosCountersChangedEvent::GetTransmitOctets);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/interfaces/interface[name=<name>]
//                    /output/queues/queue[name=<name>]/state/dropped-pkts
void SetUpQosInterfacesInterfaceOutputQueuesQueueStateDroppedPkts(
    uint64 node_id, uint32 port_id, uint32 queue_id, TreeNode* node,
    YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, queue_id, tree, &DataResponse::port_qos_counters,
      &DataResponse::has_port_qos_counters,
      &DataRequest::Request::mutable_port_qos_counters,
      &PortQosCounters::out_dropped_pkts);
  auto register_functor = RegisterFunc<PortQosCountersChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, queue_id, &PortQosCountersChangedEvent::GetDroppedPkts);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/queues/queue[name=<name>]/config/id
void SetUpQosQueuesQueueConfigId(
    uint32 queue_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = [queue_id](const GnmiEvent& event,
                                 const ::gnmi::Path& path,
                                 GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when
    // it was configured!
    return SendResponse(GetResponse(path, queue_id), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/queues/queue[name=<name>]/state/id
void SetUpQosQueuesQueueStateId(
    uint32 queue_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = [queue_id](const GnmiEvent& event,
                                 const ::gnmi::Path& path,
                                 GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when
    // it was configured!
    return SendResponse(GetResponse(path, queue_id), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

}  // namespace interface
}  // namespace yang
}  // namespace hal
}  // namespace stratum
