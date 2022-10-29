// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/tdi/tdi_pre_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/tdi/tdi_constants.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {
namespace tdi {

TdiPreManager::TdiPreManager(TdiSdeInterface* tdi_sde_interface, int device)
    : tdi_sde_interface_(ABSL_DIE_IF_NULL(tdi_sde_interface)), device_(device) {}

::util::Status TdiPreManager::PushForwardingPipelineConfig(
    const TdiDeviceConfig& config) {
  absl::WriterMutexLock l(&lock_);
  return ::util::OkStatus();
}

::util::Status TdiPreManager::WritePreEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type& type, const PreEntry& entry) {
  absl::WriterMutexLock l(&lock_);
  switch (entry.type_case()) {
    case PreEntry::kMulticastGroupEntry:
      return WriteMulticastGroupEntry(session, type,
                                      entry.multicast_group_entry());
    case PreEntry::kCloneSessionEntry:
      return WriteCloneSessionEntry(session, type, entry.clone_session_entry());
    default:
      return MAKE_ERROR(ERR_UNIMPLEMENTED)
          << "Unsupported PRE entry: " << entry.ShortDebugString();
  }
}

::util::Status TdiPreManager::ReadPreEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const PreEntry& entry, WriterInterface<::p4::v1::ReadResponse>* writer) {
  absl::ReaderMutexLock l(&lock_);
  switch (entry.type_case()) {
    case PreEntry::kMulticastGroupEntry: {
      RETURN_IF_ERROR(ReadMulticastGroupEntry(
          session, entry.multicast_group_entry(), writer));
      break;
    }
    case PreEntry::kCloneSessionEntry: {
      RETURN_IF_ERROR(
          ReadCloneSessionEntry(session, entry.clone_session_entry(), writer));
      break;
    }
    default:
      return MAKE_ERROR(ERR_UNIMPLEMENTED)
          << "Unsupported PRE entry: " << entry.ShortDebugString();
  }

  return ::util::OkStatus();
}

std::unique_ptr<TdiPreManager> TdiPreManager::CreateInstance(
    TdiSdeInterface* tdi_sde_interface, int device) {
  return absl::WrapUnique(new TdiPreManager(tdi_sde_interface, device));
}

::util::StatusOr<std::vector<uint32>> TdiPreManager::InsertMulticastNodes(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::MulticastGroupEntry& entry) {
  const uint32 group_id = entry.multicast_group_id();
  RET_CHECK(group_id <= kMaxMulticastGroupId);

  // Collect instance (rid) -> egress ports mapping
  absl::flat_hash_map<uint32, std::vector<uint32>> instance_to_egress_ports;
  for (const auto& replica : entry.replicas()) {
    RET_CHECK(replica.instance() <= UINT16_MAX);
    instance_to_egress_ports[replica.instance()].push_back(
        replica.egress_port());
  }
  std::vector<uint32> new_nodes = {};
  // FIXME: We need to revert partial modifications in case of failures.
  for (const auto& replica : instance_to_egress_ports) {
    const uint32 instance = replica.first;
    const std::vector<uint32>& egress_ports = replica.second;
    std::vector<uint32> mc_lag_ids;
    ASSIGN_OR_RETURN(uint32 mc_node_id,
                     tdi_sde_interface_->CreateMulticastNode(
                         device_, session, instance, mc_lag_ids, egress_ports));
    new_nodes.push_back(mc_node_id);
  }

  return new_nodes;
}

