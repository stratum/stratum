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


#include "third_party/stratum/hal/lib/bcm/bcm_switch.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "third_party/stratum/glue/logging.h"
#include "third_party/stratum/glue/status/status_macros.h"
#include "third_party/stratum/lib/constants.h"
#include "third_party/stratum/lib/macros.h"
#include "third_party/absl/base/integral_types.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/synchronization/mutex.h"
#include "third_party/absl/time/clock.h"
#include "util/gtl/map_util.h"

namespace stratum {
namespace hal {
namespace bcm {

BcmSwitch::BcmSwitch(PhalInterface* phal_interface,
                     BcmChassisManager* bcm_chassis_manager,
                     const std::map<int, BcmNode*>& unit_to_bcm_node)
    : phal_interface_(CHECK_NOTNULL(phal_interface)),
      bcm_chassis_manager_(CHECK_NOTNULL(bcm_chassis_manager)),
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
    uint64 node_id, const ::p4::ForwardingPipelineConfig& config) {
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

::util::Status BcmSwitch::VerifyForwardingPipelineConfig(
    uint64 node_id, const ::p4::ForwardingPipelineConfig& config) {
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
  // TODO: Implement this.
  return ::util::OkStatus();
}

::util::Status BcmSwitch::Unfreeze() {
  // TODO: Implement this.
  return ::util::OkStatus();
}

::util::Status BcmSwitch::WriteForwardingEntries(
    const ::p4::WriteRequest& req, std::vector<::util::Status>* results) {
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
    const ::p4::ReadRequest& req, WriterInterface<::p4::ReadResponse>* writer,
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
    const std::shared_ptr<WriterInterface<::p4::PacketIn>>& writer) {
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
                                         const ::p4::PacketOut& packet) {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  // Get BcmNode which the node_id is associated with.
  ASSIGN_OR_RETURN(auto* bcm_node, GetBcmNodeFromNodeId(node_id));
  return bcm_node->TransmitPacket(packet);
}

::util::Status BcmSwitch::RegisterEventNotifyWriter(
    const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer) {
  absl::ReaderMutexLock l(&chassis_lock);
  if (shutdown) {
    return MAKE_ERROR(ERR_CANCELLED) << "Switch is shutdown.";
  }
  RETURN_IF_ERROR(bcm_chassis_manager_->RegisterEventNotifyWriter(writer));
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
  // TODO(b/69920763): Implement this. The code below is just a placeholder.
  for (const auto& req : request.request()) {
    DataResponse resp;
    ::util::Status status = ::util::OkStatus();
    if (req.has_oper_status()) {
      // Find operational status of port located at:
      // - node_id: req.oper_status().node_id()
      // - port_id: req.oper_status().port_id()
      // and then write it into the response.
      resp.mutable_oper_status()->set_oper_status(PORT_STATE_UP);
    } else if (req.has_admin_status()) {
      // Find administrative status of port located at:
      // - node_id: req.admin_status().node_id()
      // - port_id: req.admin_status().port_id()
      // and then write it into the response.
      resp.mutable_admin_status()->set_admin_status(ADMIN_STATE_ENABLED);
    } else if (req.has_lacp_system_id_mac()) {
      // Find LACP System ID MAC address of port located at:
      // - node_id: req.lacp_system_id_mac().node_id()
      // - port_id: req.lacp_system_id_mac().port_id()
      // and then write it into the response.
      resp.mutable_lacp_system_id_mac()->set_mac_address(0x112233445566ull);
    } else if (req.has_port_speed()) {
      // Find speed in bits per second of port located at:
      // - node_id: req.port_speed().node_id()
      // - port_id: req.port_speed().port_id()
      // and then write it into the response.
      resp.mutable_port_speed()->set_speed_bps(kFortyGigBps);
    } else if (req.has_lacp_system_priority()) {
      // Find LACP System priority of port located at:
      // - node_id: req.lacp_system_priority().node_id()
      // - port_id: req.lacp_system_priority().port_id()
      // and then write it into the response.
      resp.mutable_lacp_system_priority()->set_priority(1000);
    } else if (req.has_negotiated_port_speed()) {
      // Find negotiated speed in bits per second of port located at:
      // - node_id: req.negotiated_port_speed().node_id()
      // - port_id: req.negotiated_port_speed().port_id()
      // and then write it into the response.
      resp.mutable_negotiated_port_speed()->set_speed_bps(kFortyGigBps);
    } else if (req.has_mac_address()) {
      // TODO Find out why the controller needs it.
      // Find MAC address of port located at:
      // - node_id: req.mac_address().node_id()
      // - port_id: req.mac_address().port_id()
      // and then write it into the response.
      resp.mutable_mac_address()->set_mac_address(0x112233445566ull);
    } else if (req.has_port_counters()) {
      // Find current port counters for port located at:
      // - node_id: req.port_counters().node_id()
      // - port_id: req.port_counters().port_id()
      // and then write it into the response.
      auto* counters = resp.mutable_port_counters();
      // To simulate the counters being incremented the current time expressed
      // in nanoseconds since Jan 1st, 1970 is used.
      // TODO Remove this hack once the real counters are available.
      uint64 now = absl::GetCurrentTimeNanos();
      counters->set_in_octets(now);
      counters->set_out_octets(now);
      counters->set_in_unicast_pkts(now);
      counters->set_out_unicast_pkts(now);
      counters->set_in_broadcast_pkts(now);
      counters->set_out_broadcast_pkts(now);
      counters->set_in_multicast_pkts(now);
      counters->set_out_multicast_pkts(now);
      counters->set_in_discards(now);
      counters->set_out_discards(now);
      counters->set_in_unknown_protos(now);
      counters->set_in_errors(now);
      counters->set_out_errors(now);
      counters->set_in_fcs_errors(now);
    } else if (req.has_memory_error_alarm()) {
      // Find current state of memory-error alarm
      // and then write it into the response.
      auto* alarm = resp.mutable_memory_error_alarm();
      uint64 now = absl::GetCurrentTimeNanos();
      alarm->set_status(true);
      alarm->set_time_created(now);
      alarm->set_severity(DataResponse::Alarm::CRITICAL);
      alarm->set_description("memory-error alarm");
    } else if (req.has_flow_programming_exception_alarm()) {
      // Find current state of flow-programing-exception alarm
      // and then write it into the response.
      auto* alarm = resp.mutable_flow_programming_exception_alarm();
      uint64 now = absl::GetCurrentTimeNanos();
      alarm->set_status(true);
      alarm->set_time_created(now);
      alarm->set_severity(DataResponse::Alarm::CRITICAL);
      alarm->set_description("flow-programming-exception alarm");
    } else if (req.has_port_qos_counters()) {
      // Find current counters for port's qos queue located at:
      // - node_id: req.port_qos_counters().node_id()
      // - port_id: req.port_qos_counters().port_id()
      // - queue_id: req.port_qos_counters().queue_id()
      // and then write it into the response.
      auto* counters = resp.mutable_port_qos_counters();
      // To simulate the counters being incremented the current time expressed
      // in nanoseconds since Jan 1st, 1970 is used.
      // TODO Remove this hack once the real counters are available.
      uint64 now = absl::GetCurrentTimeNanos();
      counters->set_out_octets(now);
      counters->set_out_pkts(now);
      counters->set_out_dropped_pkts(now);
      counters->set_queue_id(req.port_qos_counters().queue_id());
    } else {
      status = MAKE_ERROR(ERR_INTERNAL) << "Not supported yet!";
    }
    if (status == ::util::OkStatus()) {
      // If everything is OK send it to the caller.
      writer->Write(resp);
    }
    if (details) details->push_back(status);
  }
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<std::string>> BcmSwitch::VerifyState() {
  // TODO: Implement this.
  return std::vector<std::string>();
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
    uint64 node_id, const ::p4::ForwardingPipelineConfig& config) {
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
