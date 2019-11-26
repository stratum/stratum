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

#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/channel/channel.h"

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_PAL_INTERFACE_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_PAL_INTERFACE_H_

namespace stratum {
namespace hal {
namespace barefoot {

class BFPalInterface {
 public:
  struct PortStatusChangeEvent {
    int unit;
    uint32 port_id;
    PortState state;
  };

  virtual ~BFPalInterface() { }

  virtual ::util::StatusOr<PortState> PortOperStateGet(
      int unit, uint32 port_id) = 0;

  virtual ::util::Status PortAllStatsGet(
      int unit, uint32 port_id, PortCounters* counters) = 0;

  virtual ::util::Status PortStatusChangeRegisterEventWriter(
       std::unique_ptr<ChannelWriter<PortStatusChangeEvent> > writer) = 0;

  virtual ::util::Status PortStatusChangeUnregisterEventWriter() = 0;

  virtual ::util::Status PortAdd(
      int unit, uint32 port_id, uint64 speed_bps,
      FecMode fec_mode = FEC_MODE_UNKNOWN) = 0;

  virtual ::util::Status PortDelete(int unit, uint32 port_id) = 0;

  virtual ::util::Status PortEnable(int unit, uint32 port_id) = 0;

  virtual ::util::Status PortDisable(int unit, uint32 port_id) = 0;

  virtual ::util::Status PortAutonegPolicySet(
      int unit, uint32 port_id, TriState autoneg) = 0;

  virtual ::util::Status PortMtuSet(int unit, uint32 port_id, int32 mtu) = 0;

  virtual bool PortIsValid(int unit, uint32 port_id) = 0;

  virtual ::util::Status PortLoopbackModeSet(int uint, uint32 port_id,
                                             LoopbackState loopback_mode) = 0;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_PAL_INTERFACE_H_
