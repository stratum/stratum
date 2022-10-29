// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Target-agnostic SDE wrapper for Multicast methods.

#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/tdi/macros.h"
#include "stratum/hal/lib/tdi/tdi_constants.h"
#include "stratum/hal/lib/tdi/tdi_sde_helpers.h"
#include "stratum/lib/macros.h"
#include "stratum/public/proto/error.pb.h"

namespace stratum {
namespace hal {
namespace tdi {

using namespace stratum::hal::tdi::helpers;

namespace {

::util::Status PrintMcGroupEntry(const ::tdi::Table* table,
                                 const ::tdi::TableKey* table_key,
                                 const ::tdi::TableData* table_data) {
  std::vector<uint32> mc_node_list;
  std::vector<bool> l1_xid_valid_list;
  std::vector<uint32> l1_xid_list;

  // Key: $MGID
  uint32_t multicast_group_id = 0;
  RETURN_IF_ERROR(GetFieldExact(*table_key, kMgid, &multicast_group_id));
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

::util::Status PrintMcNodeEntry(const ::tdi::Table* table,
                                const ::tdi::TableKey* table_key,
                                const ::tdi::TableData* table_data) {
  // Key: $MULTICAST_NODE_ID (24 bit)
  uint32_t node_id = 0;
  RETURN_IF_ERROR(GetFieldExact(*table_key, kMcNodeId, &node_id));
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

::util::Status TdiSdeWrapper::DumpPreState(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session) {
  if (VLOG_IS_ON(2)) {
    auto real_session = std::dynamic_pointer_cast<Session>(session);
    RET_CHECK(real_session);

    const ::tdi::Table* table;
    const ::tdi::Device *device = nullptr;
    ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
    std::unique_ptr<::tdi::Target> dev_tgt;
    device->createTarget(&dev_tgt);

    // Dump group table
    LOG(INFO) << "#### $pre.mgid ####";
    RETURN_IF_TDI_ERROR(
        tdi_info_->tableFromNameGet(kPreMgidTable, &table));
    std::vector<std::unique_ptr<::tdi::TableKey>> keys;
    std::vector<std::unique_ptr<::tdi::TableData>> datums;
    RETURN_IF_TDI_ERROR(
        tdi_info_->tableFromNameGet(kPreMgidTable, &table));
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt,
                                  table, &keys, &datums));
    for (size_t i = 0; i < keys.size(); ++i) {
      const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
      const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
      PrintMcGroupEntry(table, table_key.get(), table_data.get());
    }
    LOG(INFO) << "###################";

    // Dump node table
    LOG(INFO) << "#### $pre.node ####";
    RETURN_IF_TDI_ERROR(
        tdi_info_->tableFromNameGet(kPreNodeTable, &table));
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt,
                                  table, &keys, &datums));
    for (size_t i = 0; i < keys.size(); ++i) {
      const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
      const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
      PrintMcNodeEntry(table, table_key.get(), table_data.get());
    }
    LOG(INFO) << "###################";
  }
  return ::util::OkStatus();
}

::util::StatusOr<uint32> TdiSdeWrapper::GetFreeMulticastNodeId(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);

  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreNodeTable, &table));
  size_t table_size;

  RETURN_IF_TDI_ERROR(table->sizeGet(*real_session->tdi_session_,
                                      *dev_tgt, *flags, &table_size));
  uint32 usage;
  RETURN_IF_TDI_ERROR(table->usageGet(
      *real_session->tdi_session_, *dev_tgt,
      *flags, &usage));
  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));
  uint32 id = usage;
  for (size_t _ = 0; _ < table_size; ++_) {
    // Key: $MULTICAST_NODE_ID
    RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMcNodeId, id));
    bf_status_t status;
    status = table->entryGet(
        *real_session->tdi_session_, *dev_tgt, *flags, *table_key,
        table_data.get());
    if (status == BF_OBJECT_NOT_FOUND) {
      return id;
    } else if (status == BF_SUCCESS) {
      id++;
      continue;
    } else {
      RETURN_IF_TDI_ERROR(status);
    }
  }

  return MAKE_ERROR(ERR_TABLE_FULL) << "Could not find free multicast node id.";
}

::util::StatusOr<uint32> TdiSdeWrapper::CreateMulticastNode(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    int mc_replication_id, const std::vector<uint32>& mc_lag_ids,
    const std::vector<uint32>& ports) {
  ::absl::ReaderMutexLock l(&data_lock_);

  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Table* table;  // PRE node table.
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreNodeTable, &table));
  auto table_id = table->tableInfoGet()->idGet();

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);

  ASSIGN_OR_RETURN(uint64 mc_node_id, GetFreeMulticastNodeId(dev_id, session));

  // Key: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMcNodeId, mc_node_id));
  // Data: $MULTICAST_RID (16 bit)
  RETURN_IF_ERROR(
      SetField(table_data.get(), kMcReplicationId, mc_replication_id));
  // Data: $MULTICAST_LAG_ID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeLagId, mc_lag_ids));
  // Data: $DEV_PORT
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeDevPort, ports));
  RETURN_IF_TDI_ERROR(table->entryAdd(
      *real_session->tdi_session_, *dev_tgt, *flags, *table_key, *table_data));
  return mc_node_id;
}

