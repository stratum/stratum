// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_pre_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/macros.h"
#include "stratum/hal/lib/barefoot/utils.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {
namespace barefoot {

BfrtPreManager::BfrtPreManager(const BfrtIdMapper* bfrt_id_mapper)
    : bfrt_info_(nullptr), bfrt_id_mapper_(bfrt_id_mapper) {}

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
      return WriteCloneSessionEntry(bfrt_session, type,
                                    entry.clone_session_entry());
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED)
          << "Unsupported PRE entry: " << entry.ShortDebugString();
  }
}

::util::Status BfrtPreManager::ReadPreEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, const PreEntry& entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  absl::ReaderMutexLock l(&lock_);
  switch (entry.type_case()) {
    case PreEntry::kMulticastGroupEntry: {
      RETURN_IF_ERROR(ReadMulticastGroupEntry(
          bfrt_session, entry.multicast_group_entry(), writer));
      break;
    }
    case PreEntry::kCloneSessionEntry: {
      RETURN_IF_ERROR(ReadCloneSessionEntry(
          bfrt_session, entry.clone_session_entry(), writer));
      break;
    }
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED)
          << "Unsupported PRE entry: " << entry.ShortDebugString();
  }

  return ::util::OkStatus();
}

std::unique_ptr<BfrtPreManager> BfrtPreManager::CreateInstance(
    const BfrtIdMapper* bfrt_id_mapper) {
  return absl::WrapUnique(new BfrtPreManager(bfrt_id_mapper));
}

namespace {
::util::Status PrintMcGroupEntry(const bfrt::BfRtTable* table,
                                 const bfrt::BfRtTableKey* table_key,
                                 const bfrt::BfRtTableData* table_data) {
  std::vector<uint32> mc_node_list;
  std::vector<bool> l1_xid_valid_list;
  std::vector<uint32> l1_xid_list;
  uint64 multicast_group_id;

  // Key: $MGID
  RETURN_IF_ERROR(GetField(*table_key, kMgid, &multicast_group_id));
  // Data: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeId, &mc_node_list));
  // Data: $MULTICAST_NODE_L1_XID_VALID
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeL1XidValid, &l1_xid_valid_list));
  // Data: $MULTICAST_NODE_L1_XID
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeL1Xid, &l1_xid_list));

  LOG(INFO) << "Multicast group id " << multicast_group_id << " has "
            << mc_node_list.size() << " nodes.";
  for (const auto& node : mc_node_list) {
    LOG(INFO) << "\tnode id " << node;
  }

  return ::util::OkStatus();
}

::util::Status PrintMcNodeEntry(const bfrt::BfRtTable* table,
                                const bfrt::BfRtTableKey* table_key,
                                const bfrt::BfRtTableData* table_data) {
  // Key: $MULTICAST_NODE_ID (24 bit)
  uint64 node_id;
  RETURN_IF_ERROR(GetField(*table_key, kMcNodeId, &node_id));
  // Data: $MULTICAST_RID (16 bit)
  uint64 rid;
  RETURN_IF_ERROR(GetField(*table_data, kMcReplicationId, &rid));
  // Data: $DEV_PORT
  std::vector<uint32> ports;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeDevPort, &ports));

  std::string ports_str = " ports [ ";
  for (const auto& port : ports) {
    ports_str += std::to_string(port) + " ";
  }
  ports_str += "]";
  LOG(INFO) << "Node id " << node_id << ": rid " << rid << ports_str;

  return ::util::OkStatus();
}
}  // namespace

