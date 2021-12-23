// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BCM_BCM_L2_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_BCM_BCM_L2_MANAGER_MOCK_H_

#include "gmock/gmock.h"
#include "stratum/hal/lib/bcm/bcm_l2_manager.h"

namespace stratum {
namespace hal {
namespace bcm {

class BcmL2ManagerMock : public BcmL2Manager {
 public:
  MOCK_METHOD2(PushChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD2(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_METHOD1(InsertMyStationEntry,
               ::util::Status(const BcmFlowEntry& bcm_flow_entry));
  MOCK_METHOD1(DeleteMyStationEntry,
               ::util::Status(const BcmFlowEntry& bcm_flow_entry));
  MOCK_METHOD1(InsertMulticastGroup,
               ::util::Status(const BcmFlowEntry& bcm_flow_entry));
  MOCK_METHOD1(DeleteMulticastGroup,
               ::util::Status(const BcmFlowEntry& bcm_flow_entry));
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_L2_MANAGER_MOCK_H_
