// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_ID_MAPPER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_ID_MAPPER_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "bf_rt/bf_rt_info.hpp"
#include "bf_rt/bf_rt_init.hpp"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bf.pb.h"
#include "stratum/hal/lib/barefoot/bf_sde_interface.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace barefoot {

// A helper class that convert IDs between P4Runtime and BfRt.
class BfrtIdMapper {
 public:
  // Initialize pipeline information
  // This function creates a mapping between P4Info and BfRt
  ::util::Status PushForwardingPipelineConfig(const BfrtDeviceConfig& config,
                                              const bfrt::BfRtInfo* bfrt_info)
      LOCKS_EXCLUDED(lock_);

  // Maps a P4Info ID to a BfRt ID
  ::util::StatusOr<uint32> GetBfRtId(uint32 p4info_id) const
      LOCKS_EXCLUDED(lock_);

  // Maps a BfRt ID to a P4Info ID
  ::util::StatusOr<uint32> GetP4InfoId(bf_rt_id_t bfrt_id) const
      LOCKS_EXCLUDED(lock_);

  // Gets the action selector ID of an action profile.
  ::util::StatusOr<bf_rt_id_t> GetActionSelectorBfRtId(
      bf_rt_id_t action_profile_id) const LOCKS_EXCLUDED(lock_);

  // Gets the action profile ID of an action selector.
  ::util::StatusOr<bf_rt_id_t> GetActionProfileBfRtId(
      bf_rt_id_t action_selector_id) const LOCKS_EXCLUDED(lock_);

  // Creates a table manager instance for a specific device.
  static std::unique_ptr<BfrtIdMapper> CreateInstance();

 private:
  // Private constructor, we can create the instance by using `CreateInstance`
  // function only.
  BfrtIdMapper();

  ::util::Status BuildMapping(uint32 p4info_id, std::string p4info_name,
                              const bfrt::BfRtInfo* bfrt_info)
      SHARED_LOCKS_REQUIRED(lock_);

  // Scan context.json file and build mappings for ActionProfile and
  // ActionSelector.
  // FIXME(Yi): We may want to remove this workaround if we use the P4 externs
  // in the future.
  ::util::Status BuildActionProfileMapping(
      const p4::config::v1::P4Info& p4info, const bfrt::BfRtInfo* bfrt_info,
      const std::string& context_json_content) SHARED_LOCKS_REQUIRED(lock_);

  // Reader-writer lock used to protect access to mapping.
  mutable absl::Mutex lock_;

  // Maps from bfrt ID to P4Runtime ID and vice versa.
  absl::flat_hash_map<bf_rt_id_t, uint32> bfrt_to_p4info_id_ GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, bf_rt_id_t> p4info_to_bfrt_id_ GUARDED_BY(lock_);

  // Map for getting an ActionSelector BfRt ID from an ActionProfile BfRt ID.
  absl::flat_hash_map<bf_rt_id_t, bf_rt_id_t> act_profile_to_selector_mapping_
      GUARDED_BY(lock_);

  // Map for getting an ActionProfile BfRt ID from an ActionSelector BfRt ID.
  absl::flat_hash_map<bf_rt_id_t, bf_rt_id_t> act_selector_to_profile_mapping_
      GUARDED_BY(lock_);
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_ID_MAPPER_H_