::util::Status BfrtPreManager::DumpHwState(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session) {
  if (VLOG_IS_ON(2)) {
    auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
    const bfrt::BfRtTable* table;

    // Dump group table
    LOG(INFO) << "#### $pre.mgid ####";
    RETURN_IF_BFRT_ERROR(
        bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
    std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
    std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
    RETURN_IF_BFRT_ERROR(
        bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
    RETURN_IF_ERROR(
        GetAllEntries(bfrt_session, bf_dev_tgt, table, &keys, &datums));
    for (size_t i = 0; i < keys.size(); ++i) {
      const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
      const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
      PrintMcGroupEntry(table, table_key.get(), table_data.get());
    }
    LOG(INFO) << "###################";

    // Dump node table
    LOG(INFO) << "#### $pre.node ####";
    RETURN_IF_BFRT_ERROR(
        bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
    RETURN_IF_ERROR(
        GetAllEntries(bfrt_session, bf_dev_tgt, table, &keys, &datums));
    for (size_t i = 0; i < keys.size(); ++i) {
      const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
      const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
      PrintMcNodeEntry(table, table_key.get(), table_data.get());
    }
    LOG(INFO) << "###################";
  }
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<uint32>> BfrtPreManager::GetNodesInMulticastGroup(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, uint32 group_id) {
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  // Key: $MGID
  RETURN_IF_ERROR(SetField(table_key.get(), kMgid, group_id));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));
  // Data: $MULTICAST_NODE_ID
  std::vector<uint32> mc_node_list;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeId, &mc_node_list));

  return mc_node_list;
}

::util::StatusOr<uint32> BfrtPreManager::GetFreeMulticastNodeId(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session) {
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  size_t table_size;
  RETURN_IF_BFRT_ERROR(table->tableSizeGet(&table_size));
  uint32 usage;
  RETURN_IF_BFRT_ERROR(table->tableUsageGet(
      *bfrt_session, bf_dev_tgt, bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW,
      &usage));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  uint32 id = usage;
  for (size_t _ = 0; _ < table_size; ++_) {
    // Key: $MULTICAST_NODE_ID
    RETURN_IF_ERROR(SetField(table_key.get(), kMcNodeId, id));
    bf_status_t status = table->tableEntryGet(
        *bfrt_session, bf_dev_tgt, *table_key,
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get());
    if (status == BF_OBJECT_NOT_FOUND) {
      return id;
    } else if (status == BF_SUCCESS) {
      id++;
      continue;
    } else {
      RETURN_IF_BFRT_ERROR(status);
    }
  }

  RETURN_ERROR(ERR_TABLE_FULL) << "Could not find free multicast node id.";
}

::util::StatusOr<std::vector<uint32>> BfrtPreManager::InsertMulticastNodes(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::MulticastGroupEntry& entry) {
  const uint32 group_id = entry.multicast_group_id();
  CHECK_RETURN_IF_FALSE(group_id <= kMaxMulticastGroupId);

  // Collect instance (rid) -> egress ports mapping
  absl::flat_hash_map<uint32, std::vector<uint32>> instance_to_egress_ports;
  for (const auto& replica : entry.replicas()) {
    CHECK_RETURN_IF_FALSE(replica.instance() <= UINT16_MAX);
    instance_to_egress_ports[replica.instance()].push_back(
        replica.egress_port());
  }
  std::vector<uint32> new_nodes = {};
  const bfrt::BfRtTable* table;  // PRE node table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();

  for (const auto& replica : instance_to_egress_ports) {
    const uint32 instance = replica.first;
    ASSIGN_OR_RETURN(uint64 mc_node_id, GetFreeMulticastNodeId(bfrt_session));
    // uint64 mc_node_id = createMcNodeId(group_id, instance);
    std::vector<uint32> egress_ports = replica.second;
    RETURN_IF_BFRT_ERROR(table->keyReset(table_key.get()));
    RETURN_IF_BFRT_ERROR(table->dataReset(table_data.get()));

    // Key: $MULTICAST_NODE_ID
    RETURN_IF_ERROR(SetField(table_key.get(), kMcNodeId, mc_node_id));
    // Data: $MULTICAST_RID (16 bit)
    RETURN_IF_ERROR(SetField(table_data.get(), kMcReplicationId, instance));
    // Data: $DEV_PORT
    RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeDevPort, egress_ports));

    RETURN_IF_BFRT_ERROR(table->tableEntryAdd(*bfrt_session, bf_dev_tgt,
                                              *table_key, *table_data));
    new_nodes.push_back(mc_node_id);
  }

  DumpHwState(bfrt_session);

  return new_nodes;
}

