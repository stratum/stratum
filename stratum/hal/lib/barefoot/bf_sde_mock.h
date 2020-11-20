// Copyright 2019-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_SDE_MOCK_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_SDE_MOCK_H_

#include <memory>

#include "gmock/gmock.h"
#include "stratum/hal/lib/barefoot/bf_sde_interface.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfSdeMock : public BfSdeInterface {
 public:
  MOCK_METHOD2(GetPortState, ::util::StatusOr<PortState>(int device, int port));
  MOCK_METHOD3(GetPortCounters,
               ::util::Status(int device, int port, PortCounters* counters));
  MOCK_METHOD1(
      RegisterPortStatusEventWriter,
      ::util::Status(std::unique_ptr<ChannelWriter<PortStatusEvent> > writer));
  MOCK_METHOD0(UnregisterPortStatusEventWriter, ::util::Status());
  MOCK_METHOD4(AddPort, ::util::Status(int device, int port, uint64 speed_bps,
                                       FecMode fec_mode));
  MOCK_METHOD2(DeletePort, ::util::Status(int device, int port));
  MOCK_METHOD2(EnablePort, ::util::Status(int device, int port));
  MOCK_METHOD2(DisablePort, ::util::Status(int device, int port));
  MOCK_METHOD3(SetPortAutonegPolicy,
               ::util::Status(int device, int port, TriState autoneg));
  MOCK_METHOD3(SetPortMtu, ::util::Status(int device, int port, int32 mtu));
  MOCK_METHOD2(IsValidPort, bool(int device, int port));
  MOCK_METHOD3(SetPortLoopbackMode,
               ::util::Status(int uint, int port, LoopbackState loopback_mode));
  MOCK_METHOD2(GetPortIdFromPortKey,
               ::util::StatusOr<uint32>(int device, const PortKey& port_key));
  MOCK_METHOD1(GetPcieCpuPort, ::util::StatusOr<int>(int device));
  MOCK_METHOD2(SetTmCpuPort, ::util::Status(int device, int port));
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_SDE_MOCK_H_
