// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_ID_MAPPER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_ID_MAPPER_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "bf_rt/bf_rt_init.hpp"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace barefoot {

// A helper class that convert IDs between P4Runtime and BfRt.
class BfRtIdMapper {
 public:
  // Initialize pipeline information
  // This function creates a mapping between P4Info and BfRt
  ::util::Status PushPipelineInfo(const p4::config::v1::P4Info& p4info,
                                  const bfrt::BfRtInfo* bfrt_info)
      LOCKS_EXCLUDED(lock_);

  // Gets the device target(device id + pipe id) for a specific BfRt
  // primitive(e.g. table)
  // FIXME: Now we only return the device target with pipe "BF_DEV_PIPE_ALL"
  ::util::StatusOr<bf_rt_target_t> GetDeviceTarget(bf_rt_id_t bfrt_id) const;

  // Maps a P4Info ID to a BfRt ID
  ::util::StatusOr<uint32_t> GetBfRtId(uint32_t p4info_id) const;

  // Maps a BfRt ID to a P4Info ID
  ::util::StatusOr<uint32_t> GetP4InfoId(bf_rt_id_t bfrt_id) const;

  // Creates a table manager instance for a specific unit.
  static std::unique_ptr<BfRtIdMapper> CreateInstance(int unit);

 private:
  // Private constructure, we can create the instance by using `CreateInstance`
  // function only.
  explicit BfRtIdMapper(int unit);

  ::util::Status BuildMapping(uint32_t p4info_id, std::string p4info_name,
                              const bfrt::BfRtInfo* bfrt_info)
      SHARED_LOCKS_REQUIRED(lock_);

  ::util::Status BuildP4InfoAndBfrtInfoMapping(
      const p4::config::v1::P4Info& p4info, const bfrt::BfRtInfo* bfrt_info)
      SHARED_LOCKS_REQUIRED(lock_);

  // Reader-writer lock used to protect access to mapping.
  mutable absl::Mutex lock_;

  // The unit(device) number for this mapper.
  const int unit_;

  // Mappings
  absl::flat_hash_map<bf_rt_id_t, uint32_t> bfrt_to_p4info_id_
      GUARDED_BY(lock_);
  absl::flat_hash_map<uint32_t, bf_rt_id_t> p4info_to_bfrt_id_
      GUARDED_BY(lock_);
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_ID_MAPPER_H_