::util::Status BfrtPreManager::DeleteMulticastNodes(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const std::vector<uint32>& mc_node_ids) {
  DumpHwState(bfrt_session);

  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();

  const bfrt::BfRtTable* table;
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));

  for (const auto& mc_node_id : mc_node_ids) {
    std::unique_ptr<bfrt::BfRtTableKey> table_key;
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
    RETURN_IF_ERROR(SetField(table_key.get(), kMcNodeId, mc_node_id));
    RETURN_IF_BFRT_ERROR(
        table->tableEntryDel(*bfrt_session, bf_dev_tgt, *table_key));
  }

  DumpHwState(bfrt_session);

  return ::util::OkStatus();
}

::util::Status BfrtPreManager::DeleteMulticastGroup(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, uint32 group_id) {
  DumpHwState(bfrt_session);

  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  const bfrt::BfRtTable* table;  // PRE MGID table.
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  // Key: $MGID
  RETURN_IF_ERROR(SetField(table_key.get(), kMgid, group_id));
  RETURN_IF_BFRT_ERROR(
      table->tableEntryDel(*bfrt_session, bf_dev_tgt, *table_key));

  DumpHwState(bfrt_session);

  return ::util::OkStatus();
}

::util::Status BfrtPreManager::WriteMulticastGroup(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type& type, uint32 group_id,
    std::vector<uint32> mc_node_ids) {
  DumpHwState(bfrt_session);
  const bfrt::BfRtTable* table;  // PRE MGID table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  std::vector<uint32> mc_node_list;
  std::vector<bool> l1_xid_valid_list;
  std::vector<uint32> l1_xid_list;
  for (const auto& mc_node_id : mc_node_ids) {
    mc_node_list.push_back(mc_node_id);
    // TODO(Yi): P4Runtime doesn't support XID, set invalid for now.
    l1_xid_valid_list.push_back(false);
    l1_xid_list.push_back(0);
  }
  // Key: $MGID
  RETURN_IF_ERROR(SetField(table_key.get(), kMgid, group_id));
  // Data: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeId, mc_node_list));
  // Data: $MULTICAST_NODE_L1_XID_VALID
  RETURN_IF_ERROR(
      SetField(table_data.get(), kMcNodeL1XidValid, l1_xid_valid_list));
  // Data: $MULTICAST_NODE_L1_XID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeL1Xid, l1_xid_list));

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
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED) << "Unsupported update type: " << type;
  }

  DumpHwState(bfrt_session);

  return ::util::OkStatus();
}

::util::Status BfrtPreManager::WriteMulticastGroupEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type& type,
    const ::p4::v1::MulticastGroupEntry& entry) {
  VLOG(1) << ::p4::v1::Update_Type_Name(type) << " "
          << entry.ShortDebugString();
  ::util::Status status;
  switch (type) {
    case ::p4::v1::Update::INSERT: {
      ASSIGN_OR_RETURN(std::vector<uint32> mc_node_ids,
                       InsertMulticastNodes(bfrt_session, entry));
      RETURN_IF_ERROR(WriteMulticastGroup(
          bfrt_session, type, entry.multicast_group_id(), mc_node_ids));
      break;
    }
    case ::p4::v1::Update::MODIFY: {
      ASSIGN_OR_RETURN(
          auto current_node_ids,
          GetNodesInMulticastGroup(bfrt_session, entry.multicast_group_id()));
      ASSIGN_OR_RETURN(std::vector<uint32> new_node_ids,
                       InsertMulticastNodes(bfrt_session, entry));
      RETURN_IF_ERROR_WITH_APPEND(
          WriteMulticastGroup(bfrt_session, type, entry.multicast_group_id(),
                              new_node_ids))
              .with_logging()
          << "Failed to write multicast group for request "
          << entry.ShortDebugString() << ".";
      RETURN_IF_ERROR_WITH_APPEND(
          DeleteMulticastNodes(bfrt_session, current_node_ids))
              .with_logging()
          << "Failed to delete multicast nodes for request "
          << entry.ShortDebugString() << ".";
      break;
    }
    case ::p4::v1::Update::DELETE: {
      LOG_IF(WARNING, entry.replicas_size() != 0)
          << "Replicas are ignored on MulticastGroupEntry delete requests: "
          << entry.ShortDebugString() << ".";
      ASSIGN_OR_RETURN(
          auto node_ids,
          GetNodesInMulticastGroup(bfrt_session, entry.multicast_group_id()));
      RETURN_IF_ERROR_WITH_APPEND(
          DeleteMulticastGroup(bfrt_session, entry.multicast_group_id()))
              .with_logging()
          << "Failed to delete multicast group for request "
          << entry.ShortDebugString() << ".";
      RETURN_IF_ERROR_WITH_APPEND(DeleteMulticastNodes(bfrt_session, node_ids))
              .with_logging()
          << "Failed to delete multicast nodes for request "
          << entry.ShortDebugString() << ".";
      break;
    }
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED) << "Unsupported update type: " << type;
  }
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<::p4::v1::Replica>>
BfrtPreManager::GetReplicasFromMcNode(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session, uint64 mc_node_id) {
  const bfrt::BfRtTable* table;  // PRE node table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  // Key: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetField(table_key.get(), kMcNodeId, mc_node_id));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *bfrt_session, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));
  // Data: $DEV_PORT
  std::vector<uint32> ports;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeDevPort, &ports));
  // Data: $RID (16 bit)
  uint64 rid;
  RETURN_IF_ERROR(GetField(*table_data, kMcReplicationId, &rid));
  std::vector<::p4::v1::Replica> result;
  for (const auto& port : ports) {
    ::p4::v1::Replica replica;
    replica.set_egress_port(port);
    replica.set_instance(rid);
    result.push_back(replica);
  }

  return result;
}

