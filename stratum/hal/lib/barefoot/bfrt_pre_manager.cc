// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_pre_manager.h"

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/macros.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {
namespace barefoot {

BfrtPreManager::BfrtPreManager(const BfrtIdMapper* bfrt_id_mapper)
    : bfrt_id_mapper_(bfrt_id_mapper) {}

::util::Status BfrtPreManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config, const bfrt::BfRtInfo* bfrt_info) {
  absl::WriterMutexLock l(&lock_);
  bfrt_info_ = bfrt_info;
  return ::util::OkStatus();
}

::util::Status BfrtPreManager::WritePreEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type& type, const PreEntry& entry) {
  absl::WriterMutexLock l(&lock_);
  switch (entry.type_case()) {
    case PreEntry::kMulticastGroupEntry:
      return WriteMulticastGroupEntry(bfrt_session, type,
                                      entry.multicast_group_entry());
    case PreEntry::kCloneSessionEntry:
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED)
          << "Unsupported PRE entry: " << entry.ShortDebugString();
  }
}

::util::Status BfrtPreManager::WriteMulticastNodes(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type& type, ::p4::v1::MulticastGroupEntry entry) {
  // Find nodes that we already insatalled for this group, can be empty if we
  // never install this group.
  uint64 group_id = entry.multicast_group_id();
  absl::flat_hash_set<uint32> nodes_exist = {};
  absl::flat_hash_set<uint32> nodes_to_erase = {};
  if (gtl::ContainsKey(mcast_nodes_installed, group_id)) {
    nodes_exist = gtl::FindOrDie(mcast_nodes_installed, group_id);
  }

  // Collect instance -> ports mapping
  absl::flat_hash_map<uint32, std::vector<uint32>> replica_ports;
  for (const auto& replica : entry.replicas()) {
    replica_ports[replica.instance()].push_back(replica.egress_port());
  }
  const bfrt::BfRtTable* table;  // PRE node table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();

  for (const auto& replica : replica_ports) {
    uint32 node_id = replica.first;
    std::vector<uint32> egress_ports = replica.second;
    RETURN_IF_BFRT_ERROR(table->keyReset(table_key.get()));
    RETURN_IF_BFRT_ERROR(table->dataReset(table_data.get()));

    // $MULTICAST_NODE_ID
    bf_rt_id_t field_id;
    RETURN_IF_BFRT_ERROR(table->keyFieldIdGet(kMcNodeId, &field_id));
    RETURN_IF_BFRT_ERROR(
        table_key->setValue(field_id, static_cast<uint64>(node_id)));

    if (type == ::p4::v1::Update::INSERT || type == ::p4::v1::Update::MODIFY) {
      // $DEV_PORT
      RETURN_IF_BFRT_ERROR(table->dataFieldIdGet(kMcNodeDevPort, &field_id));
      RETURN_IF_BFRT_ERROR(table_data->setValue(field_id, egress_ports));
    }

    switch (type) {
      case ::p4::v1::Update::INSERT:
        RETURN_IF_BFRT_ERROR(table->tableEntryAdd(*bfrt_session, bf_dev_tgt,
                                                  *table_key, *table_data));
        nodes_exist.insert(node_id);
        break;
      case ::p4::v1::Update::MODIFY: {
        // If multicast node doesn't exist, we need to add it instead of
        // modify it.
        if (nodes_exist.find(node_id) == nodes_exist.end()) {
          RETURN_IF_BFRT_ERROR(table->tableEntryAdd(*bfrt_session, bf_dev_tgt,
                                                    *table_key, *table_data));
          nodes_exist.insert(node_id);
        } else {
          RETURN_IF_BFRT_ERROR(table->tableEntryMod(*bfrt_session, bf_dev_tgt,
                                                    *table_key, *table_data));
        }
        break;
      }
      case ::p4::v1::Update::DELETE:
        RETURN_IF_BFRT_ERROR(
            table->tableEntryDel(*bfrt_session, bf_dev_tgt, *table_key));
        nodes_to_erase.insert(node_id);
        break;
      default:
        RETURN_ERROR(ERR_UNIMPLEMENTED) << "Unsupported update type: " << type;
    }
  }

  // Remove unused nodes if we no longer need it. (Group modification only).
  if (type == ::p4::v1::Update::MODIFY) {
    for (const auto& node_id : nodes_exist) {
      if (gtl::ContainsKey(replica_ports, node_id)) {
        // Skip if we want to keep the node.
        continue;
      }
      RETURN_IF_BFRT_ERROR(table->keyReset(table_key.get()));

      // $MULTICAST_NODE_ID
      bf_rt_id_t field_id;
      RETURN_IF_BFRT_ERROR(table->keyFieldIdGet(kMcNodeId, &field_id));
      RETURN_IF_BFRT_ERROR(
          table_key->setValue(field_id, static_cast<uint64>(node_id)));
      RETURN_IF_BFRT_ERROR(
          table->tableEntryDel(*bfrt_session, bf_dev_tgt, *table_key));
      nodes_to_erase.insert(node_id);
    }
  }
  for (const auto& node_id : nodes_to_erase) {
    nodes_exist.erase(node_id);
  }

  // Update the node cache.
  mcast_nodes_installed[group_id] = nodes_exist;
  return ::util::OkStatus();
}

