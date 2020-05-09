// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_PHAL_BACKEND_INTERFACE_H_
#define STRATUM_HAL_LIB_PHAL_PHAL_BACKEND_INTERFACE_H_

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/common.pb.h"

namespace stratum {
namespace hal {
namespace phal {

// The "PhalBackendInterface" class defines the interface for PHAL backend
// components.
class PhalBackendInterface {
 public:
  virtual ~PhalBackendInterface() {}

  // Pushes the chassis config to the class. The ChassisConfig proto includes
  // any generic platform-independent configuration info which PHAL may need.
  // Note that platform-specific configuration is internal to the implementation
  // of this class and is not pushed from outside. This function is expected to
  // perform the coldboot init sequence if PHAL is not yet initialized by the
  // time config is pushed in the coldboot mode.
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config) = 0;

  // Verifies the part of config that this class cares about. This method can
  // be called at any point to verify if the ChassisConfig proto is compatible
  // with PHAL internal info (e.g. makes sure the external SingletonPort
  // messages in ChassisConfig with the same (slot, port) match what PHAL knows
  // about transceiver modules used for that (slot, port)).
  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config) = 0;

  // Fully uninitializes PHAL. Not used for warmboot shutdown. Note that there
  // is no public method to initialize the class. The initialization is done
  // internally after the class instance is created or after
  // PushChassisConfig().
  virtual ::util::Status Shutdown() = 0;

  // TODO(unknown): Add Freeze() and Unfreeze() functions to perform NSF
  // warmboot.
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_PHAL_BACKEND_INTERFACE_H_
