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

#include "stratum/hal/lib/dummy/dummy_switch.h"

#include <string>
#include <vector>

#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "absl/memory/memory.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/dummy/dummy_node.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

::util::Status DummySwitch::PushChassisConfig(const ChassisConfig& config) {
  LOG(INFO) << __FUNCTION__;
  auto phal_status = phal_interface_->PushChassisConfig(config);
  auto chassis_mgr_status = chassis_mgr_->PushChassisConfig(config);
  if (!phal_status.ok()) {
    return phal_status;
  }
  return chassis_mgr_status;
}

::util::Status DummySwitch::VerifyChassisConfig(const ChassisConfig& config) {
  // TODO(Yi Tseng): Implement this method.
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::Status DummySwitch::PushForwardingPipelineConfig(
    uint64 node_id,
    const ::p4::v1::ForwardingPipelineConfig& config) {
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, chassis_mgr_->GetDummyNode(node_id));
  return node->PushForwardingPipelineConfig(config);
}

::util::Status DummySwitch::VerifyForwardingPipelineConfig(
    uint64 node_id,
    const ::p4::v1::ForwardingPipelineConfig& config) {
  LOG(INFO) << __FUNCTION__;
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, chassis_mgr_->GetDummyNode(node_id));
  return node->VerifyForwardingPipelineConfig(config);
}

::util::Status DummySwitch::Shutdown() {
  LOG(INFO) << __FUNCTION__;
  RETURN_IF_ERROR(phal_interface_->Shutdown());
  return chassis_mgr_->Shutdown();
}
::util::Status DummySwitch::Freeze() {
  LOG(INFO) << __FUNCTION__;
  return chassis_mgr_->Freeze();
}
::util::Status DummySwitch::Unfreeze() {
  LOG(INFO) << __FUNCTION__;
  return chassis_mgr_->Unfreeze();
}
::util::Status DummySwitch::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req,
    std::vector<::util::Status>* results) {
  LOG(INFO) << __FUNCTION__;
  uint64 node_id = req.device_id();
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, chassis_mgr_->GetDummyNode(node_id));
  return node->WriteForwardingEntries(req, results);
}

::util::Status DummySwitch::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  LOG(INFO) << __FUNCTION__;
  uint64 node_id = req.device_id();
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, chassis_mgr_->GetDummyNode(node_id));
  return node->ReadForwardingEntries(req, writer, details);
}

::util::Status DummySwitch::RegisterPacketReceiveWriter(
    uint64 node_id,
    std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> writer) {
  LOG(INFO) << __FUNCTION__;
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, chassis_mgr_->GetDummyNode(node_id));
  return node->RegisterPacketReceiveWriter(writer);
}

::util::Status DummySwitch::UnregisterPacketReceiveWriter(uint64 node_id) {
  LOG(INFO) << __FUNCTION__;
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, chassis_mgr_->GetDummyNode(node_id));
  return node->UnregisterPacketReceiveWriter();
}

::util::Status DummySwitch::TransmitPacket(uint64 node_id,
                              const ::p4::v1::PacketOut& packet) {
  LOG(INFO) << __FUNCTION__;
  DummyNode* node = nullptr;
  ASSIGN_OR_RETURN(node, chassis_mgr_->GetDummyNode(node_id));
  return node->TransmitPacket(packet);
}

::util::Status DummySwitch::RegisterEventNotifyWriter(
    std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  LOG(INFO) << __FUNCTION__;
  return chassis_mgr_->RegisterEventNotifyWriter(writer);
}

::util::Status DummySwitch::UnregisterEventNotifyWriter() {
  LOG(INFO) << __FUNCTION__;
  return chassis_mgr_->UnregisterEventNotifyWriter();
}

::util::Status DummySwitch::RetrieveValue(uint64 node_id,
                             const DataRequest& requests,
                             WriterInterface<DataResponse>* writer,
                             std::vector<::util::Status>* details) {
  LOG(INFO) << __FUNCTION__;
  return chassis_mgr_->RetrieveValue(node_id, requests, writer, details);
}

::util::Status DummySwitch::SetValue(uint64 node_id, const SetRequest& request,
                                     std::vector<::util::Status>* details) {
  // TODO(Yi Tseng): Implement this method.
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<std::string>> DummySwitch::VerifyState() {
  // TODO(Yi Tseng): Implement this method.
  return std::vector<std::string>();
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
    chassis_mgr_(chassis_mgr) {
}

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum
