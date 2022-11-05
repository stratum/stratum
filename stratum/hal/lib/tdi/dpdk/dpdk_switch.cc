// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/tdi/dpdk/dpdk_switch.h"

#include <algorithm>
#include <map>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/tdi/dpdk/dpdk_chassis_manager.h"
#include "stratum/hal/lib/tdi/dpdk/dpdk_switch.h"
#include "stratum/hal/lib/tdi/tdi_node.h"
#include "stratum/hal/lib/tdi/utils.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace tdi {

DpdkSwitch::DpdkSwitch(
    DpdkChassisManager* chassis_manager, TdiSdeInterface* sde_interface,
    const absl::flat_hash_map<int, TdiNode*>& device_id_to_tdi_node)
    : sde_interface_(ABSL_DIE_IF_NULL(sde_interface)),
      chassis_manager_(ABSL_DIE_IF_NULL(chassis_manager)),
      device_id_to_tdi_node_(device_id_to_tdi_node),
      node_id_to_tdi_node_() {
  for (const auto& entry : device_id_to_tdi_node_) {
    CHECK_GE(entry.first, 0)
        << "Invalid device_id number " << entry.first << ".";
  }
}

DpdkSwitch::~DpdkSwitch() {}

::util::Status DpdkSwitch::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  RETURN_IF_ERROR(chassis_manager_->PushChassisConfig(config));
  ASSIGN_OR_RETURN(const auto& node_id_to_device_id,
                   chassis_manager_->GetNodeIdToUnitMap());
  node_id_to_tdi_node_.clear();
  for (const auto& entry : node_id_to_device_id) {
    uint64 node_id = entry.first;
    int device_id = entry.second;
    ASSIGN_OR_RETURN(auto* tdi_node, GetTdiNodeFromDeviceId(device_id));
    RETURN_IF_ERROR(tdi_node->PushChassisConfig(config, node_id));
    node_id_to_tdi_node_[node_id] = tdi_node;
  }

  LOG(INFO) << "Chassis config pushed successfully.";

  return ::util::OkStatus();
}

::util::Status DpdkSwitch::VerifyChassisConfig(const ChassisConfig& config) {
  (void)config;
  return ::util::OkStatus();
}

::util::Status DpdkSwitch::PushForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  ASSIGN_OR_RETURN(auto* tdi_node, GetTdiNodeFromNodeId(node_id));
  RETURN_IF_ERROR(tdi_node->PushForwardingPipelineConfig(config));
  return ::util::OkStatus();
}

::util::Status DpdkSwitch::SaveForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  ASSIGN_OR_RETURN(auto* tdi_node, GetTdiNodeFromNodeId(node_id));
  RETURN_IF_ERROR(tdi_node->SaveForwardingPipelineConfig(config));
  return ::util::OkStatus();
}

::util::Status DpdkSwitch::CommitForwardingPipelineConfig(uint64 node_id) {
  absl::WriterMutexLock l(&chassis_lock);
  ASSIGN_OR_RETURN(auto* tdi_node, GetTdiNodeFromNodeId(node_id));
  RETURN_IF_ERROR(tdi_node->CommitForwardingPipelineConfig());
  return ::util::OkStatus();
}

::util::Status DpdkSwitch::VerifyForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  ASSIGN_OR_RETURN(auto* tdi_node, GetTdiNodeFromNodeId(node_id));
  return tdi_node->VerifyForwardingPipelineConfig(config);
}

::util::Status DpdkSwitch::Shutdown() {
  ::util::Status status = ::util::OkStatus();
  for (const auto& entry : device_id_to_tdi_node_) {
    TdiNode* node = entry.second;
    APPEND_STATUS_IF_ERROR(status, node->Shutdown());
  }
  APPEND_STATUS_IF_ERROR(status, chassis_manager_->Shutdown());
  return status;
}

::util::Status DpdkSwitch::Freeze() { return ::util::OkStatus(); }

::util::Status DpdkSwitch::Unfreeze() { return ::util::OkStatus(); }

::util::Status DpdkSwitch::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  if (!req.updates_size()) return ::util::OkStatus();  // nothing to do.
  RET_CHECK(req.device_id()) << "No device_id in WriteRequest.";
  RET_CHECK(results != nullptr) << "Results pointer must be non-null.";
  absl::ReaderMutexLock l(&chassis_lock);
  ASSIGN_OR_RETURN(auto* tdi_node, GetTdiNodeFromNodeId(req.device_id()));
  return tdi_node->WriteForwardingEntries(req, results);
}

::util::Status DpdkSwitch::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  RET_CHECK(req.device_id()) << "No device_id in ReadRequest.";
  RET_CHECK(writer) << "Channel writer must be non-null.";
  RET_CHECK(details) << "Details pointer must be non-null.";
  absl::ReaderMutexLock l(&chassis_lock);
  ASSIGN_OR_RETURN(auto* tdi_node, GetTdiNodeFromNodeId(req.device_id()));
  return tdi_node->ReadForwardingEntries(req, writer, details);
}

