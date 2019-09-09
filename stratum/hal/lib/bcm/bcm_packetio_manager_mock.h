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


#ifndef STRATUM_HAL_LIB_BCM_BCM_PACKETIO_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_BCM_BCM_PACKETIO_MANAGER_MOCK_H_

#include <memory>

#include "stratum/hal/lib/bcm/bcm_packetio_manager.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {
namespace bcm {

class BcmPacketioManagerMock : public BcmPacketioManager {
 public:
  MOCK_METHOD2(PushChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD2(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_METHOD2(
      RegisterPacketReceiveWriter,
      ::util::Status(
          GoogleConfig::BcmKnetIntfPurpose purpose,
          const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer));
  MOCK_METHOD1(UnregisterPacketReceiveWriter,
               ::util::Status(GoogleConfig::BcmKnetIntfPurpose purpose));
  MOCK_METHOD2(TransmitPacket,
               ::util::Status(GoogleConfig::BcmKnetIntfPurpose purpose,
                              const ::p4::v1::PacketOut& packet));
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_PACKETIO_MANAGER_MOCK_H_
