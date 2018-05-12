/*
 * Copyright 2018 Google LLC
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