::util::Status DpdkSwitch::RegisterStreamMessageResponseWriter(
    uint64 node_id,
    std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>> writer) {
  ASSIGN_OR_RETURN(auto* tdi_node, GetTdiNodeFromNodeId(node_id));
  return tdi_node->RegisterStreamMessageResponseWriter(writer);
}

::util::Status DpdkSwitch::UnregisterStreamMessageResponseWriter(
    uint64 node_id) {
  ASSIGN_OR_RETURN(auto* tdi_node, GetTdiNodeFromNodeId(node_id));
  return tdi_node->UnregisterStreamMessageResponseWriter();
}

::util::Status DpdkSwitch::HandleStreamMessageRequest(
    uint64 node_id, const ::p4::v1::StreamMessageRequest& request) {
  ASSIGN_OR_RETURN(auto* tdi_node, GetTdiNodeFromNodeId(node_id));
  return tdi_node->HandleStreamMessageRequest(request);
}

::util::Status DpdkSwitch::RegisterEventNotifyWriter(
    std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  return chassis_manager_->RegisterEventNotifyWriter(writer);
}

::util::Status DpdkSwitch::UnregisterEventNotifyWriter() {
  return chassis_manager_->UnregisterEventNotifyWriter();
}

::util::Status DpdkSwitch::RetrieveValue(uint64 node_id,
                                         const DataRequest& request,
                                         WriterInterface<DataResponse>* writer,
                                         std::vector<::util::Status>* details) {
  absl::ReaderMutexLock l(&chassis_lock);
  for (const auto& req : request.requests()) {
    DataResponse resp;
    ::util::Status status = ::util::OkStatus();
    switch (req.request_case()) {
      // Port data request
      case DataRequest::Request::kAdminStatus:
      case DataRequest::Request::kMacAddress:
      case DataRequest::Request::kLacpRouterMac:
      case DataRequest::Request::kPortCounters:
      case DataRequest::Request::kSdnPortId: {
        auto port_data = chassis_manager_->GetPortData(req);
        if (!port_data.ok()) {
          status.Update(port_data.status());
        } else {
          resp = port_data.ConsumeValueOrDie();
        }
        break;
      }
      // Node information request
      case DataRequest::Request::kNodeInfo: {
        auto device_id =
            chassis_manager_->GetUnitFromNodeId(req.node_info().node_id());
        if (!device_id.ok()) {
          status.Update(device_id.status());
        } else {
          auto* node_info = resp.mutable_node_info();
          // TODO: implement generic interface through SDE interface.
          node_info->set_vendor_name("DPDK");
          node_info->set_chip_name(
              sde_interface_->GetChipType(device_id.ValueOrDie()));
        }
        break;
      }
      case DataRequest::Request::kOperStatus:
      case DataRequest::Request::kPortSpeed:
      case DataRequest::Request::kNegotiatedPortSpeed:
      case DataRequest::Request::kForwardingViability:
      case DataRequest::Request::kHealthIndicator:
      case DataRequest::Request::kAutonegStatus:
      case DataRequest::Request::kFrontPanelPortInfo:
      case DataRequest::Request::kLoopbackStatus:
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

::util::Status DpdkSwitch::SetValue(uint64 node_id, const SetRequest& request,
                                    std::vector<::util::Status>* details) {
#if 0
  // Since this is a known limitation, there is no need for us to log
  // every time this method is called. dgf 10/20/2022
  LOG(INFO) << "DpdkSwitch::SetValue is not implemented yet. Changes will "
            << "be applied when ChassisConfig is pushed again. "
            << request.ShortDebugString() << ".";
#endif
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<std::string>> DpdkSwitch::VerifyState() {
  return std::vector<std::string>();
}

std::unique_ptr<DpdkSwitch> DpdkSwitch::CreateInstance(
    DpdkChassisManager* chassis_manager, TdiSdeInterface* sde_interface,
    const absl::flat_hash_map<int, TdiNode*>& device_id_to_tdi_node) {
  return absl::WrapUnique(
      new DpdkSwitch(chassis_manager, sde_interface, device_id_to_tdi_node));
}

::util::StatusOr<TdiNode*> DpdkSwitch::GetTdiNodeFromDeviceId(
    int device_id) const {
  TdiNode* tdi_node = gtl::FindPtrOrNull(device_id_to_tdi_node_, device_id);
  if (tdi_node == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Unit " << device_id << " is unknown.";
  }
  return tdi_node;
}

::util::StatusOr<TdiNode*> DpdkSwitch::GetTdiNodeFromNodeId(
    uint64 node_id) const {
  TdiNode* tdi_node = gtl::FindPtrOrNull(node_id_to_tdi_node_, node_id);
  if (tdi_node == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Node with ID " << node_id
           << " is unknown or no config has been pushed to it yet.";
  }
  return tdi_node;
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
