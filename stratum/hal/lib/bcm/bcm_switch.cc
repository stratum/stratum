// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bcm/bcm_switch.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace bcm {

BcmSwitch::BcmSwitch(PhalInterface* phal_interface,
                     BcmChassisManager* bcm_chassis_manager,
                     const std::map<int, BcmNode*>& unit_to_bcm_node)
    : phal_interface_(ABSL_DIE_IF_NULL(phal_interface)),
      bcm_chassis_manager_(ABSL_DIE_IF_NULL(bcm_chassis_manager)),
      unit_to_bcm_node_(unit_to_bcm_node),
      node_id_to_bcm_node_() {
  for (auto entry : unit_to_bcm_node_) {
    CHECK_GE(entry.first, 0) << "Invalid unit number " << entry.first << ".";
    CHECK_NE(entry.second, nullptr)
        << "Detected null BcmNode for unit " << entry.first << ".";
  }
}

BcmSwitch::~BcmSwitch() {}

::util::Status BcmSwitch::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  // Verify the config first. No need to continue if verification is not OK.
  // Push config to PHAL first and then the rest of the managers.
  RETURN_IF_ERROR(DoVerifyChassisConfig(config));
  RETURN_IF_ERROR(phal_interface_->PushChassisConfig(config));
  RETURN_IF_ERROR(bcm_chassis_manager_->PushChassisConfig(config));
  ASSIGN_OR_RETURN(const auto& node_id_to_unit,
                   bcm_chassis_manager_->GetNodeIdToUnitMap());
  node_id_to_bcm_node_.clear();
  for (const auto& entry : node_id_to_unit) {
    uint64 node_id = entry.first;
    int unit = entry.second;
    ASSIGN_OR_RETURN(auto* bcm_node, GetBcmNodeFromUnit(unit));
    RETURN_IF_ERROR(bcm_node->PushChassisConfig(config, node_id));
    node_id_to_bcm_node_[node_id] = bcm_node;
  }

  LOG(INFO) << "Chassis config pushed successfully.";

  return ::util::OkStatus();
}

::util::Status BcmSwitch::VerifyChassisConfig(const ChassisConfig& config) {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  return DoVerifyChassisConfig(config);
}

::util::Status BcmSwitch::PushForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  // Verify the config first. Continue if verification is OK.
  RETURN_IF_ERROR(DoVerifyForwardingPipelineConfig(node_id, config));
  ASSIGN_OR_RETURN(auto* bcm_node, GetBcmNodeFromNodeId(node_id));
  RETURN_IF_ERROR(bcm_node->PushForwardingPipelineConfig(config));

  LOG(INFO) << "P4-based forwarding pipeline config pushed successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status BcmSwitch::SaveForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  return MAKE_ERROR(ERR_UNIMPLEMENTED)
         << "SaveForwardingPipelineConfig not implemented for this target";
}

::util::Status BcmSwitch::CommitForwardingPipelineConfig(uint64 node_id) {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  return MAKE_ERROR(ERR_UNIMPLEMENTED)
         << "CommitForwardingPipelineConfig not implemented for this target";
}

::util::Status BcmSwitch::VerifyForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  return DoVerifyForwardingPipelineConfig(node_id, config);
}

::util::Status BcmSwitch::Shutdown() {
  // The shutdown flag must be checked on all read or write accesses to
  // state protected by chassis_lock, whether within RPC executions or
  // event handler threas.
  {
    absl::WriterMutexLock l(&chassis_lock);
    shutdown = true;
  }

  // Shutdown all the managers and then PHAL at the end.
  ::util::Status status = ::util::OkStatus();
  for (const auto& entry : unit_to_bcm_node_) {
    BcmNode* bcm_node = entry.second;
    APPEND_STATUS_IF_ERROR(status, bcm_node->Shutdown());
  }
  APPEND_STATUS_IF_ERROR(status, bcm_chassis_manager_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, phal_interface_->Shutdown());
  node_id_to_bcm_node_.clear();

  if (status.ok()) {
    LOG(INFO) << "Switch shutdown completed successfully.";
  }

  return status;
}

