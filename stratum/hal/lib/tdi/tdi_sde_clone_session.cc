// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Target-agnostic SDE wrapper for CloneSession methods.

#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"

#include <algorithm>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/tdi/macros.h"
#include "stratum/hal/lib/tdi/tdi_constants.h"
#include "stratum/hal/lib/tdi/tdi_sde_common.h"
#include "stratum/hal/lib/tdi/tdi_sde_helpers.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace tdi {

using namespace stratum::hal::tdi::helpers;

::util::Status TdiSdeWrapper::WriteCloneSession(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 session_id, int egress_port, int cos, int max_pkt_len, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const ::tdi::Table* table;
  const ::tdi::Device *device = nullptr;
  const ::tdi::DataFieldInfo *dataFieldInfo;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(
      tdi_info_->tableFromNameGet(kMirrorConfigTable, &table));
  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  tdi_id_t action_id;
  dataFieldInfo = table->tableInfoGet()->dataFieldGet("$normal");
  RETURN_IF_NULL(dataFieldInfo);
  action_id = dataFieldInfo->idGet();
  RETURN_IF_TDI_ERROR(table->dataAllocate(action_id, &table_data));

  // Key: $sid
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), "$sid", session_id));
  // Data: $direction
  RETURN_IF_ERROR(SetField(table_data.get(), "$direction", "BOTH"));
  // Data: $session_enable
  RETURN_IF_ERROR(SetFieldBool(table_data.get(), "$session_enable", true));
  // Data: $ucast_egress_port
  RETURN_IF_ERROR(
      SetField(table_data.get(), "$ucast_egress_port", egress_port));
  // Data: $ucast_egress_port_valid
  RETURN_IF_ERROR(
      SetFieldBool(table_data.get(), "$ucast_egress_port_valid", true));
  // Data: $ingress_cos
  RETURN_IF_ERROR(SetField(table_data.get(), "$ingress_cos", cos));
  // Data: $max_pkt_len
  RETURN_IF_ERROR(SetField(table_data.get(), "$max_pkt_len", max_pkt_len));

  if (insert) {
    RETURN_IF_TDI_ERROR(table->entryAdd(
        *real_session->tdi_session_, *dev_tgt, *flags, *table_key, *table_data));
  } else {
    RETURN_IF_TDI_ERROR(table->entryMod(
        *real_session->tdi_session_, *dev_tgt, *flags, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::InsertCloneSession(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 session_id, int egress_port, int cos, int max_pkt_len) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteCloneSession(dev_id, session, session_id, egress_port, cos,
                           max_pkt_len, true);

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::ModifyCloneSession(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 session_id, int egress_port, int cos, int max_pkt_len) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteCloneSession(dev_id, session, session_id, egress_port, cos,
                           max_pkt_len, false);

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::DeleteCloneSession(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 session_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  const ::tdi::DataFieldInfo *dataFieldInfo;
  CHECK_RETURN_IF_FALSE(real_session);

  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(
      tdi_info_->tableFromNameGet(kMirrorConfigTable, &table));
  std::unique_ptr<::tdi::TableKey> table_key;
  std::unique_ptr<::tdi::TableData> table_data;
  RETURN_IF_TDI_ERROR(table->keyAllocate(&table_key));
  tdi_id_t action_id;
  dataFieldInfo = table->tableInfoGet()->dataFieldGet("$normal");
  RETURN_IF_NULL(dataFieldInfo);
  action_id = dataFieldInfo->idGet();
  RETURN_IF_TDI_ERROR(table->dataAllocate(action_id, &table_data));
  // Key: $sid
  RETURN_IF_ERROR(SetFieldExact(table_key.get(), "$sid", session_id));

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  RETURN_IF_TDI_ERROR(table->entryDel(*real_session->tdi_session_,
                                      *dev_tgt, *flags, *table_key));

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetCloneSessions(
    int dev_id, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
    uint32 session_id, std::vector<uint32>* session_ids,
    std::vector<int>* egress_ports, std::vector<int>* coss,
    std::vector<int>* max_pkt_lens) {
  CHECK_RETURN_IF_FALSE(session_ids);
  CHECK_RETURN_IF_FALSE(egress_ports);
  CHECK_RETURN_IF_FALSE(coss);
  CHECK_RETURN_IF_FALSE(max_pkt_lens);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  const ::tdi::DataFieldInfo *dataFieldInfo;
  CHECK_RETURN_IF_FALSE(real_session);

  const ::tdi::Device *device = nullptr;
  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  std::unique_ptr<::tdi::Target> dev_tgt;
  device->createTarget(&dev_tgt);

  ::tdi::Flags *flags = new ::tdi::Flags(0);
  const ::tdi::Table* table;
  RETURN_IF_TDI_ERROR(
      tdi_info_->tableFromNameGet(kMirrorConfigTable, &table));
  tdi_id_t action_id;
  dataFieldInfo = table->tableInfoGet()->dataFieldGet("$normal");
  RETURN_IF_NULL(dataFieldInfo);
  action_id = dataFieldInfo->idGet();
  std::vector<std::unique_ptr<::tdi::TableKey>> keys;
  std::vector<std::unique_ptr<::tdi::TableData>> datums;
  // Is this a wildcard read?
  if (session_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_TDI_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_TDI_ERROR(table->dataAllocate(action_id, &datums[0]));
    // Key: $sid
    RETURN_IF_ERROR(SetFieldExact(keys[0].get(), "$sid", session_id));
    RETURN_IF_TDI_ERROR(table->entryGet(
        *real_session->tdi_session_, *dev_tgt, *flags, *keys[0],
        datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->tdi_session_, *dev_tgt,
                                  table, &keys, &datums));
  }

  session_ids->resize(0);
  egress_ports->resize(0);
  coss->resize(0);
  max_pkt_lens->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<::tdi::TableData>& table_data = datums[i];
    const std::unique_ptr<::tdi::TableKey>& table_key = keys[i];
    // Key: $sid
    uint32_t session_id = 0;
    RETURN_IF_ERROR(GetFieldExact(*table_key, "$sid", &session_id));
    session_ids->push_back(session_id);
    // Data: $ingress_cos
    uint64 ingress_cos;
    RETURN_IF_ERROR(GetField(*table_data, "$ingress_cos", &ingress_cos));
    coss->push_back(ingress_cos);
    // Data: $max_pkt_len
    uint64 pkt_len;
    RETURN_IF_ERROR(GetField(*table_data, "$max_pkt_len", &pkt_len));
    max_pkt_lens->push_back(pkt_len);
    // Data: $ucast_egress_port
    uint64 port;
    RETURN_IF_ERROR(GetField(*table_data, "$ucast_egress_port", &port));
    egress_ports->push_back(port);
    // Data: $session_enable
    bool session_enable;
    RETURN_IF_ERROR(GetFieldBool(*table_data, "$session_enable", &session_enable));
    CHECK_RETURN_IF_FALSE(session_enable)
        << "Found a session that is not enabled.";
    // Data: $ucast_egress_port_valid
    bool ucast_egress_port_valid;
    RETURN_IF_ERROR(GetFieldBool(*table_data, "$ucast_egress_port_valid",
                             &ucast_egress_port_valid));
    CHECK_RETURN_IF_FALSE(ucast_egress_port_valid)
        << "Found a unicast egress port that is not set valid.";
  }

  CHECK_EQ(session_ids->size(), keys.size());
  CHECK_EQ(egress_ports->size(), keys.size());
  CHECK_EQ(coss->size(), keys.size());
  CHECK_EQ(max_pkt_lens->size(), keys.size());

  return ::util::OkStatus();
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
