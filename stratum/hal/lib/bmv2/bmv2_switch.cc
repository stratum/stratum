// Copyright 2018-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bmv2/bmv2_switch.h"

#include <algorithm>
#include <map>
#include <set>
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
namespace bmv2 {

using ::stratum::hal::pi::PINode;

Bmv2Switch::Bmv2Switch(PhalInterface* phal_interface,
                       Bmv2ChassisManager* bmv2_chassis_manager,
                       const std::map<uint64, PINode*>& node_id_to_pi_node)
    : phal_interface_(ABSL_DIE_IF_NULL(phal_interface)),
      bmv2_chassis_manager_(ABSL_DIE_IF_NULL(bmv2_chassis_manager)),
      node_id_to_pi_node_(node_id_to_pi_node) {}

Bmv2Switch::~Bmv2Switch() {}

::util::Status Bmv2Switch::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  std::set<uint64> known_node_ids;
  std::set<uint64> new_node_ids;
  for (auto& node : node_id_to_pi_node_) known_node_ids.insert(node.first);
  for (auto& node : config.nodes()) new_node_ids.insert(node.id());
  if (known_node_ids != new_node_ids) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "The Bmv2Switch expects constant node ids";
  }
  RETURN_IF_ERROR(phal_interface_->PushChassisConfig(config));
  RETURN_IF_ERROR(bmv2_chassis_manager_->PushChassisConfig(config));
  for (auto& node : node_id_to_pi_node_) {
    // Sets the node_id for the PINode the first time, does not do anything in
    // subsequent calls (the node_id is constant).
    RETURN_IF_ERROR(node.second->PushChassisConfig(config, node.first));
  }
  return ::util::OkStatus();
}

::util::Status Bmv2Switch::VerifyChassisConfig(const ChassisConfig& config) {
  absl::ReaderMutexLock l(&chassis_lock);
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, phal_interface_->VerifyChassisConfig(config));
  APPEND_STATUS_IF_ERROR(status,
                         bmv2_chassis_manager_->VerifyChassisConfig(config));
  for (auto& node : node_id_to_pi_node_) {
    APPEND_STATUS_IF_ERROR(status,
                           node.second->PushChassisConfig(config, node.first));
  }
  return status;
}

::util::Status Bmv2Switch::PushForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  RETURN_IF_ERROR(pi_node->PushForwardingPipelineConfig(config));

  LOG(INFO) << "P4-based forwarding pipeline config pushed successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status Bmv2Switch::SaveForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  RETURN_IF_ERROR(pi_node->SaveForwardingPipelineConfig(config));

  LOG(INFO) << "P4-based forwarding pipeline config saved successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status Bmv2Switch::CommitForwardingPipelineConfig(uint64 node_id) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  RETURN_IF_ERROR(pi_node->CommitForwardingPipelineConfig());

  LOG(INFO) << "P4-based forwarding pipeline config committed successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status Bmv2Switch::VerifyForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->VerifyForwardingPipelineConfig(config);
}

::util::Status Bmv2Switch::Shutdown() {
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, bmv2_chassis_manager_->Shutdown());
  return status;
}

::util::Status Bmv2Switch::Freeze() { return ::util::OkStatus(); }

::util::Status Bmv2Switch::Unfreeze() { return ::util::OkStatus(); }

::util::Status Bmv2Switch::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  if (!req.updates_size()) return ::util::OkStatus();  // nothing to do.
  RET_CHECK(req.device_id()) << "No device_id in WriteRequest.";
  RET_CHECK(results != nullptr)
      << "Need to provide non-null results pointer for non-empty updates.";

  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(req.device_id()));
  return pi_node->WriteForwardingEntries(req, results);
}

::util::Status Bmv2Switch::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  RET_CHECK(req.device_id()) << "No device_id in ReadRequest.";
  RET_CHECK(writer) << "Channel writer must be non-null.";
  RET_CHECK(details) << "Details pointer must be non-null.";

  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(req.device_id()));
  return pi_node->ReadForwardingEntries(req, writer, details);
}

::util::Status Bmv2Switch::RegisterStreamMessageResponseWriter(
    uint64 node_id,
    std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>> writer) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->RegisterStreamMessageResponseWriter(writer);
}

::util::Status Bmv2Switch::UnregisterStreamMessageResponseWriter(
    uint64 node_id) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->UnregisterStreamMessageResponseWriter();
}

::util::Status Bmv2Switch::HandleStreamMessageRequest(
    uint64 node_id, const ::p4::v1::StreamMessageRequest& request) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->HandleStreamMessageRequest(request);
}

::util::Status Bmv2Switch::RegisterEventNotifyWriter(
    std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  return bmv2_chassis_manager_->RegisterEventNotifyWriter(writer);
}

::util::Status Bmv2Switch::UnregisterEventNotifyWriter() {
  return bmv2_chassis_manager_->UnregisterEventNotifyWriter();
}

::util::Status Bmv2Switch::RetrieveValue(uint64 /*node_id*/,
                                         const DataRequest& request,
                                         WriterInterface<DataResponse>* writer,
                                         std::vector<::util::Status>* details) {
  absl::ReaderMutexLock l(&chassis_lock);
  for (const auto& req : request.requests()) {
    ::util::StatusOr<DataResponse> resp;
    switch (req.request_case()) {
      case DataRequest::Request::kOperStatus:
      case DataRequest::Request::kAdminStatus:
      case DataRequest::Request::kMacAddress:
      case DataRequest::Request::kPortSpeed:
      case DataRequest::Request::kNegotiatedPortSpeed:
      case DataRequest::Request::kLacpRouterMac:
      case DataRequest::Request::kPortCounters:
      case DataRequest::Request::kHealthIndicator:
      case DataRequest::Request::kForwardingViability:
      case DataRequest::Request::kAutonegStatus:
      case DataRequest::Request::kSdnPortId:
        resp = bmv2_chassis_manager_->GetPortData(req);
        break;
      default:
        resp =
            MAKE_ERROR(ERR_UNIMPLEMENTED)
            << "DataRequest field "
            << req.descriptor()->FindFieldByNumber(req.request_case())->name()
            << " is not supported yet!";
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

::util::Status Bmv2Switch::SetValue(uint64 node_id, const SetRequest& request,
                                    std::vector<::util::Status>* details) {
  VLOG(1) << "Bmv2Switch::SetValue\n";
  LOG(INFO) << "Bmv2Switch::SetValue is not implemented yet, but changes will "
            << "be peformed when ChassisConfig is pushed again.";
  // TODO(antonin)
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<std::string>> Bmv2Switch::VerifyState() {
  return std::vector<std::string>();
}

std::unique_ptr<Bmv2Switch> Bmv2Switch::CreateInstance(
    PhalInterface* phal_interface, Bmv2ChassisManager* bmv2_chassis_manager,
    const std::map<uint64, PINode*>& node_id_to_pi_node) {
  return absl::WrapUnique(
      new Bmv2Switch(phal_interface, bmv2_chassis_manager, node_id_to_pi_node));
}

::util::StatusOr<PINode*> Bmv2Switch::GetPINodeFromNodeId(
    uint64 node_id) const {
  PINode* pi_node = gtl::FindPtrOrNull(node_id_to_pi_node_, node_id);
  if (pi_node == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Node with ID " << node_id
           << " is unknown or no config has been pushed to it yet.";
  }
  return pi_node;
}

}  // namespace bmv2
}  // namespace hal
}  // namespace stratum
