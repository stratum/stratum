// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "bf_rt/bf_rt_init.hpp"
#include "bf_rt/bf_rt_session.hpp"
#include "bf_rt/bf_rt_table_key.hpp"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bfrt_id_mapper.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfRtTableManager {
 public:
  // Writes a table entry.
  ::util::Status WriteTableEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::TableEntry& table_entry);

  // Reads a table entry
  ::util::StatusOr<::p4::v1::TableEntry> ReadTableEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::TableEntry& table_entry);

  // Creates a table manager instance for a specific unit.
  static std::unique_ptr<BfRtTableManager> CreateInstance(
      int unit, const BfRtIdMapper* bfrt_id_mapper);

 private:
  ::util::Status BuildTableKey(const ::p4::v1::TableEntry& table_entry,
                               bfrt::BfRtTableKey* table_key);

  ::util::Status BuildTableActionData(const ::p4::v1::Action& action,
                                      const bfrt::BfRtTable* table,
                                      bfrt::BfRtTableData* table_data);

  ::util::Status BuildTableActionProfileMemberData(
      const uint32_t action_profile_member_id, const bfrt::BfRtTable* table,
      bfrt::BfRtTableData* table_data);

  ::util::Status BuildTableActionProfileGroupData(
      const uint32_t action_profile_group_id, const bfrt::BfRtTable* table,
      bfrt::BfRtTableData* table_data);

  ::util::Status BuildTableData(const ::p4::v1::TableEntry table_entry,
                                const bfrt::BfRtTable* table,
                                bfrt::BfRtTableData* table_data);

  // Private constructure, we can create the instance by using `CreateInstance`
  // function only.
  BfRtTableManager(int unit, const BfRtIdMapper* bfrt_id_mapper);

  // The unit number, which represent the device ID in SDK level.
  int unit_;

  // The BfRt info, requires by some function to get runtime
  // instances like tables.
  const bfrt::BfRtInfo* bfrt_info_;

  // Reader-writer lock used to protect access to pipeline state.
  mutable absl::Mutex lock_;

  // The ID mapper that maps P4Runtime ID to BfRt ones (vice versa).
  // Not owned by this class
  const BfRtIdMapper* bfrt_id_mapper_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
