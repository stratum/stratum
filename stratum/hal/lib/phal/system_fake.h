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


#ifndef STRATUM_HAL_LIB_PHAL_SYSTEM_FAKE_H_
#define STRATUM_HAL_LIB_PHAL_SYSTEM_FAKE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "third_party/stratum/glue/status/status.h"
#include "third_party/stratum/glue/status/statusor.h"
#include "third_party/stratum/hal/lib/phal/system_interface.h"
#include "third_party/absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace phal {

class SystemFake;

class UdevFake : public Udev {
 public:
  explicit UdevFake(const SystemFake* system) : system_(system) {}
  ::util::StatusOr<std::unique_ptr<UdevMonitor>> MakeUdevMonitor() override;
  ::util::StatusOr<std::vector<std::pair<std::string, std::string>>>
  EnumerateSubsystem(const std::string& subsystem) override;

 private:
  const SystemFake* system_;
};

class UdevMonitorFake : public UdevMonitor {
 public:
  explicit UdevMonitorFake(const SystemFake* system) : system_(system) {}
  ::util::Status AddFilter(const std::string& subsystem) override;
  ::util::Status EnableReceiving() override;
  ::util::StatusOr<bool> GetUdevEvent(Udev::Event* event) override;

 private:
  const SystemFake* system_;
  std::set<std::string> filters_;
  bool receiving_ = false;
};

// A fake system for testing the attribute database.
class SystemFake : public SystemInterface {
 public:
  // Functions for adding to fake behavior:
  // Add a fake file with the given path and contents. This file will appear
  // in calls to PathExists and ReadFileToString.
  void AddFakeFile(const std::string& path, const std::string& contents);
  // Send a fake udev event. Anything using this class for its system interface
  // will see this event.
  // udev_filter: The udev_filter that will catch this event.
  // dev_path: The device path that this event affects.
  // sequence_number: The udev sequence number assigned to this event.
  //                  These numbers should be unique to avoid strange behavior.
  // action: The udev action that has occurred (e.g. 'add', 'remove')
  // send_event: If true, send this in response to UdevMonitorCheck. Otherwise
  //             only expose this change to calls of UdevEnumerateSubsystem.
  void SendUdevUpdate(const std::string& udev_filter,
                      const std::string& dev_path,
                      UdevSequenceNumber sequence_number,
                      const std::string& action, bool send_event);

  // SystemInterface functions:
  bool PathExists(const std::string& path) const override;
  ::util::Status ReadFileToString(const std::string& path,
                                  std::string* buffer) const override;
  ::util::Status WriteStringToFile(const std::string& buffer,
                                   const std::string& path) const override;

  // Udev functions:
  ::util::StatusOr<std::unique_ptr<Udev>> MakeUdev() const override;

 private:
  friend class UdevFake;
  friend class UdevMonitorFake;
  mutable std::map<std::string, std::string> path_to_file_contents_;
  mutable absl::Mutex udev_mutex_;
  std::map<std::pair<std::string, std::string>,
           std::pair<UdevSequenceNumber, std::string>>
      udev_state_ GUARDED_BY(udev_mutex_);
  mutable std::map<std::string, std::set<std::string>> updated_udev_devices_
      GUARDED_BY(udev_mutex_);
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_SYSTEM_FAKE_H_
