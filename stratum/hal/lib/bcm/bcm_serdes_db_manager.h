// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BCM_BCM_SERDES_DB_MANAGER_H_
#define STRATUM_HAL_LIB_BCM_BCM_SERDES_DB_MANAGER_H_

#include <memory>

#include "absl/synchronization/mutex.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/common/common.pb.h"

namespace stratum {
namespace hal {
namespace bcm {

// The "BcmSerdesDbManager" class provides an interface for accessing serdes
// database for BCM-based Google switches.
class BcmSerdesDbManager {
 public:
  virtual ~BcmSerdesDbManager();

  // Loades bcm_serdes_db_ from file.
  virtual ::util::Status Load();

  // Looks up the serdes config for a given BCM port given its frontpanel port
  // info.
  virtual ::util::Status LookupSerdesConfigForPort(
      const BcmPort& bcm_port, const FrontPanelPortInfo& fp_port_info,
      BcmSerdesLaneConfig* bcm_serdes_lane_config) const;

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BcmSerdesDbManager> CreateInstance();

  // BcmSerdesDbManager is neither copyable nor movable.
  BcmSerdesDbManager(const BcmSerdesDbManager&) = delete;
  BcmSerdesDbManager& operator=(const BcmSerdesDbManager&) = delete;

 protected:
  // Default constructor.
  BcmSerdesDbManager();

 private:
  // A copy of the running version of the serdes DB, read from file.
  BcmSerdesDb bcm_serdes_db_;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_SERDES_DB_MANAGER_H_
