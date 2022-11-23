// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation.
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_DPDK_DPDK_CHASSIS_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_TDI_DPDK_DPDK_CHASSIS_MANAGER_MOCK_H_

#include <map>
#include <memory>

#include "gmock/gmock.h"
#include "stratum/hal/lib/tdi/dpdk/dpdk_chassis_manager.h"

namespace stratum {
namespace hal {
namespace tdi {

class DpdkChassisManagerMock : public DpdkChassisManager {
 public:
  MOCK_METHOD1(PushChassisConfig, ::util::Status(const ChassisConfig& config));
  MOCK_METHOD1(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config));
  MOCK_METHOD0(Shutdown, ::util::Status());

  MOCK_METHOD1(
      RegisterEventNotifyWriter,
      ::util::Status(
          const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer));
  MOCK_METHOD0(UnregisterEventNotifyWriter, ::util::Status());
  MOCK_METHOD1(GetPortData, ::util::StatusOr<DataResponse>(
                                const DataRequest::Request& request));
  MOCK_METHOD2(GetPortTimeLastChanged,
               ::util::StatusOr<absl::Time>(uint64 node_id, uint32 port_id));
  MOCK_METHOD3(GetPortCounters, ::util::Status(uint64 node_id, uint32 port_id,
                                               PortCounters* counters));
  //  MOCK_METHOD1(ReplayPortsConfig, ::util::Status(uint64 node_id));
  //  MOCK_METHOD3(GetFrontPanelPortInfo,
  //               ::util::Status(uint64 node_id, uint32 port_id,
  //                              FrontPanelPortInfo* fp_port_info));
  MOCK_CONST_METHOD0(GetNodeIdToDeviceMap,
                     ::util::StatusOr<std::map<uint64, int>>());
  MOCK_CONST_METHOD1(GetDeviceFromNodeId,
                     ::util::StatusOr<int>(uint64 node_id));
};

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_DPDK_DPDK_CHASSIS_MANAGER_MOCK_H_
