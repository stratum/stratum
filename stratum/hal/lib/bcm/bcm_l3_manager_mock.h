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


#ifndef STRATUM_HAL_LIB_BCM_BCM_L3_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_BCM_BCM_L3_MANAGER_MOCK_H_

#include "stratum/hal/lib/bcm/bcm_l3_manager.h"
#include "testing/base/public/gmock.h"

namespace stratum {
namespace hal {
namespace bcm {

class BcmL3ManagerMock : public BcmL3Manager {
 public:
  MOCK_METHOD2(PushChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD2(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_METHOD1(FindOrCreateNonMultipathNexthop,
               ::util::StatusOr<int>(const BcmNonMultipathNexthop& nexthop));
  MOCK_METHOD1(FindOrCreateMultipathNexthop,
               ::util::StatusOr<int>(const BcmMultipathNexthop& nexthop));
  MOCK_METHOD2(ModifyNonMultipathNexthop,
               ::util::Status(int egress_intf_id,
                              const BcmNonMultipathNexthop& nexthop));
  MOCK_METHOD2(ModifyMultipathNexthop,
               ::util::Status(int egress_intf_id,
                              const BcmMultipathNexthop& nexthop));
  MOCK_METHOD1(DeleteNonMultipathNexthop, ::util::Status(int egress_intf_id));
  MOCK_METHOD1(DeleteMultipathNexthop, ::util::Status(int egress_intf_id));
  MOCK_METHOD1(InsertTableEntry,
               ::util::Status(const ::p4::v1::TableEntry& entry));
  MOCK_METHOD1(ModifyTableEntry,
               ::util::Status(const ::p4::v1::TableEntry& entry));
  MOCK_METHOD1(DeleteTableEntry,
               ::util::Status(const ::p4::v1::TableEntry& entry));
  MOCK_METHOD1(UpdateMultipathGroupsForPort, ::util::Status(uint32 port_id));
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_L3_MANAGER_MOCK_H_
