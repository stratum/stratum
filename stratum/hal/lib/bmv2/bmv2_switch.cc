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

#include "stratum/hal/lib/bmv2/bmv2_switch.h"

#include <algorithm>
#include <map>
#include <vector>

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
namespace bmv2 {

Bmv2Switch::Bmv2Switch(PhalInterface* phal_interface,
                       Bmv2ChassisManager* bmv2_chassis_manager,
                       const std::map<uint64, PINode*>& node_id_to_pi_node)
    : phal_interface_(CHECK_NOTNULL(phal_interface)),
      bmv2_chassis_manager_(CHECK_NOTNULL(bmv2_chassis_manager)),
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
  return ::util::OkStatus();
}

::util::Status Bmv2Switch::Freeze() {
  return ::util::OkStatus();
}

::util::Status Bmv2Switch::Unfreeze() {
  return ::util::OkStatus();
}

::util::Status Bmv2Switch::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  if (!req.updates_size()) return ::util::OkStatus();  // nothing to do.
  CHECK_RETURN_IF_FALSE(req.device_id()) << "No device_id in WriteRequest.";
  CHECK_RETURN_IF_FALSE(results != nullptr)
      << "Need to provide non-null results pointer for non-empty updates.";

  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(req.device_id()));
  return pi_node->WriteForwardingEntries(req, results);
}

::util::Status Bmv2Switch::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  CHECK_RETURN_IF_FALSE(req.device_id()) << "No device_id in ReadRequest.";
  CHECK_RETURN_IF_FALSE(writer) << "Channel writer must be non-null.";
  CHECK_RETURN_IF_FALSE(details) << "Details pointer must be non-null.";

  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(req.device_id()));
  return pi_node->ReadForwardingEntries(req, writer, details);
}

::util::Status Bmv2Switch::RegisterPacketReceiveWriter(
    uint64 node_id,
    std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> writer) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->RegisterPacketReceiveWriter(writer);
}

::util::Status Bmv2Switch::UnregisterPacketReceiveWriter(uint64 node_id) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->UnregisterPacketReceiveWriter();
}

::util::Status Bmv2Switch::TransmitPacket(uint64 node_id,
                                        const ::p4::v1::PacketOut& packet) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  return pi_node->TransmitPacket(packet);
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
    DataResponse resp;
    ::util::Status status = ::util::OkStatus();
    switch (req.request_case()) {
      // Get singleton port operational state.
      case DataRequest::Request::kOperStatus: {
        auto port_state = bmv2_chassis_manager_->GetPortState(
            req.oper_status().node_id(), req.oper_status().port_id());
        if (!port_state.ok()) {
          status.Update(port_state.status());
        } else {
          resp.mutable_oper_status()->set_state(port_state.ValueOrDie());
        }
        break;
      }
      case DataRequest::Request::kPortCounters: {
        status = bmv2_chassis_manager_->GetPortCounters(
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

::util::Status Bmv2Switch::SetValue(uint64 node_id, const SetRequest& request,
                        std::vector<::util::Status>* details) {
  //FIXME not implemented
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<std::string>> Bmv2Switch::VerifyState() {
  return std::vector<std::string>();
}

std::unique_ptr<Bmv2Switch> Bmv2Switch::CreateInstance(
    PhalInterface* phal_interface,
    Bmv2ChassisManager* bmv2_chassis_manager,
    const std::map<uint64, PINode*>& node_id_to_pi_node) {
  return absl::WrapUnique(new Bmv2Switch(
      phal_interface, bmv2_chassis_manager, node_id_to_pi_node));
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
