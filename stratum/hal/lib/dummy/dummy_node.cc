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

#include "stratum/hal/lib/dummy/dummy_node.h"

#include <memory>
#include <vector>

#include "stratum/public/lib/error.h"
#include "absl/memory/memory.h"
#include "stratum/glue/logging.h"

namespace stratum {
namespace hal {
namespace dummy {

::util::Status DummyNode::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&node_lock_);
  for (auto singleton_port : config.singleton_ports()) {
    uint64 port_id = singleton_port.id();
    uint64 node_id = singleton_port.node();
    if (node_id != id_) {
      continue;
    }
    SingletonPortStatus status;
    status.port_speed.set_speed_bps(singleton_port.speed_bps());
    ports_state_.emplace(port_id, status);
  }
  return ::util::OkStatus();
}

::util::Status DummyNode::VerifyChassisConfig(const ChassisConfig& config) {
  // TODO(Yi Tseng): Implement this method.
  absl::ReaderMutexLock l(&node_lock_);
  return ::util::OkStatus();
}

::util::Status DummyNode::PushForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config) {
  // TODO(Yi Tseng): Implement this method.
  absl::WriterMutexLock l(&node_lock_);
  return ::util::OkStatus();
}

::util::Status DummyNode::VerifyForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  // TODO(Yi Tseng): Implement this method.
  absl::ReaderMutexLock l(&node_lock_);
  return ::util::OkStatus();
}

::util::Status DummyNode::Shutdown() {
  // TODO(Yi Tseng): Implement this method.
  absl::WriterMutexLock l(&node_lock_);
  return ::util::OkStatus();
}

::util::Status DummyNode::Freeze() {
  // TODO(Yi Tseng): Implement this method.
  absl::WriterMutexLock l(&node_lock_);
  return ::util::OkStatus();
}
::util::Status DummyNode::Unfreeze() {
  // TODO(Yi Tseng): Implement this method.
  absl::WriterMutexLock l(&node_lock_);
  return ::util::OkStatus();
}

::util::Status DummyNode::WriteForwardingEntries(
      const ::p4::v1::WriteRequest& req,
      std::vector<::util::Status>* results) {
  // TODO(Yi Tseng): Implement this method.
  absl::WriterMutexLock l(&node_lock_);
  return ::util::OkStatus();
}

::util::Status DummyNode::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  // TODO(Yi Tseng): Implement this method.
  absl::ReaderMutexLock l(&node_lock_);
  return ::util::OkStatus();
}

::util::Status DummyNode::RegisterPacketReceiveWriter(
    std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> writer) {
  // TODO(Yi Tseng): Implement this method.
  absl::WriterMutexLock l(&node_lock_);
  return ::util::OkStatus();
}
::util::Status DummyNode::UnregisterPacketReceiveWriter() {
  // TODO(Yi Tseng): Implement this method.
  absl::WriterMutexLock l(&node_lock_);
  return ::util::OkStatus();
}

::util::Status DummyNode::TransmitPacket(const ::p4::v1::PacketOut& packet) {
  // TODO(Yi Tseng): Implement this method.
  absl::WriterMutexLock l(&node_lock_);
  return ::util::OkStatus();
}

bool DummyNode::DummyNodeEventWriter::Write(const DummyNodeEventPtr& msg) {
  uint64 node_id = msg->node_id;
  if (node_id != dummy_node_->Id()) {
    LOG(ERROR) << "Event for status update\n "
               << msg->state_update.DebugString()
               << "\nshould send to node " << node_id << " but it sent to "
               << "node " << dummy_node_->Id();
    return false;
  }
  auto port_id = msg->port_id;
  auto state_update = msg->state_update;

  auto& port_state = dummy_node_->ports_state_[port_id];
  GnmiEvent* event = nullptr;
  switch (state_update.response_case()) {
    case ::stratum::hal::DataResponse::kOperStatus:
      event =
      new PortOperStateChangedEvent(node_id,
                                    port_id,
                                    state_update.oper_status().state());
      port_state.oper_status = state_update.oper_status();
      break;
    case ::stratum::hal::DataResponse::kAdminStatus:
      event =
      new PortAdminStateChangedEvent(node_id,
                                     port_id,
                                     state_update.admin_status().state());
      port_state.admin_status = state_update.admin_status();
      break;
    case ::stratum::hal::DataResponse::kMacAddress:
      event =
      new PortMacAddressChangedEvent(node_id,
                                     port_id,
                                     state_update.mac_address().mac_address());
      port_state.mac_address = state_update.mac_address();
      break;
    case ::stratum::hal::DataResponse::kPortSpeed:
      event =
      new PortSpeedBpsChangedEvent(node_id,
                                   port_id,
                                   state_update.port_speed().speed_bps());
      port_state.port_speed = state_update.port_speed();
      break;
    case ::stratum::hal::DataResponse::kNegotiatedPortSpeed:
    event =
      new PortNegotiatedSpeedBpsChangedEvent(node_id,
                                             port_id,
                                            state_update.negotiated_port_speed()
                                                        .speed_bps());
      port_state.negotiated_port_speed = state_update.negotiated_port_speed();
      break;
    case ::stratum::hal::DataResponse::kLacpRouterMac:
      event =
      new PortLacpRouterMacChangedEvent(node_id,
                                          port_id,
                                          state_update.lacp_router_mac()
                                                      .mac_address());
      port_state.lacp_router_mac = state_update.lacp_router_mac();
      break;
    case ::stratum::hal::DataResponse::kLacpSystemPriority:
      event =
      new PortLacpSystemPriorityChangedEvent(node_id,
                                             port_id,
                                           state_update.lacp_system_priority()
                                                       .priority());
      port_state.lacp_system_priority = state_update.lacp_system_priority();
      break;
    case ::stratum::hal::DataResponse::kPortCounters:
      event =
      new PortCountersChangedEvent(node_id,
                                   port_id,
                                   state_update.port_counters());
      port_state.port_counters = state_update.port_counters();
      break;
    case ::stratum::hal::DataResponse::kForwardingViability :
      event =
      new PortForwardingViabilityChangedEvent(node_id,
                                              port_id,
                                             state_update.forwarding_viability()
                                                       .state());
      port_state.forwarding_viability = state_update.forwarding_viability();
      break;
    case ::stratum::hal::DataResponse::kHealthIndicator :
      event =
      new PortHealthIndicatorChangedEvent(node_id,
                                          port_id,
                                          state_update.health_indicator()
                                                      .state());
      port_state.health_indicator = state_update.health_indicator();
      break;
    case ::stratum::hal::DataResponse::RESPONSE_NOT_SET:
      LOG(ERROR) << "State update type does not set.";
      return false;
    default:
      // TODO(Yi Tseng): Support node event and port queue event
      LOG(ERROR) << "State update\n" << state_update.DebugString()
                 << " does not supported.";
      return false;
  }
  bool writer_result = writer_->Write(GnmiEventPtr(event));
  if (!writer_result) {
    // Remove WriterInterface if it is no longer operational.
    writer_.reset();
  }
  return writer_result;
}

::util::Status DummyNode::RegisterEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  absl::ReaderMutexLock l(&node_lock_);
  auto writer_wrapper =
    std::make_shared<DummyNodeEventWriter>(DummyNodeEventWriter(this, writer));

  RETURN_IF_ERROR(
      dummy_box_->RegisterNodeEventNotifyWriter(id_, writer_wrapper));
  return ::util::OkStatus();
}

::util::Status DummyNode::UnregisterEventNotifyWriter() {
  absl::ReaderMutexLock l(&node_lock_);
  RETURN_IF_ERROR(dummy_box_->UnregisterNodeEventNotifyWriter(id_));
  return ::util::OkStatus();
}

