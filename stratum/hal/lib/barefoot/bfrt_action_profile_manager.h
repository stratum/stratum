// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_ACT_PROF_MANAGER_H
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_ACT_PROF_MANAGER_H

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

namespace stratum {
namespace hal {
namespace barefoot {

class BfRtActionProfileManager {
 public:
  // Pushes the pipline info.
  ::util::Status PushPipelineInfo(const p4::config::v1::P4Info& p4info,
                                  const bfrt::BfRtInfo* bfrt_info)
      LOCKS_EXCLUDED(lock_);
  // Writes an action profile member
  ::util::Status WriteActionProfileEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ExternEntry& action_profile_entry) LOCKS_EXCLUDED(lock_);

  // Reads an action profile entry
  ::util::StatusOr<::p4::v1::ExternEntry> ReadActionProfileEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::ExternEntry& action_profile_entry) LOCKS_EXCLUDED(lock_);

  // Writes an action profile member
  ::util::Status WriteActionProfileMember(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileMember& action_profile_member)
      LOCKS_EXCLUDED(lock_);

  // Reads an action profile member
  ::util::StatusOr<::p4::v1::ActionProfileMember> ReadActionProfileMember(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::ActionProfileMember& action_profile_member)
      LOCKS_EXCLUDED(lock_);

  // Writes an action profile group
  ::util::Status WriteActionProfileGroup(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileGroup& action_profile_group)
      LOCKS_EXCLUDED(lock_);

  // Reads an action profile Group
  ::util::StatusOr<::p4::v1::ActionProfileGroup> ReadActionProfileGroup(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::ActionProfileGroup& action_profile_group)
      LOCKS_EXCLUDED(lock_);

  // Creates an action profile manager instance for a specific unit.
  static std::unique_ptr<BfRtActionProfileManager> CreateInstance(
      int unit, const BfRtIdMapper* bfrt_id_mapper);

 private:
  // Writes an action profile member
  ::util::Status WriteActionProfileMember(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileMember& action_profile_member)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Reads an action profile member
  ::util::StatusOr<::p4::v1::ActionProfileMember> ReadActionProfileMember(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
      const ::p4::v1::ActionProfileMember& action_profile_member)
      SHARED_LOCKS_REQUIRED(lock_);

  // Writes an action profile group
  ::util::Status WriteActionProfileGroup(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileGroup& action_profile_group)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Reads an action profile Group
  ::util::StatusOr<::p4::v1::ActionProfileGroup> ReadActionProfileGroup(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session, bf_rt_id_t bfrt_table_id,
      const ::p4::v1::ActionProfileGroup& action_profile_group)
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
  ::util::Status BuildData(
      const bfrt::BfRtTable* table,
      const ::p4::v1::ActionProfileMember& action_profile_member,
      bfrt::BfRtTableData* table_data);

  // Builds data for an action profile group (ActionSelector entry in TNA).
  // The data contains three part:
  // $ACTION_MEMBER_ID: An std::vector<bf_rt_id_t> of action profile member IDs.
  // $ACTION_MEMBER_STATUS: An std::vector<bool> of action profile member.
  // status, which makes member activate or not.
  // $MAX_GROUP_SIZE: uint64, the max size of this group.
  ::util::Status BuildData(
      const bfrt::BfRtTable* table,
      const ::p4::v1::ActionProfileGroup& action_profile_group,
      bfrt::BfRtTableData* table_data);

  // Private constructure, we can create the instance by using `CreateInstance`
  // function only.
  BfRtActionProfileManager(int unit, const BfRtIdMapper* bfrt_id_mapper);

  // The BfRt info, requires by some function to get runtime
  // instances like tables.
  const bfrt::BfRtInfo* bfrt_info_ GUARDED_BY(lock_);

  // Reader-writer lock used to protect access to pipeline state.
  mutable absl::Mutex lock_;

  // The ID mapper that maps P4Runtime ID to BfRt ones (vice versa).
  // Not owned by this class
  const BfRtIdMapper* bfrt_id_mapper_;

  // The unit number, which represent the device ID in SDK level.
  const int unit_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_ACT_PROF_MANAGER_H