::util::Status BcmSwitch::Freeze() {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

::util::Status BcmSwitch::Unfreeze() {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

::util::Status BcmSwitch::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  if (!req.updates_size()) return ::util::OkStatus();  // nothing to do.
  CHECK_RETURN_IF_FALSE(req.device_id()) << "No device_id in WriteRequest.";
  CHECK_RETURN_IF_FALSE(results != nullptr)
      << "Need to provide non-null results pointer for non-empty updates.";

  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  // Get BcmNode which the device_id is associated with.
  ASSIGN_OR_RETURN(auto* bcm_node, GetBcmNodeFromNodeId(req.device_id()));
  return bcm_node->WriteForwardingEntries(req, results);
}

::util::Status BcmSwitch::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  CHECK_RETURN_IF_FALSE(req.device_id()) << "No device_id in ReadRequest.";
  CHECK_RETURN_IF_FALSE(writer) << "Channel writer must be non-null.";
  CHECK_RETURN_IF_FALSE(details) << "Details pointer must be non-null.";

  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  // Get BcmNode which the device_id is associated with.
  ASSIGN_OR_RETURN(auto* bcm_node, GetBcmNodeFromNodeId(req.device_id()));
  return bcm_node->ReadForwardingEntries(req, writer, details);
}

::util::Status BcmSwitch::RegisterPacketReceiveWriter(
    uint64 node_id,
    std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> writer) {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  // Get BcmNode which the node_id is associated with.
  ASSIGN_OR_RETURN(auto* bcm_node, GetBcmNodeFromNodeId(node_id));
  return bcm_node->RegisterPacketReceiveWriter(writer);
}

::util::Status BcmSwitch::UnregisterPacketReceiveWriter(uint64 node_id) {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  // Get BcmNode which the node_id is associated with.
  ASSIGN_OR_RETURN(auto* bcm_node, GetBcmNodeFromNodeId(node_id));
  return bcm_node->UnregisterPacketReceiveWriter();
}

::util::Status BcmSwitch::TransmitPacket(uint64 node_id,
                                         const ::p4::v1::PacketOut& packet) {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  // Get BcmNode which the node_id is associated with.
  ASSIGN_OR_RETURN(auto* bcm_node, GetBcmNodeFromNodeId(node_id));
  return bcm_node->TransmitPacket(packet);
}

::util::Status BcmSwitch::RegisterEventNotifyWriter(
    std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  RETURN_IF_ERROR(
      bcm_chassis_manager_->RegisterEventNotifyWriter(std::move(writer)));
  return ::util::OkStatus();
}

::util::Status BcmSwitch::UnregisterEventNotifyWriter() {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  RETURN_IF_ERROR(bcm_chassis_manager_->UnregisterEventNotifyWriter());
  return ::util::OkStatus();
}

