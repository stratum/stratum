/*
 * Copyright 2018 Google LLC
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

#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"
#include "testing/base/public/gmock.h"

namespace stratum {
namespace hal {
namespace bcm {

class BcmChassisManagerMock : public BcmChassisManager {
 public:
  MOCK_METHOD1(PushChassisConfig, ::util::Status(const ChassisConfig& config));
  MOCK_METHOD1(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_METHOD0(ReadTransceiverEvents, void());
  MOCK_CONST_METHOD1(GetBcmChip, ::util::StatusOr<BcmChip>(int unit));
  MOCK_CONST_METHOD3(GetBcmPort, ::util::StatusOr<BcmPort>(int slot, int port,
                                                           int channel));
  MOCK_CONST_METHOD0(GetNodeIdToUnitMap,
                     ::util::StatusOr<std::map<uint64, int>>());
  MOCK_CONST_METHOD1(GetUnitFromNodeId, ::util::StatusOr<int>(uint64 node_id));
  MOCK_CONST_METHOD0(GetPortIdToUnitLogicalPortMap,
                     ::util::StatusOr<std::map<uint64, std::pair<int, int>>>());
  MOCK_CONST_METHOD0(GetTrunkIdToUnitTrunkPortMap,
                     ::util::StatusOr<std::map<uint64, std::pair<int, int>>>());
  MOCK_CONST_METHOD1(GetPortState, ::util::StatusOr<PortState>(uint64 port_id));
  MOCK_METHOD1(
      RegisterEventNotifyWriter,
      ::util::Status(
          const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer));
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_CHASSIS_MANAGER_MOCK_H_
