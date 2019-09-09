/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef STRATUM_HAL_LIB_BCM_BCM_SERDES_DB_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_BCM_BCM_SERDES_DB_MANAGER_MOCK_H_

#include "stratum/hal/lib/bcm/bcm_serdes_db_manager.h"
#include "gmock/gmock.h"

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
