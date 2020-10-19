// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/dummy/dummy_switch.h"

#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/memory/memory.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/dummy/dummy_global_vars.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

::util::Status DummySwitch::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;
  auto phal_status = phal_interface_->PushChassisConfig(config);
  auto chassis_mgr_status = chassis_mgr_->PushChassisConfig(config);
  if (!phal_status.ok()) {
    return phal_status;
  }
  if (!chassis_mgr_status.ok()) {
    return chassis_mgr_status;
  }
  dummy_nodes_.clear();
  node_port_id_to_slot.clear();
  node_port_id_to_port.clear();
  for (auto& node : config.nodes()) {
    LOG(INFO) <<
      absl::StrFormat("Creating node \"%s\" (id: %d). Slot %d, Index: %d.",
                      node.name(), node.id(), node.slot(), node.index());
    auto new_node = DummyNode::CreateInstance(node.id(), node.name(),
                                              node.slot(), node.index());

    // Since "PushChassisConfig" called after "RegisterEventNotifyWriter"
    // We also need to register writer from nodes
    new_node->RegisterEventNotifyWriter(gnmi_event_writer_);
    new_node->PushChassisConfig(config);
    dummy_nodes_.emplace(node.id(), new_node);
  }

  for (const auto& singleton_port : config.singleton_ports()) {
    uint64 node_id = singleton_port.node();
    uint32 port_id = singleton_port.id();
    int32 slot = singleton_port.slot();
    int32 port = singleton_port.port();
    std::pair<uint64, uint32> node_port_pair = std::make_pair(node_id, port_id);
    node_port_id_to_slot.emplace(node_port_pair, slot);
    node_port_id_to_port.emplace(node_port_pair, port);
  }

  return ::util::OkStatus();
}

::util::Status DummySwitch::VerifyChassisConfig(const ChassisConfig& config) {
  absl::ReaderMutexLock l(&chassis_lock);
  // TODO(Yi Tseng): Implement this method.
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::Status DummySwitch::PushForwardingPipelineConfig(
    uint64 node_id,
    const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::ReaderMutexLock l(&chassis_lock);
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, GetDummyNode(node_id));
  return node->PushForwardingPipelineConfig(config);
}

::util::Status DummySwitch::SaveForwardingPipelineConfig(
    uint64 node_id,
    const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::ReaderMutexLock l(&chassis_lock);
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, GetDummyNode(node_id));
  return MAKE_ERROR(ERR_UNIMPLEMENTED)
      << "SaveForwardingPipelineConfig not implemented for this target";
}

::util::Status DummySwitch::CommitForwardingPipelineConfig(uint64 node_id) {
  absl::ReaderMutexLock l(&chassis_lock);
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, GetDummyNode(node_id));
  return MAKE_ERROR(ERR_UNIMPLEMENTED)
      << "CommitForwardingPipelineConfig not implemented for this target";
}

::util::Status DummySwitch::VerifyForwardingPipelineConfig(
    uint64 node_id,
    const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::ReaderMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, GetDummyNode(node_id));
  return node->VerifyForwardingPipelineConfig(config);
}

::util::Status DummySwitch::Shutdown() {
  absl::WriterMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;
  RETURN_IF_ERROR(phal_interface_->Shutdown());
  bool successful = true;
  for (auto kv : dummy_nodes_) {
    auto node = kv.second;
    auto node_status = node -> Shutdown();
    if (!node_status.ok()) {
      LOG(ERROR) << "Got error while shutting down node " << node->Name()
                 << node_status.ToString();
      successful = false;
      // Continue shutting down other nodes.
    }
  }
  shutdown = chassis_mgr_->Shutdown().ok() && successful;
  return shutdown ? ::util::OkStatus() :
      ::util::Status(::util::error::INTERNAL,
                     "Got error while shutting down the switch");
}
::util::Status DummySwitch::Freeze() {
  absl::WriterMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;
  bool successful = true;
  for (auto kv : dummy_nodes_) {
    auto node = kv.second;
    auto node_status = node -> Freeze();
    if (!node_status.ok()) {
      LOG(ERROR) << "Got error while freezing node " << node->Name()
                 << node_status.ToString();
      successful = false;
      // Continue freezing other nodes.
    }
  }
  return successful ? chassis_mgr_->Freeze() :
      ::util::Status(::util::error::INTERNAL,
                     "Got error while freezing the switch");
}
::util::Status DummySwitch::Unfreeze() {
  absl::WriterMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;
  bool successful = true;
  for (auto kv : dummy_nodes_) {
    auto node = kv.second;
    auto node_status = node -> Unfreeze();
    if (!node_status.ok()) {
      LOG(ERROR) << "Got error while unfreezing node " << node->Name()
                 << node_status.ToString();
      successful = false;
      // Continue unfreezing other nodes.
    }
  }
  return successful ? chassis_mgr_->Unfreeze() :
      ::util::Status(::util::error::INTERNAL,
                     "Got error while unfreezing the switch");
}
::util::Status DummySwitch::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req,
    std::vector<::util::Status>* results) {
  absl::ReaderMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;
  uint64 node_id = req.device_id();
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, GetDummyNode(node_id));
  return node->WriteForwardingEntries(req, results);
}

::util::Status DummySwitch::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  absl::ReaderMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;
  uint64 node_id = req.device_id();
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, GetDummyNode(node_id));
  return node->ReadForwardingEntries(req, writer, details);
}

::util::Status DummySwitch::RegisterPacketReceiveWriter(
    uint64 node_id,
    std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> writer) {
  absl::ReaderMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, GetDummyNode(node_id));
  return node->RegisterPacketReceiveWriter(writer);
}

