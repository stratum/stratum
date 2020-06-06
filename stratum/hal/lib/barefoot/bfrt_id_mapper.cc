// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_id_mapper.h"

#include <vector>

#include "absl/strings/match.h"
#include "stratum/hal/lib/barefoot/macros.h"

namespace stratum {
namespace hal {
namespace barefoot {

::util::Status BfRtIdMapper::PushPipelineInfo(
    const p4::config::v1::P4Info& p4info, const bfrt::BfRtInfo* bfrt_info) {
  absl::WriterMutexLock l(&lock_);
  RETURN_IF_ERROR(BuildP4InfoAndBfrtInfoMapping(p4info, bfrt_info));
  return ::util::OkStatus();
}

::util::Status BfRtIdMapper::BuildMapping(uint32_t p4info_id,
                                          std::string p4info_name,
                                          const bfrt::BfRtInfo* bfrt_info) {
  const bfrt::BfRtTable* table;
  auto bf_status = bfrt_info->bfrtTableFromIdGet(p4info_id, &table);

  if (bf_status == BF_SUCCESS) {
    // Both p4info and bfrt json uses the same id for a specific
    // table/action selector/profile
    p4info_to_bfrt_id_[p4info_id] = p4info_id;
    bfrt_to_p4info_id_[p4info_id] = p4info_id;
    return ::util::OkStatus();
  }
  // Unable to find table by id, because bfrt uses a different id, we
  // can try to search it by name.
  bf_status = bfrt_info->bfrtTableFromNameGet(p4info_name, &table);
  if (bf_status == BF_SUCCESS) {
    // Table can be found with the given name, but they uses different IDs
    // We need to store mapping so we can map them later.
    bf_rt_id_t bfrt_table_id;
    table->tableIdGet(&bfrt_table_id);
    p4info_to_bfrt_id_[p4info_id] = bfrt_table_id;
    bfrt_to_p4info_id_[bfrt_table_id] = p4info_id;
    return ::util::OkStatus();
  }

  // Special case: bfrt includes pipeline name as prefix(e.g., "pipe."), but
  // p4info doesn't. We need to scan all tables to see if there is a table
  // called "[pipeline name].[P4 info table name]"
  std::vector<const bfrt::BfRtTable*> bfrt_tables;
  BFRT_RETURN_IF_ERROR(bfrt_info->bfrtInfoGetTables(&bfrt_tables));
  for (auto* bfrt_table : bfrt_tables) {
    bf_rt_id_t bfrt_table_id;
    std::string bfrt_table_name;
    bfrt_table->tableIdGet(&bfrt_table_id);
    bfrt_table->tableNameGet(&bfrt_table_name);
    if (absl::StrContains(bfrt_table_name, p4info_name)) {
      p4info_to_bfrt_id_[p4info_id] = bfrt_table_id;
      bfrt_to_p4info_id_[bfrt_table_id] = p4info_id;
      return ::util::OkStatus();
    }
  }
  RETURN_ERROR() << "Unable to find " << p4info_name << " from bfrt info.";
}

// Builds mapping between p4info and bfrt info
// In most case, such as table id, we don't really need to map
// from p4info ID to bfrt ID.
// However for some cases, like externs which does not exists
// in native P4 core headers, the frontend compiler will
// generate different IDs between p4info and bfrt info.
::util::Status BfRtIdMapper::BuildP4InfoAndBfrtInfoMapping(
    const p4::config::v1::P4Info& p4info, const bfrt::BfRtInfo* bfrt_info) {
  // Try to find P4 tables from BFRT info
  for (const auto& table : p4info.tables()) {
    RETURN_IF_ERROR(BuildMapping(table.preamble().id(), table.preamble().name(),
                                 bfrt_info));
  }

  // Action profiles
  for (const auto& action_profile : p4info.action_profiles()) {
    RETURN_IF_ERROR(BuildMapping(action_profile.preamble().id(),
                                 action_profile.preamble().name(), bfrt_info));
  }
  return ::util::OkStatus();
}

::util::StatusOr<bf_rt_target_t> BfRtIdMapper::GetDeviceTarget(
    bf_rt_id_t bfrt_id) const {
  bf_rt_target_t dev_tgt;
  dev_tgt.dev_id = unit_;
  dev_tgt.pipe_id = BF_DEV_PIPE_ALL;
  return dev_tgt;
}

::util::StatusOr<uint32_t> BfRtIdMapper::GetBfRtId(uint32_t p4info_id) const {
  absl::ReaderMutexLock l(&lock_);
  auto it = p4info_to_bfrt_id_.find(p4info_id);
  CHECK_RETURN_IF_FALSE(it != p4info_to_bfrt_id_.end())
      << "Unable to find bfrt id form p4info id: " << p4info_id;
  return it->second;
}

::util::StatusOr<uint32_t> BfRtIdMapper::GetP4InfoId(bf_rt_id_t bfrt_id) const {
  absl::ReaderMutexLock l(&lock_);
  auto it = bfrt_to_p4info_id_.find(bfrt_id);
  CHECK_RETURN_IF_FALSE(it != bfrt_to_p4info_id_.end())
      << "Unable to find p4info id form bfrt id: " << bfrt_id;
  return it->second;
}

std::unique_ptr<BfRtIdMapper> BfRtIdMapper::CreateInstance(int unit) {
  return absl::WrapUnique(new BfRtIdMapper(unit));
}

BfRtIdMapper::BfRtIdMapper(int unit) : unit_(unit) {}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
