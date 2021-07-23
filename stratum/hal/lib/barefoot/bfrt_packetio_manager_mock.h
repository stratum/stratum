// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_PACKETIO_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_PACKETIO_MANAGER_MOCK_H_

#include <memory>

#include "gmock/gmock.h"
#include "stratum/hal/lib/barefoot/bfrt_packetio_manager.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtPacketioManagerMock : public BfrtPacketioManager {
 public:
  MOCK_METHOD2(PushChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD2(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD1(PushForwardingPipelineConfig,
               ::util::Status(const BfrtDeviceConfig& config));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_METHOD1(
      RegisterPacketReceiveWriter,
      ::util::Status(
          const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer));
  MOCK_METHOD0(UnregisterPacketReceiveWriter, ::util::Status());
  MOCK_METHOD1(TransmitPacket,
               ::util::Status(const ::p4::v1::PacketOut& packet));
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_PACKETIO_MANAGER_MOCK_H_
