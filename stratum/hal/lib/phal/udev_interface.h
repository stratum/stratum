// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_PHAL_UDEV_INTERFACE_H_
#define STRATUM_HAL_LIB_PHAL_UDEV_INTERFACE_H_

#include <string>
#include <utility>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {

// Class "UdevInterface" defines an interface around the linux libudev as it is
// used to detect insert/removal of hot-pluggable hardware modules (e.g.
// transceiver modules like QSFPs).
class UdevInterface {
 public:
  virtual ~UdevInterface() {}

  // Initializes the class given the 'filter' used to filter out the udev
  // devices.
  virtual ::util::Status Initialize(const std::string& filter) = 0;

  // Shuts down the class and resets all the internal state.
  virtual ::util::Status Shutdown() = 0;

  // A non-blocking call which checks if there is a new change in the list of
  // connected devices. It then return the pair of (action, devpath) for the
  // device which was connected/disconnected. This method is often called in
  // in context of another thread.
  virtual ::util::StatusOr<std::pair<std::string, std::string>> Check() = 0;

 protected:
  // Constructor is protected (as opposed to private) so that the mock class
  // can access it, while the external classes need to use the factory.
  UdevInterface() {}
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_UDEV_INTERFACE_H_
