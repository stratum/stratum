// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_SDE_INTERFACE_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_SDE_INTERFACE_H_

#include <memory>

#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/lib/channel/channel.h"

namespace stratum {
namespace hal {
namespace barefoot {

// TODO(max): docs
class BfSdeInterface {
 public:
  struct PortStatusEvent {
    int device;
    int port;
    PortState state;
  };

  virtual ~BfSdeInterface() {}

  virtual ::util::StatusOr<PortState> GetPortState(int device, int port) = 0;

  virtual ::util::Status GetPortCounters(int device, int port,
                                         PortCounters* counters) = 0;

  virtual ::util::Status RegisterPortStatusEventWriter(
      std::unique_ptr<ChannelWriter<PortStatusEvent> > writer) = 0;

  virtual ::util::Status UnregisterPortStatusEventWriter() = 0;

  virtual ::util::Status AddPort(int device, int port, uint64 speed_bps,
                                 FecMode fec_mode = FEC_MODE_UNKNOWN) = 0;

  virtual ::util::Status DeletePort(int device, int port) = 0;

  virtual ::util::Status EnablePort(int device, int port) = 0;

  virtual ::util::Status DisablePort(int device, int port) = 0;

  virtual ::util::Status SetPortAutonegPolicy(int device, int port,
                                              TriState autoneg) = 0;

  virtual ::util::Status SetPortMtu(int device, int port, int32 mtu) = 0;

  virtual bool IsValidPort(int device, int port) = 0;

  virtual ::util::Status SetPortLoopbackMode(int device, int port,
                                             LoopbackState loopback_mode) = 0;

  virtual ::util::StatusOr<uint32> PortIdFromPortKeyGet(
      int device, const PortKey& port_key) = 0;

  // Get the CPU port of a device.
  virtual ::util::StatusOr<int> GetPcieCpuPort(int device) = 0;

  // Set the CPU port in the traffic manager.
  virtual ::util::Status SetTmCpuPort(int device, int port) = 0;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BfSdeInterface() {}
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_SDE_INTERFACE_H_
