// Copyright 2018-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_switch.h"

#include <algorithm>
#include <map>
#include <vector>

#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"
#include "stratum/hal/lib/pi/pi_node.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/glue/integral_types.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/gtl/map_util.h"

using ::stratum::hal::pi::PINode;

namespace stratum {
namespace hal {
namespace barefoot {

BFSwitch::BFSwitch(PhalInterface* phal_interface,
                   BFChassisManager* bf_chassis_manager,
                   BFPdInterface* bf_pd_interface,
                   const std::map<int, PINode*>& unit_to_pi_node)
    : phal_interface_(CHECK_NOTNULL(phal_interface)),
      bf_chassis_manager_(CHECK_NOTNULL(bf_chassis_manager)),
      bf_pd_interface_(CHECK_NOTNULL(bf_pd_interface)),
      unit_to_pi_node_(unit_to_pi_node),
      node_id_to_pi_node_() {
  for (const auto& entry : unit_to_pi_node_) {
    CHECK_GE(entry.first, 0) << "Invalid unit number " << entry.first << ".";
    CHECK_NE(entry.second, nullptr)
        << "Detected null PINode for unit " << entry.first << ".";
  }
}

BFSwitch::~BFSwitch() {}

::util::Status BFSwitch::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  RETURN_IF_ERROR(phal_interface_->PushChassisConfig(config));
  RETURN_IF_ERROR(bf_chassis_manager_->PushChassisConfig(config));
  ASSIGN_OR_RETURN(const auto& node_id_to_unit,
                   bf_chassis_manager_->GetNodeIdToUnitMap());
  node_id_to_pi_node_.clear();
  for (const auto& entry : node_id_to_unit) {
    uint64 node_id = entry.first;
    int unit = entry.second;
    ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromUnit(unit));
    RETURN_IF_ERROR(pi_node->PushChassisConfig(config, node_id));
    node_id_to_pi_node_[node_id] = pi_node;
  }

  LOG(INFO) << "Chassis config pushed successfully.";

  return ::util::OkStatus();
}

::util::Status BFSwitch::VerifyChassisConfig(const ChassisConfig& config) {
  (void)config;
  return ::util::OkStatus();
}

::util::Status BFSwitch::PushForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  RETURN_IF_ERROR(pi_node->PushForwardingPipelineConfig(config));
  RETURN_IF_ERROR(bf_chassis_manager_->ReplayPortsConfig(node_id));

  LOG(INFO) << "P4-based forwarding pipeline config pushed successfully to "
            << "node with ID " << node_id << ".";

  ASSIGN_OR_RETURN(const auto& node_id_to_unit,
                   bf_chassis_manager_->GetNodeIdToUnitMap());

  CHECK_RETURN_IF_FALSE(gtl::ContainsKey(node_id_to_unit, node_id))
      << "Unable to find unit number for node " << node_id;
  int unit = gtl::FindOrDie(node_id_to_unit, node_id);
  ASSIGN_OR_RETURN(auto cpu_port, bf_pd_interface_->PcieCpuPortGet(unit));
  RETURN_IF_ERROR(bf_pd_interface_->TmSetCpuPort(unit, cpu_port));
  return ::util::OkStatus();
}

::util::Status BFSwitch::SaveForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  RETURN_IF_ERROR(pi_node->SaveForwardingPipelineConfig(config));
  RETURN_IF_ERROR(bf_chassis_manager_->ReplayPortsConfig(node_id));

  LOG(INFO) << "P4-based forwarding pipeline config saved successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status BFSwitch::CommitForwardingPipelineConfig(uint64 node_id) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  RETURN_IF_ERROR(pi_node->CommitForwardingPipelineConfig());

  LOG(INFO) << "P4-based forwarding pipeline config committed successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status BFSwitch::VerifyForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->VerifyForwardingPipelineConfig(config);
}

::util::Status BFSwitch::Shutdown() {
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, bf_chassis_manager_->Shutdown());
  return status;
}

::util::Status BFSwitch::Freeze() {
  return ::util::OkStatus();
}

::util::Status BFSwitch::Unfreeze() {
  return ::util::OkStatus();
}