::util::Status DummySwitch::UnregisterPacketReceiveWriter(uint64 node_id) {
  absl::ReaderMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, GetDummyNode(node_id));
  return node->UnregisterPacketReceiveWriter();
}

::util::Status DummySwitch::TransmitPacket(uint64 node_id,
                              const ::p4::v1::PacketOut& packet) {
  absl::ReaderMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, GetDummyNode(node_id));
  return node->TransmitPacket(packet);
}

::util::Status DummySwitch::RegisterEventNotifyWriter(
    std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  absl::WriterMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;
  gnmi_event_writer_ = writer;
  return chassis_mgr_->RegisterEventNotifyWriter(writer);
}

::util::Status DummySwitch::UnregisterEventNotifyWriter() {
  absl::WriterMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;
  for (auto node : GetDummyNodes()) {
    node->UnregisterEventNotifyWriter();
  }
  gnmi_event_writer_.reset();
  return chassis_mgr_->UnregisterEventNotifyWriter();
}

::util::Status DummySwitch::RetrieveValue(uint64 node_id,
                             const DataRequest& requests,
                             WriterInterface<DataResponse>* writer,
                             std::vector<::util::Status>* details) {
  absl::ReaderMutexLock l(&chassis_lock);
  LOG(INFO) << __FUNCTION__;

  DummyNode* dummy_node;
  if (node_id != 0) {
    ASSIGN_OR_RETURN(dummy_node, GetDummyNode(node_id));
  }
  DataResponse resp_val;
  ::util::StatusOr<DataResponse> resp;
  for (const auto& request : requests.requests()) {
    switch (request.request_case()) {
      case Request::kOperStatus:
      case Request::kAdminStatus:
      case Request::kMacAddress:
      case Request::kPortSpeed:
      case Request::kNegotiatedPortSpeed:
      case Request::kLacpRouterMac:
      case Request::kLacpSystemPriority:
      case Request::kPortCounters:
      case Request::kForwardingViability:
      case Request::kHealthIndicator:
      case Request::kHardwarePort:
        resp = dummy_node->RetrievePortData(request);
        break;
      case Request::kMemoryErrorAlarm:
      case Request::kFlowProgrammingExceptionAlarm:
      case Request::kNodeInfo:
        resp = chassis_mgr_->RetrieveChassisData(request);
        break;
      case Request::kPortQosCounters:
        resp = dummy_node->RetrievePortQosData(request);
        break;
      case Request::kFrontPanelPortInfo: {
        FrontPanelPortInfo front_panel_port_info;
        std::pair<uint64, uint32> node_port_pair =
            std::make_pair(request.front_panel_port_info().node_id(),
                           request.front_panel_port_info().port_id());
        int slot = node_port_id_to_slot[node_port_pair];
        int port = node_port_id_to_port[node_port_pair];
        ::util::Status status =
            phal_interface_->GetFrontPanelPortInfo(slot, port,
                                  resp_val.mutable_front_panel_port_info());
        if (status.ok()) {
          resp = resp_val;
        }
        break;
      }
      case DataRequest::Request::kOpticalTransceiverInfo: {
        ::util::Status status = phal_interface_->GetOpticalTransceiverInfo(
            request.optical_transceiver_info().module(),
            request.optical_transceiver_info().network_interface(),
            resp_val.mutable_optical_transceiver_info());
        if (status.ok()) {
          resp = resp_val;
        }
        break;
      }
      default:
        resp = MAKE_ERROR(ERR_INTERNAL) << "Not supported yet";
        break;
    }
    if (resp.ok()) {
      writer->Write(resp.ValueOrDie());
    } else if (details) {
      details->push_back(resp.status());
    }
  }
  return ::util::OkStatus();
}

::util::Status DummySwitch::SetValue(uint64 node_id, const SetRequest& request,
                                     std::vector<::util::Status>* details) {
  absl::ReaderMutexLock l(&chassis_lock);
  // TODO(Yi Tseng): Implement this method.
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<std::string>> DummySwitch::VerifyState() {
  // TODO(Yi Tseng): Implement this method.
  return std::vector<std::string>();
}

std::vector<DummyNode*> DummySwitch::GetDummyNodes() {
  // TODO(Yi Tseng) find more efficient ways to implement this.
  std::vector<DummyNode*> nodes;
  nodes.reserve(dummy_nodes_.size());
  for (auto kv : dummy_nodes_) {
    nodes.emplace_back(kv.second);
  }
  return nodes;
}

::util::StatusOr<DummyNode*> DummySwitch::GetDummyNode(uint64 node_id) {
  auto node_element = dummy_nodes_.find(node_id);
  if (node_element == dummy_nodes_.end()) {
    return MAKE_ERROR(::util::error::NOT_FOUND)
      << "DummyNode with id " << node_id << " not found.";
  }
  return ::util::StatusOr<DummyNode*>(node_element->second);
}

std::unique_ptr<DummySwitch>
  DummySwitch::CreateInstance(PhalInterface* phal_interface,
                              DummyChassisManager* chassis_mgr) {
  return absl::WrapUnique(new DummySwitch(phal_interface, chassis_mgr));
}

DummySwitch::~DummySwitch() {}

DummySwitch::DummySwitch(PhalInterface* phal_interface,
                         DummyChassisManager* chassis_mgr)
  : phal_interface_(phal_interface),
    chassis_mgr_(chassis_mgr),
    dummy_nodes_(::absl::flat_hash_map<uint64, DummyNode*>()),
    gnmi_event_writer_(nullptr) {
}

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum
