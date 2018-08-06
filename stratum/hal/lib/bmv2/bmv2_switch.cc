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
                       const std::map<int, PINode*>& unit_to_pi_node)
    : phal_interface_(CHECK_NOTNULL(phal_interface)),
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

Bmv2Switch::~Bmv2Switch() {}

::util::Status Bmv2Switch::PushChassisConfig(const ChassisConfig& config) {
  RETURN_IF_ERROR(phal_interface_->PushChassisConfig(config));
  return ::util::OkStatus();
}

::util::Status Bmv2Switch::VerifyChassisConfig(const ChassisConfig& config) {
  (void)config;
  return ::util::OkStatus();
}

::util::Status Bmv2Switch::PushForwardingPipelineConfig(
    uint64 node_id, const ::p4::v1::ForwardingPipelineConfig& config) {
  ASSIGN_OR_RETURN(auto* pi_node, GetPINodeFromNodeId(node_id));
  RETURN_IF_ERROR(pi_node->PushForwardingPipelineConfig(config));

  LOG(INFO) << "P4-based forwarding pipeline config pushed successfully to "
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
    const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer) {
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
    const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer) {
  (void)writer;
  return ::util::OkStatus();
}

::util::Status Bmv2Switch::UnregisterEventNotifyWriter() {
  return ::util::OkStatus();
}

::util::Status Bmv2Switch::RetrieveValue(uint64 /*node_id*/,
                                         const DataRequest& request,
                                         WriterInterface<DataResponse>* writer,
                                         std::vector<::util::Status>* details) {
  (void)request;
  (void)writer;
  (void)details;
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<std::string>> Bmv2Switch::VerifyState() {
  return std::vector<std::string>();
}

std::unique_ptr<Bmv2Switch> Bmv2Switch::CreateInstance(
    PhalInterface* phal_interface,
    const std::map<int, PINode*>& unit_to_pi_node) {
  return absl::WrapUnique(new Bmv2Switch(phal_interface, unit_to_pi_node));
}

::util::StatusOr<PINode*> Bmv2Switch::GetPINodeFromUnit(int unit) const {
  PINode* pi_node = gtl::FindPtrOrNull(unit_to_pi_node_, unit);
  if (pi_node == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Unit " << unit << " is unknown.";
  }
  return pi_node;
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