::util::Status BfrtPreManager::WriteMulticastGroup(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type& type, ::p4::v1::MulticastGroupEntry entry) {
  // Collect instances(node) ID
  absl::flat_hash_set<uint32> replica_instances;
  for (const auto& replica : entry.replicas()) {
    replica_instances.insert(replica.instance());
  }

  const bfrt::BfRtTable* table;  // PRE MGID table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  // Match key: $MGID
  bf_rt_id_t field_id;
  RETURN_IF_BFRT_ERROR(table->keyFieldIdGet(kMgid, &field_id));
  RETURN_IF_BFRT_ERROR(table_key->setValue(
      field_id, static_cast<uint64>(entry.multicast_group_id())));

  std::vector<uint32> mc_node_list;
  std::vector<bool> l1_xid_valid_list;
  std::vector<uint32> l1_xid_list;
  for (const auto& replica : replica_instances) {
    mc_node_list.push_back(replica);
    // TODO(Yi): P4Runtime doesn't support XID, set invalid for now.
    l1_xid_valid_list.push_back(false);
    l1_xid_list.push_back(0);
  }

  // Data: $MULTICAST_NODE_ID
  RETURN_IF_BFRT_ERROR(table->dataFieldIdGet(kMcNodeId, &field_id));
  RETURN_IF_BFRT_ERROR(table_data->setValue(field_id, mc_node_list));

  // Data: $MULTICAST_NODE_L1_XID_VALID
  RETURN_IF_BFRT_ERROR(table->dataFieldIdGet(kMcNodeL1XidValid, &field_id));
  RETURN_IF_BFRT_ERROR(table_data->setValue(field_id, l1_xid_valid_list));

  // Data: $MULTICAST_NODE_L1_XID
  RETURN_IF_BFRT_ERROR(table->dataFieldIdGet(kMcNodeL1Xid, &field_id));
  RETURN_IF_BFRT_ERROR(table_data->setValue(field_id, l1_xid_list));

  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  switch (type) {
    case ::p4::v1::Update::INSERT:
      RETURN_IF_BFRT_ERROR(table->tableEntryAdd(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data));
      break;
    case ::p4::v1::Update::MODIFY:
      RETURN_IF_BFRT_ERROR(table->tableEntryMod(*bfrt_session, bf_dev_tgt,
                                                *table_key, *table_data));
      break;
    case ::p4::v1::Update::DELETE:
      RETURN_IF_BFRT_ERROR(
          table->tableEntryDel(*bfrt_session, bf_dev_tgt, *table_key));
      break;
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED) << "Unsupported update type: " << type;
  }
  return ::util::OkStatus();
}

