// Copyright 2020-present Open Networking Founcation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_PD_INTERFACE_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_PD_INTERFACE_H_

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BFPdInterface {
 public:
  virtual ~BFPdInterface() {}
  // Gets the CPU port of an unit(device).
  virtual ::util::StatusOr<int> GetPcieCpuPort(int unit) = 0;

  // Sets the CPU port to the traffic manager.
  virtual ::util::Status SetTmCpuPort(int unit, int port) = 0;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_PD_INTERFACE_H_
