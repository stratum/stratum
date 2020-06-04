// Copyright 2020-present Open Networking Founcation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_PD_WRAPPER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_PD_WRAPPER_H_

#include "stratum/hal/lib/barefoot/bf_pd_interface.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BFPdWrapper : public BFPdInterface {
 public:
  // Gets the CPU port of an unit(device).
  ::util::StatusOr<int> PcieCpuPortGet(int unit) override;

  // Sets the CPU port to the traffic manager.
  ::util::Status TmSetCpuPort(int unit, int port) override;

  static BFPdWrapper* GetSingleton();

  // BFPdWrapper is neither copyable nor movable.
  BFPdWrapper(const BFPdWrapper&) = delete;
  BFPdWrapper& operator=(const BFPdWrapper&) = delete;
  BFPdWrapper(BFPdWrapper&&) = delete;
  BFPdWrapper& operator=(BFPdWrapper&&) = delete;

 private:
  BFPdWrapper();  // Use GetSingleton
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_PD_WRAPPER_H_
