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

#include "stratum/hal/lib/dummy/dummy_chassis_mgr.h"

#include <memory>
#include <string>

#include "absl/strings/str_format.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/gtl/flat_hash_map.h"
#include "stratum/hal/lib/common/common.pb.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

using Request = stratum::hal::DataRequest::Request;

// Global lock for chassis manager
::absl::Mutex chassis_lock_;
::absl::Mutex gnmi_event_lock_;
DummyChassisManager* chassis_mgr_singleton_ = nullptr;

DummyChassisManager::DummyChassisManager()
: dummy_nodes_(::stratum::gtl::flat_hash_map<int, DummyNode*>()),
  dummy_sdk_(DummySDK::GetSingleton()) { }

DummyChassisManager::~DummyChassisManager() {}

::util::Status
DummyChassisManager::PushChassisConfig(const ChassisConfig& config) {
  LOG(INFO) << __FUNCTION__;
  for (auto& node : config.nodes()) {
    LOG(INFO) <<
      absl::StrFormat("Creating node \"%s\" (id: %d). Slot %d, Index: %d.",
                      node.name(), node.id(), node.slot(), node.index());
    auto new_node = DummyNode::CreateInstance(node.id(), node.name(),
                                              node.slot(), node.index());

    // Since "PushChassisConfig" called after "RegisterEventNotifyWriter"
    // We also need to register writer from nodes
    new_node->RegisterEventNotifyWriter(chassis_event_writer_);
    new_node->PushChassisConfig(config);
    dummy_nodes_.emplace(node.id(), new_node);
  }
  return ::util::OkStatus();
}

::util::Status
DummyChassisManager::VerifyChassisConfig(const ChassisConfig& config) {
  LOG(INFO) << __FUNCTION__;
  // TODO(Yi Tseng) Verify the chassis config.
  return ::util::OkStatus();
}

::util::Status DummyChassisManager::Shutdown() {
  LOG(INFO) << __FUNCTION__;
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
  return successful ? ::util::OkStatus() :
      ::util::Status(::util::error::INTERNAL,
                     "Got error while shutting down the chassis");
}

::util::Status DummyChassisManager::Freeze() {
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
  return successful ? ::util::OkStatus() :
      ::util::Status(::util::error::INTERNAL,
                     "Got error while freezing the chassis");
}

::util::Status DummyChassisManager::Unfreeze() {
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
  return successful ? ::util::OkStatus() :
      ::util::Status(::util::error::INTERNAL,
                     "Got error while unfreezing the chassis");
}

DummyChassisManager* DummyChassisManager::GetSingleton() {
  if (chassis_mgr_singleton_ == nullptr) {
    chassis_mgr_singleton_ = new DummyChassisManager();
  }
  return chassis_mgr_singleton_;
}

::util::StatusOr<DummyNode*> DummyChassisManager::GetDummyNode(uint64 node_id) {
  auto node_element = dummy_nodes_.find(node_id);
  if (node_element == dummy_nodes_.end()) {
    return MAKE_ERROR(::util::error::NOT_FOUND) << "DummyNode with id " << node_id
                                 << " not found.";
  }
  return ::util::StatusOr<DummyNode*>(node_element->second);
}

std::vector<DummyNode*> DummyChassisManager::GetDummyNodes() {
  // TODO(Yi Tseng) find more efficient ways to implement this.
  std::vector<DummyNode*> nodes;
  nodes.reserve(dummy_nodes_.size());
  for (auto kv : dummy_nodes_) {
    nodes.emplace_back(kv.second);
  }
  return nodes;
}

::util::Status DummyChassisManager::RegisterEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  if (chassis_event_writer_) {
    return MAKE_ERROR(ERR_INTERNAL) << "Event notify writer already exists";
  }
  // Event notify writer will be registered by node later
  chassis_event_writer_ = writer;
  dummy_sdk_->RegisterChassisEventNotifyWriter(chassis_event_writer_);
  return ::util::OkStatus();
}

::util::Status DummyChassisManager::UnregisterEventNotifyWriter() {
  for (auto node : GetDummyNodes()) {
    node->UnregisterEventNotifyWriter();
  }
  chassis_event_writer_.reset();
  dummy_sdk_->UnregisterChassisEventNotifyWriter();
  return ::util::OkStatus();
}

::util::StatusOr<DataResponse>
DummyChassisManager::RetrieveChassisData(const Request request) {
  // TODO(Yi Tseng): Implement this method.
  return MAKE_ERROR(ERR_INTERNAL) << "Not supported yet!";
}

::util::Status
DummyChassisManager::RetrieveValue(uint64 node_id,
                                   const DataRequest& requests,
                                   WriterInterface<DataResponse>* writer,
                                   std::vector<::util::Status>* details) {
  auto dummy_node = GetDummyNode(node_id);
  DataResponse resp_val;
  ::util::StatusOr<DataResponse> resp;
  for (auto request : requests.requests()) {
    if (!dummy_node.ok()) {
      // fill error status if we can not find dummy node with node_id
      if (details) {
        details->push_back(dummy_node.status());
      }
      continue;
    }
    auto dummy_node_val = dummy_node.ValueOrDie();
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
        resp = dummy_node_val->RetrievePortData(request);
        break;
      case Request::kMemoryErrorAlarm:
      case Request::kFlowProgrammingExceptionAlarm:
        resp = RetrieveChassisData(request);
        break;
      case Request::kPortQosCounters:
        resp = dummy_node_val->RetrievePortQosData(request);
        break;
      default:
        resp = MAKE_ERROR(ERR_INTERNAL) << "Not supported yet";
        break;
    }
    if (resp.ok()) {
      writer->Write(resp.ValueOrDie());
    }
    if (details) {
      details->push_back(resp.status());
    }
  }
  return ::util::OkStatus();
}

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

