// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_switch.h"

#include <algorithm>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_node.h"
#include "stratum/hal/lib/barefoot/utils.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace barefoot {

BfrtSwitch::BfrtSwitch(
    PhalInterface* phal_interface, BfChassisManager* bf_chassis_manager,
    BfSdeInterface* bf_sde_interface,
    const absl::flat_hash_map<int, BfrtNode*>& device_id_to_bfrt_node)
    : phal_interface_(ABSL_DIE_IF_NULL(phal_interface)),
      bf_sde_interface_(ABSL_DIE_IF_NULL(bf_sde_interface)),
      bf_chassis_manager_(ABSL_DIE_IF_NULL(bf_chassis_manager)),
      device_id_to_bfrt_node_(device_id_to_bfrt_node),
      node_id_to_bfrt_node_() {
  for (const auto& entry : device_id_to_bfrt_node_) {
    CHECK_GE(entry.first, 0)
        << "Invalid device_id number " << entry.first << ".";
    CHECK_NE(entry.second, nullptr)
        << "Detected null BfrtNode for device_id " << entry.first << ".";
  }
}

BfrtSwitch::~BfrtSwitch() {}

::util::Status BfrtSwitch::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  RETURN_IF_ERROR(DoVerifyChassisConfig(config));
  RETURN_IF_ERROR(phal_interface_->PushChassisConfig(config));
  RETURN_IF_ERROR(bf_chassis_manager_->PushChassisConfig(config));
  ASSIGN_OR_RETURN(const auto& node_id_to_device_id,
                   bf_chassis_manager_->GetNodeIdToDeviceMap());
  node_id_to_bfrt_node_.clear();
  for (const auto& entry : node_id_to_device_id) {
    uint64 node_id = entry.first;
    int device_id = entry.second;
    ASSIGN_OR_RETURN(auto* bfrt_node, GetBfrtNodeFromDeviceId(device_id));
    RETURN_IF_ERROR(bfrt_node->PushChassisConfig(config, node_id));
    node_id_to_bfrt_node_[node_id] = bfrt_node;
  }

  LOG(INFO) << "Chassis config pushed successfully.";

  return ::util::OkStatus();
}

::util::Status BfrtSwitch::VerifyChassisConfig(const ChassisConfig& config) {
  absl::ReaderMutexLock l(&chassis_lock);
  return DoVerifyChassisConfig(config);
}

::util::Status BfrtSwitch::PushForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  RETURN_IF_ERROR(DoVerifyForwardingPipelineConfig(node_id, config));
  ASSIGN_OR_RETURN(auto* bfrt_node, GetBfrtNodeFromNodeId(node_id));
  RETURN_IF_ERROR(bfrt_node->PushForwardingPipelineConfig(config));
  RETURN_IF_ERROR(bf_chassis_manager_->ReplayChassisConfig(node_id));

  LOG(INFO) << "P4-based forwarding pipeline config pushed successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status BfrtSwitch::SaveForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  ASSIGN_OR_RETURN(auto* bfrt_node, GetBfrtNodeFromNodeId(node_id));
  RETURN_IF_ERROR(bfrt_node->SaveForwardingPipelineConfig(config));
  RETURN_IF_ERROR(bf_chassis_manager_->ReplayChassisConfig(node_id));

  LOG(INFO) << "P4-based forwarding pipeline config saved successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status BfrtSwitch::CommitForwardingPipelineConfig(uint64 node_id) {
  absl::WriterMutexLock l(&chassis_lock);
  ASSIGN_OR_RETURN(auto* bfrt_node, GetBfrtNodeFromNodeId(node_id));
  RETURN_IF_ERROR(bfrt_node->CommitForwardingPipelineConfig());

  LOG(INFO) << "P4-based forwarding pipeline config committed successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status BfrtSwitch::VerifyForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  // TODO(max): This should be a ReaderMutexLock?
  absl::WriterMutexLock l(&chassis_lock);
  return DoVerifyForwardingPipelineConfig(node_id, config);
}

::util::Status BfrtSwitch::Shutdown() {
  // The shutdown flag must be checked on all read or write accesses to
  // state protected by chassis_lock, whether within RPC executions or
  // event handler threads.
  {
    absl::WriterMutexLock l(&chassis_lock);
    shutdown = true;
  }

  ::util::Status status = ::util::OkStatus();
  for (const auto& entry : device_id_to_bfrt_node_) {
    BfrtNode* node = entry.second;
    APPEND_STATUS_IF_ERROR(status, node->Shutdown());
  }
  APPEND_STATUS_IF_ERROR(status, bf_chassis_manager_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, phal_interface_->Shutdown());
  // APPEND_STATUS_IF_ERROR(status, bf_sde_interface_->Shutdown());
  node_id_to_bfrt_node_.clear();

  return status;
}

::util::Status BfrtSwitch::Freeze() { return ::util::OkStatus(); }

::util::Status BfrtSwitch::Unfreeze() { return ::util::OkStatus(); }

::util::Status BfrtSwitch::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  if (!req.updates_size()) return ::util::OkStatus();  // nothing to do.
  RET_CHECK(req.device_id()) << "No device_id in WriteRequest.";
  RET_CHECK(results != nullptr)
      << "Need to provide non-null results pointer for non-empty updates.";

  absl::ReaderMutexLock l(&chassis_lock);
  ASSIGN_OR_RETURN(auto* bfrt_node, GetBfrtNodeFromNodeId(req.device_id()));
  return bfrt_node->WriteForwardingEntries(req, results);
}

::util::Status BfrtSwitch::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  RET_CHECK(req.device_id()) << "No device_id in ReadRequest.";
  RET_CHECK(writer) << "Channel writer must be non-null.";
  RET_CHECK(details) << "Details pointer must be non-null.";

  absl::ReaderMutexLock l(&chassis_lock);
  ASSIGN_OR_RETURN(auto* bfrt_node, GetBfrtNodeFromNodeId(req.device_id()));
  return bfrt_node->ReadForwardingEntries(req, writer, details);
}

::util::Status BfrtSwitch::RegisterStreamMessageResponseWriter(
    uint64 node_id,
    std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>> writer) {
  ASSIGN_OR_RETURN(auto* bfrt_node, GetBfrtNodeFromNodeId(node_id));
  return bfrt_node->RegisterStreamMessageResponseWriter(writer);
}

::util::Status BfrtSwitch::UnregisterStreamMessageResponseWriter(
    uint64 node_id) {
  ASSIGN_OR_RETURN(auto* bfrt_node, GetBfrtNodeFromNodeId(node_id));
  return bfrt_node->UnregisterStreamMessageResponseWriter();
}

::util::Status BfrtSwitch::HandleStreamMessageRequest(
    uint64 node_id, const ::p4::v1::StreamMessageRequest& request) {
  ASSIGN_OR_RETURN(auto* bfrt_node, GetBfrtNodeFromNodeId(node_id));
  return bfrt_node->HandleStreamMessageRequest(request);
}

::util::Status BfrtSwitch::RegisterEventNotifyWriter(
    std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  return bf_chassis_manager_->RegisterEventNotifyWriter(writer);
}

::util::Status BfrtSwitch::UnregisterEventNotifyWriter() {
  return bf_chassis_manager_->UnregisterEventNotifyWriter();
}

::util::Status BfrtSwitch::RetrieveValue(uint64 node_id,
                                         const DataRequest& request,
                                         WriterInterface<DataResponse>* writer,
                                         std::vector<::util::Status>* details) {
  absl::ReaderMutexLock l(&chassis_lock);
  for (const auto& req : request.requests()) {
    DataResponse resp;
    ::util::Status status = ::util::OkStatus();
    switch (req.request_case()) {
      case DataRequest::Request::kOperStatus:
      case DataRequest::Request::kAdminStatus:
      case DataRequest::Request::kMacAddress:
      case DataRequest::Request::kPortSpeed:
      case DataRequest::Request::kNegotiatedPortSpeed:
      case DataRequest::Request::kLacpRouterMac:
      case DataRequest::Request::kPortCounters:
      case DataRequest::Request::kForwardingViability:
      case DataRequest::Request::kHealthIndicator:
      case DataRequest::Request::kAutonegStatus:
      case DataRequest::Request::kFrontPanelPortInfo:
      case DataRequest::Request::kLoopbackStatus:
      case DataRequest::Request::kSdnPortId: {
        auto port_data = bf_chassis_manager_->GetPortData(req);
        if (!port_data.ok()) {
          status.Update(port_data.status());
        } else {
          resp = port_data.ConsumeValueOrDie();
        }
        break;
      }
      case DataRequest::Request::kNodeInfo: {
        auto device_id =
            bf_chassis_manager_->GetDeviceFromNodeId(req.node_info().node_id());
        if (!device_id.ok()) {
          status.Update(device_id.status());
        } else {
          auto* node_info = resp.mutable_node_info();
          node_info->set_vendor_name("Barefoot");
          node_info->set_chip_name(
              bf_sde_interface_->GetBfChipType(device_id.ValueOrDie()));
        }
        break;
      }
      default:
        status =
            MAKE_ERROR(ERR_UNIMPLEMENTED)
            << "DataRequest field "
            << req.descriptor()->FindFieldByNumber(req.request_case())->name()
            << " is not supported yet!";
        break;
    }
    if (status.ok()) {
      // If everything is OK send it to the caller.
      writer->Write(resp);
    }
    if (details) details->push_back(status);
  }
  return ::util::OkStatus();
}

::util::Status BfrtSwitch::SetValue(uint64 node_id, const SetRequest& request,
                                    std::vector<::util::Status>* details) {
  LOG(INFO) << "BfrtSwitch::SetValue is not implemented yet, but changes will "
            << "be performed when ChassisConfig is pushed again. "
            << request.ShortDebugString() << ".";

  return ::util::OkStatus();
}

::util::StatusOr<std::vector<std::string>> BfrtSwitch::VerifyState() {
  return std::vector<std::string>();
}

std::unique_ptr<BfrtSwitch> BfrtSwitch::CreateInstance(
    PhalInterface* phal_interface, BfChassisManager* bf_chassis_manager,
    BfSdeInterface* bf_sde_interface,
    const absl::flat_hash_map<int, BfrtNode*>& device_id_to_bfrt_node) {
  return absl::WrapUnique(new BfrtSwitch(phal_interface, bf_chassis_manager,
                                         bf_sde_interface,
                                         device_id_to_bfrt_node));
}

::util::Status BfrtSwitch::DoVerifyForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  // Get the BfrtNode pointer first. No need to continue if we cannot find one.
  ASSIGN_OR_RETURN(auto* bfrt_node, GetBfrtNodeFromNodeId(node_id));
  // Verify the forwarding config in all the managers and nodes.
  auto status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status,
                         bfrt_node->VerifyForwardingPipelineConfig(config));

  if (status.ok()) {
    LOG(INFO) << "P4-based forwarding pipeline config verified successfully"
              << " for node with ID " << node_id << ".";
  }

  return status;
}

::util::Status BfrtSwitch::DoVerifyChassisConfig(const ChassisConfig& config) {
  // First make sure PHAL is happy with the config then continue with the rest
  // of the managers and nodes.
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, phal_interface_->VerifyChassisConfig(config));
  APPEND_STATUS_IF_ERROR(status,
                         bf_chassis_manager_->VerifyChassisConfig(config));
  // Get the current copy of the node_id_to_device from chassis manager. If this
  // fails with ERR_NOT_INITIALIZED, do not verify anything at the node level.
  // Note that we do not expect any change in node_id_to_device. Any change in
  // this map will be detected in bf_chassis_manager_->VerifyChassisConfig.
  auto ret = bf_chassis_manager_->GetNodeIdToDeviceMap();
  if (!ret.ok()) {
    if (ret.status().error_code() != ERR_NOT_INITIALIZED) {
      APPEND_STATUS_IF_ERROR(status, ret.status());
    }
  } else {
    const auto& node_id_to_device_id = ret.ValueOrDie();
    for (const auto& entry : node_id_to_device_id) {
      uint64 node_id = entry.first;
      int device_id = entry.second;
      BfrtNode* bfrt_node =
          gtl::FindPtrOrNull(device_id_to_bfrt_node_, device_id);
      if (bfrt_node == nullptr) {
        ::util::Status error = MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
                               << "Node ID " << node_id
                               << " mapped to unknown device " << device_id
                               << ".";
        APPEND_STATUS_IF_ERROR(status, error);
        continue;
      }
      APPEND_STATUS_IF_ERROR(status,
                             bfrt_node->VerifyChassisConfig(config, node_id));
    }
  }

  if (status.ok()) {
    LOG(INFO) << "Chassis config verified successfully.";
  }

  return status;
}

::util::StatusOr<BfrtNode*> BfrtSwitch::GetBfrtNodeFromDeviceId(
    int device_id) const {
  BfrtNode* bfrt_node = gtl::FindPtrOrNull(device_id_to_bfrt_node_, device_id);
  if (bfrt_node == nullptr) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "Device " << device_id << " is unknown.";
  }
  return bfrt_node;
}

::util::StatusOr<BfrtNode*> BfrtSwitch::GetBfrtNodeFromNodeId(
    uint64 node_id) const {
  BfrtNode* bfrt_node = gtl::FindPtrOrNull(node_id_to_bfrt_node_, node_id);
  if (bfrt_node == nullptr) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "Node with ID " << node_id
           << " is unknown or no config has been pushed to it yet.";
  }
  return bfrt_node;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