::util::StatusOr<std::vector<uint32>> TdiSdeWrapper::GetNodesInMulticastGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 group_id) {
  ::absl::ReaderMutexLock l(&data_lock_);

  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreMgidTable, &table));

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));
  // Key: $MGID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMgid, group_id));
  RETURN_IF_TDI_ERROR(table->entryGet(
      *real_session->tdi_session_,*dev_tgt, *flags, *table_key,
      table_data.get()));
  // Data: $MULTICAST_NODE_ID
  std::vector<uint32> mc_node_list;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeId, &mc_node_list));
  return mc_node_list;

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::DeleteMulticastNodes(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    const std::vector<uint32>& mc_node_ids) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreNodeTable, &table));
  auto table_id = table->tableInfoGet()->idGet();

  // TODO(max): handle partial delete failures
  for (const auto& mc_node_id : mc_node_ids) {
    std::unique_ptr<::tdi::TableKey> table_key;
    RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
    RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMcNodeId, mc_node_id));
    RETURN_IF_TDI_ERROR(table->entryDel(*real_session->tdi_session_,
                                         *dev_tgt, *flags, *table_key));
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetMulticastNode(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 mc_node_id, int* replication_id, std::vector<uint32>* lag_ids,
    std::vector<uint32>* ports) {
  RET_CHECK(replication_id);
  RET_CHECK(lag_ids);
  RET_CHECK(ports);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;  // PRE node table.
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreNodeTable, &table));
  auto table_id = table->tableInfoGet()->idGet();

  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));
  // Key: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMcNodeId, mc_node_id));
  RETURN_IF_TDI_ERROR(table->entryGet(
      *real_session->tdi_session_, *dev_tgt, *flags, *table_key,
      table_data.get()));
  // Data: $DEV_PORT
  std::vector<uint32> dev_ports;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeDevPort, &dev_ports));
  *ports = dev_ports;
  // Data: $RID (16 bit)
  uint64 rid;
  RETURN_IF_ERROR(GetField(*table_data, kMcReplicationId, &rid));
  *replication_id = rid;
  // Data: $MULTICAST_LAG_ID
  std::vector<uint32> lags;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeLagId, &lags));
  *lag_ids = lags;

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::WriteMulticastGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 group_id, const std::vector<uint32>& mc_node_ids, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);


  ::tdi::Flags *flags = new ::tdi::Flags(0);
  const ::tdi::Table* table = nullptr;  // PRE MGID table.
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreMgidTable, &table));
  auto table_id = table->tableInfoGet()->idGet();
  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_TDI_ERROR(table->dataAllocate(&table_data));

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
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMgid, group_id));
  // Data: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeId, mc_node_list));
  // Data: $MULTICAST_NODE_L1_XID_VALID
  RETURN_IF_ERROR(
      SetField(table_data.get(), kMcNodeL1XidValid, l1_xid_valid_list));
  // Data: $MULTICAST_NODE_L1_XID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeL1Xid, l1_xid_list));

  if (insert) {
    RETURN_IF_TDI_ERROR(table->entryAdd(
        *real_session->tdi_session_, *dev_tgt, *flags, *table_key, *table_data));

  } else {
    RETURN_IF_TDI_ERROR(table->entryMod(
        *real_session->tdi_session_, *dev_tgt, *flags, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::InsertMulticastGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 group_id, const std::vector<uint32>& mc_node_ids) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteMulticastGroup(dev_id, session, group_id, mc_node_ids, true);

  return ::util::OkStatus();
}
::util::Status TdiSdeWrapper::ModifyMulticastGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 group_id, const std::vector<uint32>& mc_node_ids) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteMulticastGroup(dev_id, session, group_id, mc_node_ids, false);

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::DeleteMulticastGroup(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 group_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;  // PRE MGID table.
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreMgidTable, &table));
  std::unique_ptr<::tdi::TableKey> table_key;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  // Key: $MGID
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), kMgid, group_id));
  RETURN_IF_TDI_ERROR(table->entryDel(*real_session->tdi_session_,
                                            *dev_tgt, *flags, *table_key));

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetMulticastGroups(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 group_id, std::vector<uint32>* group_ids,
    std::vector<std::vector<uint32>>* mc_node_ids) {
  RET_CHECK(group_ids);
  RET_CHECK(mc_node_ids);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  RET_CHECK(real_session);

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;  // PRE MGID table.
  RETURN_IF_TDI_ERROR(tdi_info_->tableFromNameGet(kPreMgidTable, &table));
  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;
  // Is this a wildcard read?
  if (group_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(&datums[0]));
    // Key: $MGID
    RETURN_IF_ERROR(SetFieldExact(keys[0].get(), kMgid, group_id));
    RETURN_IF_TDI_ERROR(table->entryGet(
        *real_session->tdi_session_, *dev_tgt, *flags, *keys[0],
        datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt,
                                  table, &keys, &datums));
  }

  group_ids->resize(0);
  mc_node_ids->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
    const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
    ::p4::v1::MulticastGroupEntry result;
    // Key: $MGID
    uint32_t group_id = 0;
    RETURN_IF_ERROR(GetFieldExact(*table_key, kMgid, &group_id));
    group_ids->push_back(group_id);
    // Data: $MULTICAST_NODE_ID
    std::vector<uint32> mc_node_list;
    RETURN_IF_ERROR(GetField(*table_data, kMcNodeId, &mc_node_list));
    mc_node_ids->push_back(mc_node_list);
  }

  CHECK_EQ(group_ids->size(), keys.size());
  CHECK_EQ(mc_node_ids->size(), keys.size());

  return ::util::OkStatus();
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
