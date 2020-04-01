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

#ifndef STRATUM_HAL_LIB_BCM_BCM_CHASSIS_RO_MOCK_H_
#define STRATUM_HAL_LIB_BCM_BCM_CHASSIS_RO_MOCK_H_

#include <map>
#include <set>

#include "gmock/gmock.h"
#include "stratum/hal/lib/bcm/bcm_chassis_ro_interface.h"

namespace stratum {

namespace hal {
namespace bcm {

class BcmChassisRoMock : public BcmChassisRoInterface {
 public:
  MOCK_CONST_METHOD1(GetBcmChip, ::util::StatusOr<BcmChip>(int unit));
  MOCK_CONST_METHOD3(GetBcmPort, ::util::StatusOr<BcmPort>(int slot, int port,
                                                           int channel));
  MOCK_CONST_METHOD2(GetBcmPort,
                     ::util::StatusOr<BcmPort>(uint64 node_id, uint32 port_id));
  MOCK_CONST_METHOD0(GetNodeIdToUnitMap,
                     ::util::StatusOr<std::map<uint64, int>>());
  MOCK_CONST_METHOD1(GetUnitFromNodeId, ::util::StatusOr<int>(uint64 node_id));
  MOCK_CONST_METHOD1(
      GetPortIdToSdkPortMap,
      ::util::StatusOr<std::map<uint32, SdkPort>>(uint64 node_id));
  MOCK_CONST_METHOD1(
      GetTrunkIdToSdkTrunkMap,
      ::util::StatusOr<std::map<uint32, SdkTrunk>>(uint64 node_id));
  MOCK_CONST_METHOD2(GetPortState, ::util::StatusOr<PortState>(uint64 node_id,
                                                               uint32 port_id));
  MOCK_CONST_METHOD2(GetTrunkState,
                     ::util::StatusOr<TrunkState>(uint64 node_id,
                                                  uint32 trunk_id));
  MOCK_CONST_METHOD2(GetTrunkMembers,
                     ::util::StatusOr<std::set<uint32>>(uint64 node_id,
                                                        uint32 trunk_id));
  MOCK_CONST_METHOD2(GetParentTrunkId,
                     ::util::StatusOr<uint32>(uint64 node_id, uint32 port_id));
  MOCK_CONST_METHOD1(GetPortState,
                     ::util::StatusOr<PortState>(const SdkPort& sdk_port));
  MOCK_CONST_METHOD2(GetPortAdminState,
                     ::util::StatusOr<AdminState>(uint64 node_id,
                                                  uint32 port_id));
  MOCK_CONST_METHOD2(GetPortLoopbackState,
                     ::util::StatusOr<LoopbackState>(uint64 node_id,
                                                     uint32 port_id));
  MOCK_CONST_METHOD3(GetPortCounters,
                     ::util::Status(uint64 node_id, uint32 port_id,
                                    PortCounters* pc));
};

}  // namespace bcm
}  // namespace hal

}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_CHASSIS_RO_MOCK_H_