// FIXME: We need to revert partial modifications in case of failures.
::util::Status TdiPreManager::WriteMulticastGroupEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type& type,
    const ::p4::v1::MulticastGroupEntry& entry) {
  VLOG(1) << ::p4::v1::Update_Type_Name(type) << " "
          << entry.ShortDebugString();
  ::util::Status status;
  switch (type) {
    case ::p4::v1::Update::INSERT: {
      ASSIGN_OR_RETURN(std::vector<uint32> mc_node_ids,
                       InsertMulticastNodes(session, entry));
      RETURN_IF_ERROR(tdi_sde_interface_->InsertMulticastGroup(
          device_, session, entry.multicast_group_id(), mc_node_ids));
      break;
    }
    case ::p4::v1::Update::MODIFY: {
      ASSIGN_OR_RETURN(auto current_node_ids,
                       tdi_sde_interface_->GetNodesInMulticastGroup(
                           device_, session, entry.multicast_group_id()));
      ASSIGN_OR_RETURN(std::vector<uint32> new_node_ids,
                       InsertMulticastNodes(session, entry));
      RETURN_IF_ERROR_WITH_APPEND(
          tdi_sde_interface_->ModifyMulticastGroup(
              device_, session, entry.multicast_group_id(), new_node_ids))
              .with_logging()
          << "Failed to write multicast group for request "
          << entry.ShortDebugString() << ".";
      RETURN_IF_ERROR_WITH_APPEND(tdi_sde_interface_->DeleteMulticastNodes(
                                      device_, session, current_node_ids))
              .with_logging()
          << "Failed to delete multicast nodes for request "
          << entry.ShortDebugString() << ".";
      break;
    }
    case ::p4::v1::Update::DELETE: {
      LOG_IF(WARNING, entry.replicas_size() != 0)
          << "Replicas are ignored on MulticastGroupEntry delete requests: "
          << entry.ShortDebugString() << ".";
      ASSIGN_OR_RETURN(auto node_ids,
                       tdi_sde_interface_->GetNodesInMulticastGroup(
                           device_, session, entry.multicast_group_id()));
      RETURN_IF_ERROR_WITH_APPEND(
          tdi_sde_interface_->DeleteMulticastGroup(device_, session,
                                                  entry.multicast_group_id()))
              .with_logging()
          << "Failed to delete multicast group for request "
          << entry.ShortDebugString() << ".";
      RETURN_IF_ERROR_WITH_APPEND(
          tdi_sde_interface_->DeleteMulticastNodes(device_, session, node_ids))
              .with_logging()
          << "Failed to delete multicast nodes for request "
          << entry.ShortDebugString() << ".";
      break;
    }
    default:
      return MAKE_ERROR(ERR_UNIMPLEMENTED) << "Unsupported update type: " << type;
  }
  return ::util::OkStatus();
}

::util::Status TdiPreManager::ReadMulticastGroupEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::MulticastGroupEntry& entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  std::vector<uint32> group_ids;
  std::vector<std::vector<uint32>> mc_node_ids_per_group;
  RETURN_IF_ERROR(tdi_sde_interface_->GetMulticastGroups(
      device_, session, entry.multicast_group_id(), &group_ids,
      &mc_node_ids_per_group));

  // Build response.
  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < group_ids.size(); ++i) {
    const uint32 group_id = group_ids[i];
    const std::vector<uint32>& mc_node_ids = mc_node_ids_per_group[i];

    ::p4::v1::MulticastGroupEntry result;
    result.set_multicast_group_id(group_id);

    // Read egress ports from all multicast nodes and build replica list.
    for (const auto& mc_node_id : mc_node_ids) {
      int replication_id;
      std::vector<uint32> lag_ids;
      std::vector<uint32> ports;
      RETURN_IF_ERROR(tdi_sde_interface_->GetMulticastNode(
          device_, session, mc_node_id, &replication_id, &lag_ids, &ports));
      for (const auto& port : ports) {
        ::p4::v1::Replica replica;
        replica.set_egress_port(port);
        replica.set_instance(replication_id);
        *result.add_replicas() = replica;
      }
    }
    // Sort replicas by instance and port.
    std::sort(result.mutable_replicas()->begin(),
              result.mutable_replicas()->end(),
              [](::p4::v1::Replica a, ::p4::v1::Replica b) {
                if (a.instance() < b.instance()) {
                  return true;
                }
                if (a.instance() == b.instance() &&
                    a.egress_port() < b.egress_port()) {
                  return true;
                }
                return false;
              });
    LOG(INFO) << "MulticastGroupEntry " << result.ShortDebugString();
    *resp.add_entities()
         ->mutable_packet_replication_engine_entry()
         ->mutable_multicast_group_entry() = result;
  }
  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

