/* Copyright 2019-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>

#include "gmock/gmock.h"
#include "stratum/hal/lib/barefoot/bf_pal_interface.h"

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_PAL_MOCK_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_PAL_MOCK_H_

namespace stratum {
namespace hal {
namespace barefoot {

class BFPalMock : public BFPalInterface {
 public:
  MOCK_METHOD2(PortOperStateGet,
               ::util::StatusOr<PortState>(int unit, uint32 port_id));

  MOCK_METHOD3(PortAllStatsGet,
               ::util::Status(int unit, uint32 port_id,
                              PortCounters* counters));

  MOCK_METHOD1(
      PortStatusChangeRegisterEventWriter,
      ::util::Status(
           std::unique_ptr<ChannelWriter<PortStatusChangeEvent> > writer));

  MOCK_METHOD0(PortStatusChangeUnregisterEventWriter, ::util::Status());

  MOCK_METHOD4(PortAdd,
               ::util::Status(int unit, uint32 port_id, uint64 speed_bps,
                              FecMode fec_mode));

  MOCK_METHOD2(PortDelete, ::util::Status(int unit, uint32 port_id));

  MOCK_METHOD2(PortEnable, ::util::Status(int unit, uint32 port_id));

  MOCK_METHOD2(PortDisable, ::util::Status(int unit, uint32 port_id));

  MOCK_METHOD3(PortAutonegPolicySet,
               ::util::Status(int unit, uint32 port_id, TriState autoneg));

  MOCK_METHOD3(PortMtuSet,
               ::util::Status(int unit, uint32 port_id, int32 mtu));

  MOCK_METHOD2(PortIsValid, bool(int unit, uint32 port_id));
  MOCK_METHOD3(PortLoopbackModeSet,
               ::util::Status(int uint, uint32 port_id,
               LoopbackState loopback_mode));
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_PAL_MOCK_H_