::util::Status BcmSwitch::RetrieveValue(uint64 /*node_id*/,
                                        const DataRequest& request,
                                        WriterInterface<DataResponse>* writer,
                                        std::vector<::util::Status>* details) {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  // TODO(b/69920763): Implement this. The code below is just a placeholder.
  for (const auto& req : request.requests()) {
    DataResponse resp;
    ::util::Status status = ::util::OkStatus();
    switch (req.request_case()) {
      // Get singleton port operational state.
      case DataRequest::Request::kOperStatus: {
        auto port_state = bcm_chassis_manager_->GetPortState(
            req.oper_status().node_id(), req.oper_status().port_id());
        if (!port_state.ok()) {
          status.Update(port_state.status());
        } else {
          resp.mutable_oper_status()->set_state(port_state.ValueOrDie());
        }
        break;
      }
      // Get singleton port admin state.
      case DataRequest::Request::kAdminStatus: {
        auto admin_state = bcm_chassis_manager_->GetPortAdminState(
            req.admin_status().node_id(), req.admin_status().port_id());
        if (!admin_state.ok()) {
          status.Update(admin_state.status());
        } else {
          resp.mutable_admin_status()->set_state(admin_state.ValueOrDie());
        }
        break;
      }
      // Get singleton port loopback state.
      case DataRequest::Request::kLoopbackStatus: {
        auto loopback_state = bcm_chassis_manager_->GetPortLoopbackState(
            req.loopback_status().node_id(), req.loopback_status().port_id());
        if (!loopback_state.ok()) {
          status.Update(loopback_state.status());
        } else {
          resp.mutable_loopback_status()->set_state(
              loopback_state.ValueOrDie());
        }
        break;
      }
      // Get configured singleton port speed in bits per second.
      case DataRequest::Request::kPortSpeed: {
        auto bcm_port = bcm_chassis_manager_->GetBcmPort(
            req.port_speed().node_id(), req.port_speed().port_id());
        if (!bcm_port.ok()) {
          status.Update(bcm_port.status());
        } else {
          resp.mutable_port_speed()->set_speed_bps(
              bcm_port.ValueOrDie().speed_bps());
        }
        break;
      }
      case DataRequest::Request::kLacpRouterMac:
        // Find LACP System ID MAC address of port located at:
        // - node_id: req.lacp_router_mac().node_id()
        // - port_id: req.lacp_router_mac().port_id()
        // and then write it into the response.
        resp.mutable_lacp_router_mac()->set_mac_address(0x112233445566ull);
        break;
      case DataRequest::Request::kLacpSystemPriority:
        // Find LACP System priority of port located at:
        // - node_id: req.lacp_system_priority().node_id()
        // - port_id: req.lacp_system_priority().port_id()
        // and then write it into the response.
        resp.mutable_lacp_system_priority()->set_priority(1000);
        break;
      case DataRequest::Request::kNegotiatedPortSpeed:
        // Find negotiated speed in bits per second of port located at:
        // - node_id: req.negotiated_port_speed().node_id()
        // - port_id: req.negotiated_port_speed().port_id()
        // and then write it into the response.
        resp.mutable_negotiated_port_speed()->set_speed_bps(kFortyGigBps);
        break;
      case DataRequest::Request::kMacAddress:
        // TODO(unknown) Find out why the controller needs it.
        // Find MAC address of port located at:
        // - node_id: req.mac_address().node_id()
        // - port_id: req.mac_address().port_id()
        // and then write it into the response.
        resp.mutable_mac_address()->set_mac_address(0x112233445566ull);
        break;
      case DataRequest::Request::kPortCounters: {
        // Find current port counters for port located at:
        // - node_id: req.port_counters().node_id()
        // - port_id: req.port_counters().port_id()
        // and then write it into the response.
        status.Update(bcm_chassis_manager_->GetPortCounters(
            req.port_counters().node_id(), req.port_counters().port_id(),
            resp.mutable_port_counters()));
        break;
      }
      case DataRequest::Request::kHealthIndicator:
        // Find current port health indicator (LED) for port located at:
        // - node_id: req.health_indicator().node_id()
        // - port_id: req.health_indicator().port_id()
        // and then write it into the response.
        resp.mutable_health_indicator()->set_state(HEALTH_STATE_GOOD);
        break;
      case DataRequest::Request::kForwardingViability:
        // Find current port forwarding viable state for port located at:
        // - node_id: req.forwarding_viable().node_id()
        // - port_id: req.forwarding_viable().port_id()
        // and then write it into the response.
        resp.mutable_forwarding_viability()->set_state(
            TRUNK_MEMBER_BLOCK_STATE_FORWARDING);
        break;
      case DataRequest::Request::kMemoryErrorAlarm: {
        // Find current state of memory-error alarm
        // and then write it into the response.
        auto* alarm = resp.mutable_memory_error_alarm();
        uint64 now = absl::GetCurrentTimeNanos();
        alarm->set_status(true);
        alarm->set_time_created(now);
        alarm->set_severity(Alarm::CRITICAL);
        alarm->set_description("memory-error alarm");
        break;
      }
      case DataRequest::Request::kFlowProgrammingExceptionAlarm: {
        // Find current state of flow-programing-exception alarm
        // and then write it into the response.
        auto* alarm = resp.mutable_flow_programming_exception_alarm();
        uint64 now = absl::GetCurrentTimeNanos();
        alarm->set_status(true);
        alarm->set_time_created(now);
        alarm->set_severity(Alarm::CRITICAL);
        alarm->set_description("flow-programming-exception alarm");
        break;
      }
      case DataRequest::Request::kPortQosCounters: {
        // Find current counters for port's qos queue located at:
        // - node_id: req.port_qos_counters().node_id()
        // - port_id: req.port_qos_counters().port_id()
        // - queue_id: req.port_qos_counters().queue_id()
        // and then write it into the response.
        auto* counters = resp.mutable_port_qos_counters();
        // To simulate the counters being incremented the current time expressed
        // in nanoseconds since Jan 1st, 1970 is used.
        // TODO(unknown) Remove this hack once the real counters are
        // available.
        uint64 now = absl::GetCurrentTimeNanos();
        counters->set_out_octets(now);
        counters->set_out_pkts(now);
        counters->set_out_dropped_pkts(now);
        counters->set_queue_id(req.port_qos_counters().queue_id());
        break;
      }
      case DataRequest::Request::kNodePacketioDebugInfo:
        // Find current debug info for node located at:
        // - node_id: req.node_packet_io_debug_info().node_id()
        // and then write it into the response.
        resp.mutable_node_packetio_debug_info()->set_debug_string(
            "A (sample) node debug string.");
        break;
      case DataRequest::Request::kNodeInfo: {
        auto unit =
            bcm_chassis_manager_->GetUnitFromNodeId(req.node_info().node_id());
        if (!unit.ok()) {
          status.Update(unit.status());
        } else {
          auto bcm_chip = bcm_chassis_manager_->GetBcmChip(unit.ValueOrDie());
          if (!bcm_chip.ok()) {
            status.Update(bcm_chip.status());
          } else {
            auto* node_info = resp.mutable_node_info();
            node_info->set_vendor_name("Broadcom");
            node_info->set_chip_name(
                PrintBcmChipNumber(bcm_chip.ValueOrDie().type()));
          }
        }
        break;
      }
      case DataRequest::Request::kOpticalTransceiverInfo:
        // Retrieve current optical transceiver state from phal.
        status.Update(phal_interface_->GetOpticalTransceiverInfo(
            req.optical_transceiver_info().module(),
            req.optical_transceiver_info().network_interface(),
            resp.mutable_optical_transceiver_info()));
        break;
      default:
        status =
            MAKE_ERROR(ERR_UNIMPLEMENTED)
            << "DataRequest field "
            << req.descriptor()->FindFieldByNumber(req.request_case())->name()
            << " is not supported yet!";
    }
    if (status.ok()) {
      // If everything is OK send it to the caller.
      writer->Write(resp);
    }
    if (details) details->push_back(status);
  }
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<std::string>> BcmSwitch::VerifyState() {
  // TODO(unknown): Implement this.
  LOG(INFO) << "State verification is currently a NOP.";
  return std::vector<std::string>();
}

::util::Status BcmSwitch::SetValue(uint64 node_id, const SetRequest& request,
                                   std::vector<::util::Status>* details) {
  // TODO(tmadejski) add handling gNMI Set requests below.
  for (const auto& req : request.requests()) {
    ::util::Status status = ::util::OkStatus();
    switch (req.request_case()) {
      case SetRequest::Request::RequestCase::kPort:
        switch (req.port().value_case()) {
          case SetRequest::Request::Port::ValueCase::kAdminStatus:
          case SetRequest::Request::Port::ValueCase::kMacAddress:
          case SetRequest::Request::Port::ValueCase::kPortSpeed:
          case SetRequest::Request::Port::ValueCase::kLacpRouterMac:
          case SetRequest::Request::Port::ValueCase::kLacpSystemPriority:
          case SetRequest::Request::Port::ValueCase::kHealthIndicator:
            LOG(ERROR) << "Request " << req.port().ShortDebugString()
                       << " through SetValue() is ignored. Modify the "
                          "ChassisConfig instead!";
            break;
          case SetRequest::Request::Port::ValueCase::kLoopbackStatus: {
            absl::WriterMutexLock l(&chassis_lock);
            status.Update(bcm_chassis_manager_->SetPortLoopbackState(
                req.port().node_id(), req.port().port_id(),
                req.port().loopback_status().state()));
            break;
          }
          default:
            status = MAKE_ERROR(ERR_INTERNAL) << "Not supported yet!";
        }
        break;
      case SetRequest::Request::RequestCase::kOpticalNetworkInterface:
        switch (req.optical_network_interface().value_case()) {
          case SetRequest::Request::OpticalNetworkInterface::ValueCase::kOpticalTransceiverInfo: {  // NOLINT
            status.Update(phal_interface_->SetOpticalTransceiverInfo(
                req.optical_network_interface().module(),
                req.optical_network_interface().network_interface(),
                req.optical_network_interface().optical_transceiver_info()));
            break;
          }
          default:
            status = MAKE_ERROR(ERR_INTERNAL) << "Not supported yet!";
        }
        break;
      default:
        status = MAKE_ERROR(ERR_INTERNAL)
                 << req.ShortDebugString() << " Not supported yet!";
    }
    if (details) details->push_back(status);
  }
  return ::util::OkStatus();
}

std::unique_ptr<BcmSwitch> BcmSwitch::CreateInstance(
    PhalInterface* phal_interface, BcmChassisManager* bcm_chassis_manager,
    const std::map<int, BcmNode*>& unit_to_bcm_node) {
  return absl::WrapUnique(
      new BcmSwitch(phal_interface, bcm_chassis_manager, unit_to_bcm_node));
}

::util::Status BcmSwitch::DoVerifyChassisConfig(const ChassisConfig& config) {
  // First make sure PHAL is happy with the config then continue with the rest
  // of the managers and nodes.
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, phal_interface_->VerifyChassisConfig(config));
  APPEND_STATUS_IF_ERROR(status,
                         bcm_chassis_manager_->VerifyChassisConfig(config));
  // Get the current copy of the node_id_to_unit from chassis manager. If this
  // fails with ERR_NOT_INITIALIZED, do not verify anything at the node level.
  // Note that we do not expect any change in node_id_to_unit. Any change in
  // this map will be detected in bcm_chassis_manager_->VerifyChassisConfig.
  auto ret = bcm_chassis_manager_->GetNodeIdToUnitMap();
  if (!ret.ok()) {
    if (ret.status().error_code() != ERR_NOT_INITIALIZED) {
      APPEND_STATUS_IF_ERROR(status, ret.status());
    }
  } else {
    const auto& node_id_to_unit = ret.ValueOrDie();
    for (const auto& entry : node_id_to_unit) {
      uint64 node_id = entry.first;
      int unit = entry.second;
      BcmNode* bcm_node = gtl::FindPtrOrNull(unit_to_bcm_node_, unit);
      if (bcm_node == nullptr) {
        ::util::Status error = MAKE_ERROR(ERR_INVALID_PARAM)
                               << "Node ID " << node_id
                               << " mapped to unknown unit " << unit << ".";
        APPEND_STATUS_IF_ERROR(status, error);
        continue;
      }
      APPEND_STATUS_IF_ERROR(status,
                             bcm_node->VerifyChassisConfig(config, node_id));
    }
  }

  if (status.ok()) {
    LOG(INFO) << "Chassis config verified successfully.";
  }

  return status;
}

::util::Status BcmSwitch::DoVerifyForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  // Get the BcmNode pointer first. No need to continue if we cannot find one.
  ASSIGN_OR_RETURN(auto* bcm_node, GetBcmNodeFromNodeId(node_id));
  // Verify the forwarding config in all the managers and nodes.
  auto status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status,
                         bcm_node->VerifyForwardingPipelineConfig(config));

  if (status.ok()) {
    LOG(INFO) << "P4-based forwarding pipeline config verfied successfully for "
              << "node with ID " << node_id << ".";
  }

  return status;
}

::util::StatusOr<BcmNode*> BcmSwitch::GetBcmNodeFromUnit(int unit) const {
  BcmNode* bcm_node = gtl::FindPtrOrNull(unit_to_bcm_node_, unit);
  if (bcm_node == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Unit " << unit << " is unknown.";
  }
  return bcm_node;
}

::util::StatusOr<BcmNode*> BcmSwitch::GetBcmNodeFromNodeId(
    uint64 node_id) const {
  BcmNode* bcm_node = gtl::FindPtrOrNull(node_id_to_bcm_node_, node_id);
  if (bcm_node == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Node with ID " << node_id
           << " is unknown or no config has been pushed to it yet.";
  }
  return bcm_node;
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
