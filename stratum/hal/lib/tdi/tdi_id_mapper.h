// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_TDI_ID_MAPPER_H_
#define STRATUM_HAL_LIB_TDI_TDI_ID_MAPPER_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/tdi/tdi.pb.h"
#include "stratum/hal/lib/tdi/tdi_sde_interface.h"
#include "stratum/lib/macros.h"
#include "tdi/common/tdi_info.hpp"
#include "tdi/common/tdi_init.hpp"

namespace stratum {
namespace hal {
namespace tdi {

// A helper class that convert IDs between P4Runtime and TDI.
class TdiIdMapper {
 public:
  // Initialize pipeline information
  // This function creates a mapping between P4Info and TDI
  ::util::Status PushForwardingPipelineConfig(const TdiDeviceConfig& config,
                                              const ::tdi::TdiInfo* tdi_info)
      LOCKS_EXCLUDED(lock_);

  // Maps a P4Info ID to a TDI ID
  ::util::StatusOr<uint32> GetTdiRtId(uint32 p4info_id) const
      LOCKS_EXCLUDED(lock_);

  // Maps a TDI ID to a P4Info ID
  ::util::StatusOr<uint32> GetP4InfoId(tdi_id_t tdi_id) const
      LOCKS_EXCLUDED(lock_);

  // Gets the action selector ID of an action profile.
  ::util::StatusOr<tdi_id_t> GetActionSelectorTdiRtId(
      tdi_id_t action_profile_id) const LOCKS_EXCLUDED(lock_);

  // Gets the action profile ID of an action selector.
  ::util::StatusOr<tdi_id_t> GetActionProfileTdiRtId(
      tdi_id_t action_selector_id) const LOCKS_EXCLUDED(lock_);

  // Creates a table manager instance for a specific device.
  static std::unique_ptr<TdiIdMapper> CreateInstance();

 private:
  // Private constructor, we can create the instance by using `CreateInstance`
  // function only.
  TdiIdMapper();

  ::util::Status BuildMapping(uint32 p4info_id, std::string p4info_name,
                              const ::tdi::TdiInfo* tdi_info)
      SHARED_LOCKS_REQUIRED(lock_);

  // Scan context.json file and build mappings for ActionProfile and
  // ActionSelector.
  // FIXME(Yi): We may want to remove this workaround if we use the P4 externs
  // in the future.
  ::util::Status BuildActionProfileMapping(
      const p4::config::v1::P4Info& p4info, const ::tdi::TdiInfo* tdi_info,
      const std::string& context_json_content) SHARED_LOCKS_REQUIRED(lock_);

  // Reader-writer lock used to protect access to mapping.
  mutable absl::Mutex lock_;

  // Maps from TDI ID to P4Runtime ID and vice versa.
  absl::flat_hash_map<tdi_id_t, uint32> tdi_to_p4info_id_ GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, tdi_id_t> p4info_to_tdi_id_ GUARDED_BY(lock_);

  // Map for getting an ActionSelector TDI ID from an ActionProfile TDI ID.
  absl::flat_hash_map<tdi_id_t, tdi_id_t> act_profile_to_selector_mapping_
      GUARDED_BY(lock_);

  // Map for getting an ActionProfile TDI ID from an ActionSelector TDI ID.
  absl::flat_hash_map<tdi_id_t, tdi_id_t> act_selector_to_profile_mapping_
      GUARDED_BY(lock_);
};

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_ID_MAPPER_H_
