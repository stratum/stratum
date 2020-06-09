// Copyright 2018-present Barefoot Networks, Inc.
// Copyright 2019-present Dell EMC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/np4intel/np4_switch.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/pi/pi_node.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace np4intel {

using ::pi::fe::proto::DeviceMgr;
using ::stratum::hal::pi::PINode;

NP4Switch::NP4Switch(PhalInterface* phal_interface,
                     NP4ChassisManager* np4_chassis_manager)
    : phal_interface_(ABSL_DIE_IF_NULL(phal_interface)),
      np4_chassis_manager_(ABSL_DIE_IF_NULL(np4_chassis_manager)) {}

NP4Switch::~NP4Switch() {}

::util::Status NP4Switch::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);

  // Free all PI nodes first
  node_id_to_pi_node_.clear();
  node_id_to_device_mgr_.clear();
  // Create PI Nodes
  // Note: we use the node_id for the device_id
  for (auto& node : config.nodes()) {
    // Allocate PI node
    std::unique_ptr<DeviceMgr> device_mgr(new DeviceMgr(node.id()));
    node_id_to_pi_node_[node.id()] =
        pi::PINode::CreateInstance(device_mgr.get(), node.id());
    node_id_to_device_mgr_[node.id()] = std::move(device_mgr);
  }

  RETURN_IF_ERROR(phal_interface_->PushChassisConfig(config));
  RETURN_IF_ERROR(np4_chassis_manager_->PushChassisConfig(config));
  for (auto& node : node_id_to_pi_node_) {
    // Sets the node_id for the PINode the first time, does not do anything in
    // subsequent calls (the node_id is constant).
    RETURN_IF_ERROR(node.second->PushChassisConfig(config, node.first));
  }
  return ::util::OkStatus();
}

::util::Status NP4Switch::VerifyChassisConfig(const ChassisConfig& config) {
  absl::ReaderMutexLock l(&chassis_lock);
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, phal_interface_->VerifyChassisConfig(config));
  APPEND_STATUS_IF_ERROR(status,
                         np4_chassis_manager_->VerifyChassisConfig(config));
  for (auto& node : node_id_to_pi_node_) {
    APPEND_STATUS_IF_ERROR(status,
                           node.second->PushChassisConfig(config, node.first));
  }
  return status;
}

::util::Status NP4Switch::PushForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  RETURN_IF_ERROR(pi_node->PushForwardingPipelineConfig(config));

  LOG(INFO) << "P4-based forwarding pipeline config pushed successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status NP4Switch::SaveForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  RETURN_IF_ERROR(pi_node->SaveForwardingPipelineConfig(config));

  LOG(INFO) << "P4-based forwarding pipeline config saved successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status NP4Switch::CommitForwardingPipelineConfig(uint64 node_id) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  RETURN_IF_ERROR(pi_node->CommitForwardingPipelineConfig());

  LOG(INFO) << "P4-based forwarding pipeline config committed successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status NP4Switch::VerifyForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->VerifyForwardingPipelineConfig(config);
}

::util::Status NP4Switch::Shutdown() {
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, np4_chassis_manager_->Shutdown());
  return status;
}

::util::Status NP4Switch::Freeze() { return ::util::OkStatus(); }

::util::Status NP4Switch::Unfreeze() { return ::util::OkStatus(); }

::util::Status NP4Switch::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  if (!req.updates_size()) return ::util::OkStatus();  // nothing to do.
  CHECK_RETURN_IF_FALSE(req.device_id()) << "No device_id in WriteRequest.";
  CHECK_RETURN_IF_FALSE(results != nullptr)
      << "Need to provide non-null results pointer for non-empty updates.";

  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(req.device_id()));
  return pi_node->WriteForwardingEntries(req, results);
}

::util::Status NP4Switch::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  CHECK_RETURN_IF_FALSE(req.device_id()) << "No device_id in ReadRequest.";
  CHECK_RETURN_IF_FALSE(writer) << "Channel writer must be non-null.";
  CHECK_RETURN_IF_FALSE(details) << "Details pointer must be non-null.";

  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(req.device_id()));
  return pi_node->ReadForwardingEntries(req, writer, details);
}

::util::Status NP4Switch::RegisterPacketReceiveWriter(
    uint64 node_id,
    std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> writer) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->RegisterPacketReceiveWriter(writer);
}

::util::Status NP4Switch::UnregisterPacketReceiveWriter(uint64 node_id) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->UnregisterPacketReceiveWriter();
}

::util::Status NP4Switch::TransmitPacket(uint64 node_id,
                                         const ::p4::v1::PacketOut& packet) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->TransmitPacket(packet);
}

::util::Status NP4Switch::RegisterEventNotifyWriter(
    std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  return np4_chassis_manager_->RegisterEventNotifyWriter(writer);
}

::util::Status NP4Switch::UnregisterEventNotifyWriter() {
  return np4_chassis_manager_->UnregisterEventNotifyWriter();
}

::util::Status NP4Switch::RetrieveValue(uint64 /*node_id*/,
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
        resp = np4_chassis_manager_->GetPortData(req);
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

::util::Status NP4Switch::SetValue(uint64 node_id, const SetRequest& request,
                                   std::vector<::util::Status>* details) {
  VLOG(1) << "NP4Switch::SetValue\n";
  LOG(INFO) << "NP4Switch::SetValue is not implemented yet, but changes will "
            << "be peformed when ChassisConfig is pushed again.";
  // TODO(antonin)
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<std::string>> NP4Switch::VerifyState() {
  return std::vector<std::string>();
}

std::unique_ptr<NP4Switch> NP4Switch::CreateInstance(
    PhalInterface* phal_interface, NP4ChassisManager* np4_chassis_manager) {
  return absl::WrapUnique(new NP4Switch(phal_interface, np4_chassis_manager));
}

::util::StatusOr<PINode*> NP4Switch::GetPINodeFromNodeId(uint64 node_id) const {
  auto it = node_id_to_pi_node_.find(node_id);
  if (it == node_id_to_pi_node_.end()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Node with ID " << node_id
           << " is unknown or no config has been pushed to it yet.";
  }
  return it->second.get();
}

}  // namespace np4intel
}  // namespace hal
}  // namespace stratum