::util::Status BFSwitch::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  if (!req.updates_size()) return ::util::OkStatus();  // nothing to do.
  CHECK_RETURN_IF_FALSE(req.device_id()) << "No device_id in WriteRequest.";
  CHECK_RETURN_IF_FALSE(results != nullptr)
      << "Need to provide non-null results pointer for non-empty updates.";

  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(req.device_id()));
  return pi_node->WriteForwardingEntries(req, results);
}

::util::Status BFSwitch::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  CHECK_RETURN_IF_FALSE(req.device_id()) << "No device_id in ReadRequest.";
  CHECK_RETURN_IF_FALSE(writer) << "Channel writer must be non-null.";
  CHECK_RETURN_IF_FALSE(details) << "Details pointer must be non-null.";

  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(req.device_id()));
  return pi_node->ReadForwardingEntries(req, writer, details);
}

::util::Status BFSwitch::RegisterPacketReceiveWriter(
    uint64 node_id,
    std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> writer) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->RegisterPacketReceiveWriter(writer);
}

::util::Status BFSwitch::UnregisterPacketReceiveWriter(uint64 node_id) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->UnregisterPacketReceiveWriter();
}

::util::Status BFSwitch::TransmitPacket(uint64 node_id,
                                        const ::p4::v1::PacketOut& packet) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->TransmitPacket(packet);
}

::util::Status BFSwitch::RegisterEventNotifyWriter(
    std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  return bf_chassis_manager_->RegisterEventNotifyWriter(writer);
}

::util::Status BFSwitch::UnregisterEventNotifyWriter() {
  return bf_chassis_manager_->UnregisterEventNotifyWriter();
}

::util::Status BFSwitch::RetrieveValue(uint64 node_id,
                                       const DataRequest& request,
                                       WriterInterface<DataResponse>* writer,
                                       std::vector<::util::Status>* details) {
  absl::ReaderMutexLock l(&chassis_lock);
  for (const auto& req : request.requests()) {
    ::util::StatusOr<DataResponse> resp;
    switch (req.request_case()) {
      case DataRequest::Request::kOperStatus:
      case DataRequest::Request::kAdminStatus:
      case DataRequest::Request::kPortSpeed:
      case DataRequest::Request::kNegotiatedPortSpeed:
      case DataRequest::Request::kPortCounters:
      case DataRequest::Request::kAutonegStatus:
      case DataRequest::Request::kFrontPanelPortInfo:
      case DataRequest::Request::kLoopbackStatus:
        resp = bf_chassis_manager_->GetPortData(req);
        break;
      default:
        // TODO(antonin)
        resp = MAKE_ERROR(ERR_INTERNAL) << "Not supported yet";
        break;
    }
    if (resp.ok()) {
      // If everything is OK send it to the caller.
      writer->Write(resp.ValueOrDie());
    }
    if (details) details->push_back(resp.status());
  }
  return ::util::OkStatus();
}

::util::Status BFSwitch::SetValue(uint64 node_id, const SetRequest& request,
                                  std::vector<::util::Status>* details) {
  (void)node_id;
  (void)request;
  (void)details;
  LOG(INFO) << "BFSwitch::SetValue is not implemented yet, but changes will "
            << "be peformed when ChassisConfig is pushed again.";
  // TODO(antonin)
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<std::string>> BFSwitch::VerifyState() {
  return std::vector<std::string>();
}

std::unique_ptr<BFSwitch> BFSwitch::CreateInstance(
    PhalInterface* phal_interface,
    BFChassisManager* bf_chassis_manager,
    BFPdInterface* bf_pd_interface,
    const std::map<int, PINode*>& unit_to_pi_node) {
  return absl::WrapUnique(
      new BFSwitch(phal_interface, bf_chassis_manager, bf_pd_interface,
                   unit_to_pi_node));
}

::util::StatusOr<PINode*> BFSwitch::GetPINodeFromUnit(int unit) const {
  PINode* pi_node = gtl::FindPtrOrNull(unit_to_pi_node_, unit);
  if (pi_node == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Unit " << unit << " is unknown.";
  }
  return pi_node;
}

::util::StatusOr<PINode*> BFSwitch::GetPINodeFromNodeId(
    uint64 node_id) const {
  PINode* pi_node = gtl::FindPtrOrNull(node_id_to_pi_node_, node_id);
  if (pi_node == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Node with ID " << node_id
           << " is unknown or no config has been pushed to it yet.";
  }
  return pi_node;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
