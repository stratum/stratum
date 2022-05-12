// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BCM_BCM_SERDES_DB_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_BCM_BCM_SERDES_DB_MANAGER_MOCK_H_

#include "gmock/gmock.h"
#include "stratum/hal/lib/bcm/bcm_serdes_db_manager.h"

namespace stratum {
namespace hal {
namespace bcm {

class BcmSerdesDbManagerMock : public BcmSerdesDbManager {
 public:
  MOCK_METHOD0(Load, ::util::Status());
  MOCK_CONST_METHOD3(
      LookupSerdesConfigForPort,
      ::util::Status(const BcmPort& bcm_port,
                     const FrontPanelPortInfo& fp_port_info,
                     BcmSerdesLaneConfig* bcm_serdes_lane_config));
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_SERDES_DB_MANAGER_MOCK_H_