::util::Status BfrtPreManager::WriteMulticastGroupEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type& type, ::p4::v1::MulticastGroupEntry entry) {
  // Need to insert/modify/delete in a specific order
  switch (type) {
    case ::p4::v1::Update::INSERT:
    case ::p4::v1::Update::MODIFY:
      RETURN_IF_ERROR(WriteMulticastNodes(bfrt_session, type, entry));
      RETURN_IF_ERROR(WriteMulticastGroup(bfrt_session, type, entry));
      break;
    case ::p4::v1::Update::DELETE:
      RETURN_IF_ERROR(WriteMulticastGroup(bfrt_session, type, entry));
      RETURN_IF_ERROR(WriteMulticastNodes(bfrt_session, type, entry));
      break;
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED) << "Unsupported update type: " << type;
  }
  return ::util::OkStatus();
}

::util::StatusOr<PreEntry> BfrtPreManager::ReadPreEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, const PreEntry& entry) {
  absl::ReaderMutexLock l(&lock_);
  PreEntry result;
  switch (entry.type_case()) {
    case PreEntry::kMulticastGroupEntry: {
      ASSIGN_OR_RETURN(
          auto mc_grp_entr,
          ReadMulticastGroupEntry(bfrt_session, entry.multicast_group_entry()));
      result.mutable_multicast_group_entry()->CopyFrom(mc_grp_entr);
      break;
    }
    case PreEntry::kCloneSessionEntry:
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED)
          << "Unsupported PRE entry: " << entry.ShortDebugString();
  }
  return result;
}

::util::StatusOr<std::vector<uint32>> BfrtPreManager::GetEgressPortsFromMcNode(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, uint64 mc_node_id) {
  std::vector<uint32> result;
  const bfrt::BfRtTable* table;  // PRE node table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  // $MLUTICAST_NODE_ID
  bf_rt_id_t field_id;
  RETURN_IF_BFRT_ERROR(table->keyFieldIdGet(kMcNodeId, &field_id));
  RETURN_IF_BFRT_ERROR(
      table_key->setValue(field_id, static_cast<uint64>(mc_node_id)));

  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));

  // $DEV_PORT
  RETURN_IF_BFRT_ERROR(table->dataFieldIdGet(kMcNodeDevPort, &field_id));
  RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &result));

  return result;
}

::util::StatusOr<::p4::v1::MulticastGroupEntry>
BfrtPreManager::ReadMulticastGroupEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    ::p4::v1::MulticastGroupEntry entry) {
  ::p4::v1::MulticastGroupEntry result;
  result.set_multicast_group_id(entry.multicast_group_id());
  const bfrt::BfRtTable* table;  // PRE MGID table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  // Match key: $MGID
  bf_rt_id_t field_id;
  RETURN_IF_BFRT_ERROR(table->keyFieldIdGet(kMgid, &field_id));
  RETURN_IF_BFRT_ERROR(table_key->setValue(
      field_id, static_cast<uint64>(entry.multicast_group_id())));
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));

  // Data: $MULTICAST_NODE_ID
  std::vector<uint32> mc_node_list;
  RETURN_IF_BFRT_ERROR(table->dataFieldIdGet(kMcNodeId, &field_id));
  RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &mc_node_list));

  // Read egress ports from all multicast ports and build replica list
  for (const auto& mc_node_id : mc_node_list) {
    ASSIGN_OR_RETURN(auto egress_ports,
                     GetEgressPortsFromMcNode(bfrt_session, mc_node_id));
    for (const auto& egress_port : egress_ports) {
      auto* replica = result.add_replicas();
      replica->set_instance(mc_node_id);
      replica->set_egress_port(egress_port);
    }
  }

  return result;
}

std::unique_ptr<BfrtPreManager> BfrtPreManager::CreateInstance(
    const BfrtIdMapper* bfrt_id_mapper) {
  return absl::WrapUnique(new BfrtPreManager(bfrt_id_mapper));
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
