// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "bf_rt/bf_rt_init.hpp"
#include "bf_rt/bf_rt_table_key.hpp"
#include "bf_rt/bf_rt_session.hpp"

#include "absl/synchronization/mutex.h"
#include "absl/container/flat_hash_map.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BFRuntimeTableManager {

 public:
  ::util::Status PushPipelineInfo(const p4::config::v1::P4Info& p4info,
                                  bfrt::BfRtInfo* bfrt_info,
                                  bf_rt_target_t dev_tgt);
  ::util::Status WriteTableEntry(std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type,
    const ::p4::v1::TableEntry& table_entry);
  ::util::StatusOr<::p4::v1::TableEntry> ReadTableEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::TableEntry& table_entry);

  static std::unique_ptr<BFRuntimeTableManager> CreateInstance(int unit);

 private:
  ::util::Status BuildMapping(uint32_t p4info_id,
                              std::string p4info_name,
                              bfrt::BfRtInfo* bfrt_info);

  ::util::Status BuildP4InfoAndBfrtInfoMapping(const p4::config::v1::P4Info& p4info,
                                               bfrt::BfRtInfo* bfrt_info);

  ::util::Status BuildTableKey(const ::p4::v1::TableEntry& table_entry,
                               bfrt::BfRtTableKey *table_key);

  ::util::Status BuildTableActionData(const ::p4::v1::Action& action,
                                      const bfrt::BfRtTable *table,
                                      bfrt::BfRtTableData* table_data);

  ::util::Status BuildTableActionProfileMemberData(const uint32_t action_profile_member_id,
                                                   const bfrt::BfRtTable *table,
                                                   bfrt::BfRtTableData* table_data);

  ::util::Status BuildTableActionProfileGroupData(const uint32_t action_profile_group_id,
                                                  const bfrt::BfRtTable *table,
                                                  bfrt::BfRtTableData* table_data);

  ::util::Status BuildTableData(const ::p4::v1::TableEntry table_entry,
                                const bfrt::BfRtTable *table,
                                bfrt::BfRtTableData* table_data);

  ::util::StatusOr<uint32_t> GetBfRtId(uint32_t p4info_id);
  ::util::StatusOr<uint32_t> GetP4InfoId(bf_rt_id_t bfrt_id);

  BFRuntimeTableManager(int unit);
  int unit_;
  bfrt::BfRtInfo* bfrt_info_;

  // Reader-writer lock used to protect access to pipeline state.
  mutable absl::Mutex lock_;

  bool initialized_;

  absl::flat_hash_map<bf_rt_id_t, uint32_t>bfrt_to_p4info_id_;
  absl::flat_hash_map<uint32_t, bf_rt_id_t>p4info_to_bfrt_id_;

  bf_rt_target_t dev_tgt_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
