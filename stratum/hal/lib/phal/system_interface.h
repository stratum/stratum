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

#ifndef STRATUM_HAL_LIB_PHAL_SYSTEM_INTERFACE_H_
#define STRATUM_HAL_LIB_PHAL_SYSTEM_INTERFACE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace phal {

// We use unsigned long long int here to match the udev specification.
using UdevSequenceNumber = unsigned long long int;  // NOLINT(runtime/int)

// These two classes wrap all interactions with udev.
class UdevMonitor;
// Represents an instance of Udev. This class is used to initiate all monitoring
// of system hardware state.
class Udev {
 public:
  // All information relating to a single udev event.
  struct Event {
    std::string device_path;
    UdevSequenceNumber sequence_number;
    std::string action_type;
  };

  virtual ~Udev() {}

  // Creates a new udev monitor. This monitor is responsible for handling some
  // subset of udev events.
  virtual ::util::StatusOr<std::unique_ptr<UdevMonitor>> MakeUdevMonitor() = 0;

  // Returns a list of all devices in the given subsystem, and their current
  // states. Each returned pair contains <device path, action>. The returned
  // action is typically "add".
  virtual ::util::StatusOr<std::vector<std::pair<std::string, std::string>>>
  EnumerateSubsystem(const std::string& subsystem) = 0;
};

// Represents a single Udev Monitor, which is responsible for monitoring a
// subset of hardware state changes and reporting them via GetUdevEvent.
class UdevMonitor {
 public:
  virtual ~UdevMonitor() {}
  // Adds a filter to this monitor. By default, a monitor will receive all udev
  // events. Adding filters to a monitor limits the set of events handled.
  // Filters are applied by subsystem name. Note that UdevMonitorAddFilter may
  // not be called after UdevMonitorEnableReceiving.
  virtual ::util::Status AddFilter(const std::string& subsystem) = 0;

  // Enables receiving events. GetUdevEvent may not be called before
  // EnableReceiving.
  virtual ::util::Status EnableReceiving() = 0;

  // If a new udev event has been handled by this monitor, returns true.
  // Otherwise returns false. If true is returned, the passed event is
  // filled with the new udev event's information. If false is returned,
  // the passed event is unchanged.
  virtual ::util::StatusOr<bool> GetUdevEvent(Udev::Event* event) = 0;
};

// A mockable interface for all system interactions performed by
// our configuration code. Needed for testing and cross-platform purposes.
class SystemInterface {
 public:
  virtual ~SystemInterface() {}

  // File access functions:
  virtual bool PathExists(const std::string& path) const = 0;
  virtual ::util::Status ReadFileToString(const std::string& path,
                                          std::string* buffer) const = 0;
  virtual ::util::Status WriteStringToFile(const std::string& buffer,
                                           const std::string& path) const = 0;

  // Udev functions:
  // Creates a new Udev, which is responsible for all other udev functions.
  virtual ::util::StatusOr<std::unique_ptr<Udev>> MakeUdev() const = 0;

  // TODO(unknown): Add any other functions we need.
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_SYSTEM_INTERFACE_H_
