// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_BCM_BCM_L3_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_BCM_BCM_L3_MANAGER_MOCK_H_

#include "stratum/hal/lib/bcm/bcm_l3_manager.h"
#include "gmock/gmock.h"

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
