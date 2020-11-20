// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_ACTION_PROFILE_MANAGER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_ACTION_PROFILE_MANAGER_H_

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
#include "stratum/hal/lib/barefoot/bf.pb.h"
#include "stratum/hal/lib/barefoot/bf_sde_interface.h"
#include "stratum/hal/lib/barefoot/bfrt_id_mapper.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtActionProfileManager {
 public:
  // Pushes the pipline info.
  ::util::Status PushForwardingPipelineConfig(const BfrtDeviceConfig& config,
                                              const bfrt::BfRtInfo* bfrt_info)
      LOCKS_EXCLUDED(lock_);

  // Writes an action profile member.
  ::util::Status WriteActionProfileEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ExternEntry& action_profile_entry) LOCKS_EXCLUDED(lock_);

  // Writes an action profile member.
  ::util::Status WriteActionProfileMember(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileMember& action_profile_member)
      LOCKS_EXCLUDED(lock_);

  // Writes an action profile group.
  ::util::Status WriteActionProfileGroup(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileGroup& action_profile_group)
      LOCKS_EXCLUDED(lock_);

  // Reads the P4 ActionProfileEntry(s) matched by the given extern entry.
  ::util::Status ReadActionProfileEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::ExternEntry& action_profile_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Reads the P4 ActionProfileMember(s) matched by the given entry.
  ::util::Status ReadActionProfileMember(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::ActionProfileMember& action_profile_member,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Reads the P4 ActionProfileGroup(s) matched by the given entry.
  ::util::Status ReadActionProfileGroup(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::ActionProfileGroup& action_profile_group,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Creates an action profile manager instance.
  static std::unique_ptr<BfrtActionProfileManager> CreateInstance(
      const BfrtIdMapper* bfrt_id_mapper, BfSdeInterface* bf_sde_interface);

 private:
  // Private constructor, we can create the instance by using `CreateInstance`
  // function only.
  explicit BfrtActionProfileManager(const BfrtIdMapper* bfrt_id_mapper,
                                    BfSdeInterface* bf_sde_interface);

  // Internal version of WriteActionProfileMember which takes no locks.
  ::util::Status DoWriteActionProfileMember(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileMember& action_profile_member)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Internal version of WriteActionProfileGroup which takes no locks.
  ::util::Status DoWriteActionProfileGroup(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileGroup& action_profile_group)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Internal version of ReadActionProfileMember which takes no locks.
  ::util::Status DoReadActionProfileMember(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
      const ::p4::v1::ActionProfileMember& action_profile_member,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  // Internal version of ReadActionProfileGroup which takes no locks.
  ::util::Status DoReadActionProfileGroup(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
      const ::p4::v1::ActionProfileGroup& action_profile_group,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  // Builds key for an action profile member (ActionProfile entry in TNA).
  // The key contains one field "$ACTION_MEMBER_ID"
  ::util::Status BuildTableKey(
      const bfrt::BfRtTable* table,
      const ::p4::v1::ActionProfileMember& action_profile_member,
      bfrt::BfRtTableKey* table_key);

  // Builds key for an action profile group (ActionSelector entry in TNA).
  // The key contains one field "$SELECTOR_GROUP_ID"
  ::util::Status BuildTableKey(
      const bfrt::BfRtTable* table,
      const ::p4::v1::ActionProfileGroup& action_profile_group,
      bfrt::BfRtTableKey* table_key);

  // Builds data for an action profile member (ActionProfile entry in TNA).
  // The data contains the action ID and action parameters (like a normal
  // entry).
  ::util::Status BuildTableData(
      const bfrt::BfRtTable* table,
      const ::p4::v1::ActionProfileMember& action_profile_member,
      bfrt::BfRtTableData* table_data);

  // Builds data for an action profile group (ActionSelector entry in TNA).
  // The data contains three part:
  // $ACTION_MEMBER_ID: An std::vector<bf_rt_id_t> of action profile member IDs.
  // $ACTION_MEMBER_STATUS: An std::vector<bool> of action profile member.
  // status, which makes member activate or not.
  // $MAX_GROUP_SIZE: uint64, the max size of this group.
  ::util::Status BuildTableData(
      const bfrt::BfRtTable* table,
      const ::p4::v1::ActionProfileGroup& action_profile_group,
      bfrt::BfRtTableData* table_data);

  ::util::StatusOr<::p4::v1::ActionProfileMember> BuildP4ActionProfileMember(
      const bfrt::BfRtTable* table, const bfrt::BfRtTableKey& table_key,
      const bfrt::BfRtTableData& table_data);

  ::util::StatusOr<::p4::v1::ActionProfileGroup> BuildP4ActionProfileGroup(
      const bfrt::BfRtTable* table, const bfrt::BfRtTableKey& table_key,
      const bfrt::BfRtTableData& table_data);

  // Reader-writer lock used to protect access to pipeline state.
  mutable absl::Mutex lock_;

  // The BfRt info, requires by some function to get runtime
  // instances like tables.
  const bfrt::BfRtInfo* bfrt_info_ GUARDED_BY(lock_);

  // Pointer to a BfSdeInterface implementation that wraps all the SDE calls.
  BfSdeInterface* bf_sde_interface_ = nullptr;  // not owned by this class.

  // The ID mapper that maps P4Runtime ID to BfRt ones (vice versa).
  // Not owned by this class
  const BfrtIdMapper* bfrt_id_mapper_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_ACTION_PROFILE_MANAGER_H_
