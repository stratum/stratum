/* Copyright 2018-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
                   const std::map<int, PINode*>& unit_to_pi_node)
    : phal_interface_(CHECK_NOTNULL(phal_interface)),
      bf_chassis_manager_(CHECK_NOTNULL(bf_chassis_manager)),
      unit_to_pi_node_(unit_to_pi_node),
      node_id_to_pi_node_() {
  for (const auto& entry : unit_to_pi_node_) {
    CHECK_GE(entry.first, 0) << "Invalid unit number " << entry.first << ".";
    // TODO(antonin): investigate why this doesn't compile
    // CHECK_NE(entry.second, nullptr)
    //     << "Detected null PINode for unit " << entry.first << ".";
  }
  // TODO(antonin): this is temporary until we implement the PushChassisConfig
  // method.
  for (const auto& entry : unit_to_pi_node_) {
    auto node_id = entry.second->GetNodeId();
    node_id_to_pi_node_[node_id] = entry.second;
  }
}

BFSwitch::~BFSwitch() {}

::util::Status BFSwitch::PushChassisConfig(const ChassisConfig& config) {
  RETURN_IF_ERROR(phal_interface_->PushChassisConfig(config));
  return ::util::OkStatus();
}

::util::Status BFSwitch::VerifyChassisConfig(const ChassisConfig& config) {
  (void)config;
  return ::util::OkStatus();
}

::util::Status BFSwitch::PushForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  RETURN_IF_ERROR(pi_node->PushForwardingPipelineConfig(config));

  LOG(INFO) << "P4-based forwarding pipeline config pushed successfully to "
            << "node with ID " << node_id << ".";

  return ::util::OkStatus();
}

::util::Status BFSwitch::VerifyForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->VerifyForwardingPipelineConfig(config);
}

::util::Status BFSwitch::Shutdown() {
  return ::util::OkStatus();
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
  RETURN_IF_ERROR(bf_chassis_manager_->RegisterEventNotifyWriter(writer));
  return ::util::OkStatus();
}

::util::Status BFSwitch::UnregisterEventNotifyWriter() {
  return ::util::OkStatus();
}

::util::Status BFSwitch::RetrieveValue(uint64 node_id,
                                       const DataRequest& request,
                                       WriterInterface<DataResponse>* writer,
                                       std::vector<::util::Status>* details) {
  for (const auto& req : request.requests()) {
    DataResponse resp;
    ::util::Status status = ::util::OkStatus();
    switch (req.request_case()) {
      // Get singleton port operational state.
      case DataRequest::Request::kOperStatus: {
        auto port_state = bf_chassis_manager_->GetPortState(
            req.oper_status().node_id(), req.oper_status().port_id());
        if (!port_state.ok()) {
          status.Update(port_state.status());
        } else {
          resp.mutable_oper_status()->set_state(port_state.ValueOrDie());
        }
        break;
      }
      case DataRequest::Request::kPortCounters: {
        status = bf_chassis_manager_->GetPortCounters(
            req.port_counters().node_id(),
            req.port_counters().port_id(),
            resp.mutable_port_counters());
        break;
      }
      default:
        // TODO(antonin)
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

::util::Status BFSwitch::SetValue(uint64 node_id, const SetRequest& request,
                                  std::vector<::util::Status>* details) {
  (void)node_id;
  (void)request;
  (void)details;
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<std::string>> BFSwitch::VerifyState() {
  return std::vector<std::string>();
}

std::unique_ptr<BFSwitch> BFSwitch::CreateInstance(
    PhalInterface* phal_interface,
    BFChassisManager* bf_chassis_manager,
    const std::map<int, PINode*>& unit_to_pi_node) {
  return absl::WrapUnique(
      new BFSwitch(phal_interface, bf_chassis_manager, unit_to_pi_node));
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