::util::Status BfrtPreManager::ReadMulticastGroupEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::MulticastGroupEntry& entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  const bfrt::BfRtTable* table;  // PRE MGID table.
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  // Is this a wildcard read?
  if (entry.multicast_group_id()) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&datums[0]));
    // Key: $MGID
    RETURN_IF_ERROR(SetField(keys[0].get(), kMgid, entry.multicast_group_id()));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *bfrt_session, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(
        GetAllEntries(bfrt_session, bf_dev_tgt, table, &keys, &datums));
  }

  // Build response.
  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    ::p4::v1::MulticastGroupEntry result;
    // Key: $MGID
    uint64 group_id;
    RETURN_IF_ERROR(GetField(*table_key, kMgid, &group_id));
    result.set_multicast_group_id(group_id);
    // Data: $MULTICAST_NODE_ID
    std::vector<uint32> mc_node_list;
    RETURN_IF_ERROR(GetField(*table_data, kMcNodeId, &mc_node_list));

    // Read egress ports from all multicast nodes and build replica list.
    for (const auto& mc_node_id : mc_node_list) {
      ASSIGN_OR_RETURN(auto replicas,
                       GetReplicasFromMcNode(bfrt_session, mc_node_id));
      for (const auto& replica : replicas) {
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

::util::Status BfrtPreManager::WriteCloneSessionEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type& type,
    const ::p4::v1::CloneSessionEntry& entry) {
  CHECK_RETURN_IF_FALSE(entry.session_id() != 0 &&
                        entry.session_id() <= kMaxCloneSessionId)
      << "Invalid session id in CloneSessionEntry " << entry.ShortDebugString()
      << ".";
  CHECK_RETURN_IF_FALSE(entry.packet_length_bytes() <= UINT16_MAX)
      << "Packet length exceeds maximum value: " << entry.ShortDebugString()
      << ".";

  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet("$mirror.cfg", &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  bf_rt_id_t action_id;
  RETURN_IF_BFRT_ERROR(table->actionIdGet("$normal", &action_id));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(action_id, &table_data));

  switch (type) {
    case ::p4::v1::Update::MODIFY:
    case ::p4::v1::Update::INSERT: {
      CHECK_RETURN_IF_FALSE(entry.replicas_size() == 1)
          << "Multiple replicas are not supported: " << entry.ShortDebugString()
          << ".";
      const auto& replica = entry.replicas(0);
      CHECK_RETURN_IF_FALSE(replica.egress_port() != 0)
          << "Invalid egress port in Replica " << replica.ShortDebugString()
          << ".";
      CHECK_RETURN_IF_FALSE(replica.instance() == 0)
          << "Instances on Replicas are not supported: "
          << replica.ShortDebugString() << ".";

      // Key: $sid
      RETURN_IF_ERROR(SetField(table_key.get(), "$sid", entry.session_id()));
      // Data: $direction
      RETURN_IF_ERROR(SetField(table_data.get(), "$direction", "BOTH"));
      // Data: $session_enable
      RETURN_IF_ERROR(SetFieldBool(table_data.get(), "$session_enable", true));
      // Data: $ucast_egress_port
      RETURN_IF_ERROR(SetField(table_data.get(), "$ucast_egress_port",
                               replica.egress_port()));
      // Data: $ucast_egress_port_valid
      RETURN_IF_ERROR(
          SetFieldBool(table_data.get(), "$ucast_egress_port_valid", true));
      // Data: $ingress_cos
      RETURN_IF_ERROR(
          SetField(table_data.get(), "$ingress_cos", entry.class_of_service()));
      // Data: $max_pkt_len
      RETURN_IF_ERROR(SetField(table_data.get(), "$max_pkt_len",
                               entry.packet_length_bytes()));
      switch (type) {
        case ::p4::v1::Update::INSERT:
          RETURN_IF_BFRT_ERROR(table->tableEntryAdd(*bfrt_session, bf_dev_tgt,
                                                    *table_key, *table_data));
          break;
        case ::p4::v1::Update::MODIFY:
          RETURN_IF_BFRT_ERROR(table->tableEntryMod(*bfrt_session, bf_dev_tgt,
                                                    *table_key, *table_data));
          break;
        default:
          RETURN_ERROR(ERR_UNIMPLEMENTED)
              << "Unsupported update type: " << type;
      }
      return ::util::OkStatus();
    }
    case ::p4::v1::Update::DELETE: {
      // Key: $sid
      RETURN_IF_ERROR(SetField(table_key.get(), "$sid", entry.session_id()));
      RETURN_IF_BFRT_ERROR(
          table->tableEntryDel(*bfrt_session, bf_dev_tgt, *table_key));

      return ::util::OkStatus();
    }
    default:
      RETURN_ERROR(ERR_UNIMPLEMENTED)
          << "Unsupported update type: " << type << " on CloneSessionEntry "
          << entry.ShortDebugString() << ".";
  }
}

::util::Status BfrtPreManager::ReadCloneSessionEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::CloneSessionEntry& entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  auto bf_dev_tgt = bfrt_id_mapper_->GetDeviceTarget();
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet("$mirror.cfg", &table));
  bf_rt_id_t action_id;
  RETURN_IF_BFRT_ERROR(table->actionIdGet("$normal", &action_id));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  // Is this a wildcard read?
  if (entry.session_id()) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(action_id, &datums[0]));
    // Key: $sid
    RETURN_IF_ERROR(SetField(keys[0].get(), "$sid", entry.session_id()));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *bfrt_session, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(
        GetAllEntries(bfrt_session, bf_dev_tgt, table, &keys, &datums));
  }

  // Build response.
  ::p4::v1::ReadResponse resp;
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    ::p4::v1::CloneSessionEntry result;
    // Key: $sid
    uint64 session_id;
    RETURN_IF_ERROR(GetField(*table_key, "$sid", &session_id));
    result.set_session_id(session_id);
    // Data: $ingress_cos
    uint64 ingress_cos;
    RETURN_IF_ERROR(GetField(*table_data, "$ingress_cos", &ingress_cos));
    result.set_class_of_service(ingress_cos);
    // Data: $max_pkt_len
    uint64 pkt_len;
    RETURN_IF_ERROR(GetField(*table_data, "$max_pkt_len", &pkt_len));
    result.set_packet_length_bytes(pkt_len);
    // Data: $session_enable
    bool session_enable;
    RETURN_IF_ERROR(GetField(*table_data, "$session_enable", &session_enable));
    CHECK_RETURN_IF_FALSE(session_enable)
        << "Found a session that is not enabled.";
    // Data: $ucast_egress_port_valid
    bool ucast_egress_port_valid;
    RETURN_IF_ERROR(GetField(*table_data, "$ucast_egress_port_valid",
                             &ucast_egress_port_valid));
    CHECK_RETURN_IF_FALSE(ucast_egress_port_valid)
        << "Found a unicase egress port that is not set valid.";

    auto* replica = result.add_replicas();
    // Data: $ucast_egress_port
    uint64 port;
    RETURN_IF_ERROR(GetField(*table_data, "$ucast_egress_port", &port));
    replica->set_egress_port(port);
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

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
