// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_COUNTER_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_COUNTER_MANAGER_MOCK_H_

#include <memory>

#include "gmock/gmock.h"
#include "stratum/hal/lib/barefoot/bfrt_pre_manager.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtCounterManagerMock : public BfrtCounterManager {
 public:
  // MOCK_METHOD1(PushChassisConfig,
  //              ::util::Status(const BfrtDeviceConfig& config, uint64
  //              node_id));
  // MOCK_METHOD2(VerifyChassisConfig,
  //              ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD1(PushForwardingPipelineConfig,
               ::util::Status(const BfrtDeviceConfig& config));
  MOCK_METHOD1(VerifyForwardingPipelineConfig,
               ::util::Status(const BfrtDeviceConfig& config));
  MOCK_METHOD3(
      WriteIndirectCounterEntry,
      ::util::Status(std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const ::p4::v1::Update::Type type,
                     const ::p4::v1::CounterEntry& counter_entry));
  MOCK_METHOD3(
      ReadIndirectCounterEntry,
      ::util::Status(std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const ::p4::v1::CounterEntry& counter_entry,
                     WriterInterface<::p4::v1::ReadResponse>* writer));
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_COUNTER_MANAGER_MOCK_H_