::util::Status TdiPreManager::WriteCloneSessionEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::Update::Type& type,
    const ::p4::v1::CloneSessionEntry& entry) {
  RET_CHECK(entry.session_id() != 0 &&
                        entry.session_id() <= kMaxCloneSessionId)
      << "Invalid session id in CloneSessionEntry " << entry.ShortDebugString()
      << ".";
  RET_CHECK(entry.packet_length_bytes() <= UINT16_MAX)
      << "Packet length exceeds maximum value: " << entry.ShortDebugString()
      << ".";

  switch (type) {
    case ::p4::v1::Update::INSERT: {
      RET_CHECK(entry.replicas_size() == 1)
          << "Multiple replicas are not supported: " << entry.ShortDebugString()
          << ".";
      RET_CHECK(entry.class_of_service() < 8)
          << "Class of service must be smaller than 8: "
          << entry.ShortDebugString() << ".";
      const auto& replica = entry.replicas(0);
      RET_CHECK(replica.egress_port() != 0)
          << "Invalid egress port in Replica " << replica.ShortDebugString()
          << ".";
      RET_CHECK(replica.instance() == 0)
          << "Instances on Replicas are not supported: "
          << replica.ShortDebugString() << ".";
      RETURN_IF_ERROR(tdi_sde_interface_->InsertCloneSession(
          device_, session, entry.session_id(), replica.egress_port(),
          entry.class_of_service(), entry.packet_length_bytes()));
      break;
    }
    case ::p4::v1::Update::MODIFY: {
      RET_CHECK(entry.replicas_size() == 1)
          << "Multiple replicas are not supported: " << entry.ShortDebugString()
          << ".";
      const auto& replica = entry.replicas(0);
      RET_CHECK(replica.egress_port() != 0)
          << "Invalid egress port in Replica " << replica.ShortDebugString()
          << ".";
      RET_CHECK(replica.instance() == 0)
          << "Instances on Replicas are not supported: "
          << replica.ShortDebugString() << ".";
      RETURN_IF_ERROR(tdi_sde_interface_->ModifyCloneSession(
          device_, session, entry.session_id(), replica.egress_port(),
          entry.class_of_service(), entry.packet_length_bytes()));
      break;
    }
    case ::p4::v1::Update::DELETE: {
      RETURN_IF_ERROR(tdi_sde_interface_->DeleteCloneSession(
          device_, session, entry.session_id()));
      break;
    }
    default:
      return MAKE_ERROR(ERR_UNIMPLEMENTED)
          << "Unsupported update type: " << type << " on CloneSessionEntry "
          << entry.ShortDebugString() << ".";
  }

  return ::util::OkStatus();
}

::util::Status TdiPreManager::ReadCloneSessionEntry(
    std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const ::p4::v1::CloneSessionEntry& entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  std::vector<uint32> session_ids;
  std::vector<int> egress_ports;
  std::vector<int> coss;
  std::vector<int> max_pkt_lens;
  RETURN_IF_ERROR(tdi_sde_interface_->GetCloneSessions(
      device_, session, entry.session_id(), &session_ids, &egress_ports, &coss,
      &max_pkt_lens));

  // Build response.
  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < session_ids.size(); ++i) {
    const uint32 session_id = session_ids[i];
    const int egress_port = egress_ports[i];
    const int cos = coss[i];
    const int max_pkt_len = max_pkt_lens[i];

    ::p4::v1::CloneSessionEntry result;
    result.set_session_id(session_id);
    result.set_class_of_service(cos);
    result.set_packet_length_bytes(max_pkt_len);
    auto* replica = result.add_replicas();
    replica->set_egress_port(egress_port);
    replica->set_instance(0);

    LOG(INFO) << "CloneSessionEntry " << result.ShortDebugString();
    *resp.add_entities()
         ->mutable_packet_replication_engine_entry()
         ->mutable_clone_session_entry() = result;
  }

  if (!writer->Write(resp)) {
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream for failed.";
  }

  return ::util::OkStatus();
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
