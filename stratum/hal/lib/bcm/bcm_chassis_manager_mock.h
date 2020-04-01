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

#ifndef STRATUM_HAL_LIB_BCM_BCM_CHASSIS_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_BCM_BCM_CHASSIS_MANAGER_MOCK_H_

#include <map>
#include <memory>

#include "gmock/gmock.h"
#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"

namespace stratum {
namespace hal {
namespace bcm {

class BcmChassisManagerMock : public BcmChassisManager {
 public:
  MOCK_METHOD1(PushChassisConfig, ::util::Status(const ChassisConfig& config));
  MOCK_METHOD1(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_METHOD1(SetUnitToBcmNodeMap,
               void(const std::map<int, BcmNode*>& unit_to_bcm_node));
  MOCK_CONST_METHOD1(GetPortState,
                     ::util::StatusOr<PortState>(const SdkPort& sdk_port));
  MOCK_METHOD1(
      RegisterEventNotifyWriter,
      ::util::Status(std::shared_ptr<WriterInterface<GnmiEventPtr>> writer));
  MOCK_METHOD0(UnregisterEventNotifyWriter, ::util::Status());
  MOCK_METHOD3(SetPortLoopbackState,
               ::util::Status(uint64 node_id, uint32 port_id,
                              LoopbackState state));
  MOCK_CONST_METHOD2(GetBcmPort,
                     ::util::StatusOr<BcmPort>(uint64 node_id, uint32 port_id));
  MOCK_CONST_METHOD2(GetPortState, ::util::StatusOr<PortState>(uint64 node_id,
                                                               uint32 port_id));
  MOCK_CONST_METHOD2(GetPortAdminState,
                     ::util::StatusOr<AdminState>(uint64 node_id,
                                                  uint32 port_id));
  MOCK_CONST_METHOD2(GetPortLoopbackState,
                     ::util::StatusOr<LoopbackState>(uint64 node_id,
                                                     uint32 port_id));
  MOCK_CONST_METHOD0(GetNodeIdToUnitMap,
                     ::util::StatusOr<std::map<uint64, int>>());
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_CHASSIS_MANAGER_MOCK_H_