// Retrieve port data from this node
::util::StatusOr<DataResponse>
DummyNode::RetrievePortData(const Request& request) {
  uint64 port_id;
  DataResponse resp;
  SingletonPortStatus port_status;

  switch (request.request_case()) {
      case Request::kOperStatus:
        port_id = request.oper_status().port_id();
        port_status = ports_state_[port_id];
        resp.mutable_oper_status()->set_state(port_status.oper_status.state());
        break;
      case Request::kAdminStatus:
        port_id = request.admin_status().port_id();
        port_status = ports_state_[port_id];
        resp.mutable_admin_status()->
          set_state(port_status.admin_status.state());
        break;
      case Request::kMacAddress:
        port_id = request.mac_address().port_id();
        port_status = ports_state_[port_id];
        resp.mutable_mac_address()->
          set_mac_address(port_status.mac_address.mac_address());
        break;
      case Request::kPortSpeed:
        port_id = request.port_speed().port_id();
        port_status = ports_state_[port_id];
        resp.mutable_port_speed()->
          set_speed_bps(port_status.port_speed.speed_bps());
        break;
      case Request::kNegotiatedPortSpeed:
        port_id = request.negotiated_port_speed().port_id();
        port_status = ports_state_[port_id];
        resp.mutable_negotiated_port_speed()->
          set_speed_bps(port_status.negotiated_port_speed.speed_bps());
        break;
      case Request::kLacpRouterMac:
        port_id = request.lacp_router_mac().port_id();
        port_status = ports_state_[port_id];
        resp.mutable_lacp_router_mac()->
          set_mac_address(port_status.lacp_router_mac.mac_address());
        break;
      case Request::kLacpSystemPriority:
        port_id = request.lacp_system_priority().port_id();
        port_status = ports_state_[port_id];
        resp.mutable_lacp_system_priority()->
          set_priority(port_status.lacp_system_priority.priority());
        break;
      case Request::kPortCounters:
        port_id = request.port_counters().port_id();
        port_status = ports_state_[port_id];
        resp.mutable_port_counters()->
          set_in_octets(port_status.port_counters.in_octets());
        resp.mutable_port_counters()->
          set_in_unicast_pkts(port_status.port_counters.in_unicast_pkts());
        resp.mutable_port_counters()->
          set_in_broadcast_pkts(port_status.port_counters.in_broadcast_pkts());
        resp.mutable_port_counters()->
          set_in_multicast_pkts(port_status.port_counters.in_multicast_pkts());
        resp.mutable_port_counters()->
          set_in_discards(port_status.port_counters.in_discards());
        resp.mutable_port_counters()->
          set_in_errors(port_status.port_counters.in_errors());
        resp.mutable_port_counters()->
          set_in_unknown_protos(port_status.port_counters.in_unknown_protos());
        resp.mutable_port_counters()->
          set_out_octets(port_status.port_counters.out_octets());
        resp.mutable_port_counters()->
          set_out_unicast_pkts(port_status.port_counters.out_unicast_pkts());
        resp.mutable_port_counters()->
         set_out_broadcast_pkts(port_status.port_counters.out_broadcast_pkts());
        resp.mutable_port_counters()->
         set_out_multicast_pkts(port_status.port_counters.out_multicast_pkts());
        resp.mutable_port_counters()->
          set_out_discards(port_status.port_counters.out_discards());
        resp.mutable_port_counters()->
          set_out_errors(port_status.port_counters.out_errors());
        resp.mutable_port_counters()->
          set_in_fcs_errors(port_status.port_counters.in_fcs_errors());
        break;
      case Request::kForwardingViability:
        port_id = request.forwarding_viability().port_id();
        port_status = ports_state_[port_id];
        resp.mutable_forwarding_viability()->
          set_state(port_status.forwarding_viability.state());
        break;
      case Request::kHealthIndicator:
        port_id = request.health_indicator().port_id();
        port_status = ports_state_[port_id];
        resp.mutable_health_indicator()->
          set_state(port_status.health_indicator.state());
        break;
    case Request::kHardwarePort:
        // FIXME(Yi Tseng): Sets hardware port name
        resp.mutable_hardware_port()->set_name("");
        break;
      default:
        return MAKE_ERROR(ERR_INTERNAL) << "Not supported yet!";
  }
  return ::util::StatusOr<DataResponse>(resp);
}

::util::StatusOr<DataResponse>
DummyNode::RetrievePortQosData(const Request& request) {
  return MAKE_ERROR(ERR_INTERNAL) << "Not supported yet!";
}

uint64 DummyNode::Id() const {
  return id_;
}

std::string DummyNode::Name() const {
  return name_;
}

int32 DummyNode::Slot() const {
  return slot_;
}

int32 DummyNode::Index() const {
  return index_;
}

DummyNode*
DummyNode::CreateInstance(const uint64 id, const std::string& name,
                          const int32 slot, const int32 index) {
  return new DummyNode(id, name, slot, index);
}

DummyNode::DummyNode(const uint64 id, const std::string& name,
                     const int32 slot, const int32 index)
  : id_(id),
    name_(name),
    slot_(slot),
    index_(index),
    dummy_box_(DummyBox::GetSingleton()) {}

}  // namespace dummy
}  // namespace hal
}  // namespace stratum
