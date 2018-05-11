// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "third_party/stratum/hal/lib/phal/system_fake.h"

#include "third_party/stratum/lib/macros.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace phal {

void SystemFake::AddFakeFile(const std::string& path,
                             const std::string& contents) {
  path_to_file_contents_.insert(std::make_pair(path, contents));
}

void SystemFake::SendUdevUpdate(const std::string& udev_filter,
                                const std::string& dev_path,
                                UdevSequenceNumber sequence_number,
                                const std::string& action, bool send_event) {
  absl::MutexLock lock(&udev_mutex_);
  udev_state_[std::make_pair(udev_filter, dev_path)] =
      std::make_pair(sequence_number, action);
  if (send_event) {
    updated_udev_devices_.insert(
        std::make_pair(udev_filter, std::set<std::string>()));
    updated_udev_devices_[udev_filter].insert(dev_path);
  }
}

bool SystemFake::PathExists(const std::string& path) const {
  return path_to_file_contents_.find(path) != path_to_file_contents_.end();
}

::util::Status SystemFake::ReadFileToString(const std::string& path,
                                            std::string* buffer) const {
  auto found_contents = path_to_file_contents_.find(path);
  CHECK_RETURN_IF_FALSE(found_contents != path_to_file_contents_.end())
      << "Cannot read file " << path << " to string. Does not exist.";
  *buffer = found_contents->second;
  return ::util::OkStatus();
}

::util::Status SystemFake::WriteStringToFile(const std::string& buffer,
                                             const std::string& path) const {
  path_to_file_contents_[path] = buffer;
  return ::util::OkStatus();
}

::util::StatusOr<std::unique_ptr<Udev>> SystemFake::MakeUdev() const {
  return {absl::make_unique<UdevFake>(this)};
}

::util::StatusOr<std::unique_ptr<UdevMonitor>> UdevFake::MakeUdevMonitor() {
  return {absl::make_unique<UdevMonitorFake>(system_)};
}

::util::StatusOr<std::vector<std::pair<std::string, std::string>>>
UdevFake::EnumerateSubsystem(const std::string& subsystem) {
  absl::MutexLock lock(&system_->udev_mutex_);
  std::vector<std::pair<std::string, std::string>> enumeration;
  for (const auto& device : system_->udev_state_) {
    // Check that this device is in the given subsystem, and has a most
    // recent action indicating that it is present.
    const std::pair<std::string, std::string>& filter_and_device = device.first;
    const std::pair<UdevSequenceNumber, std::string>& seq_and_action =
        device.second;
    if (device.first.first == subsystem && device.second.second != "remove") {
      enumeration.push_back(
          std::make_pair(filter_and_device.second, seq_and_action.second));
    }
  }
  return enumeration;
}

::util::Status UdevMonitorFake::AddFilter(const std::string& subsystem) {
  CHECK_RETURN_IF_FALSE(!receiving_);
  // This currently only supports testing subsystem filters. We'll need to
  // update this if we ever want to use devtype filters as well.
  filters_.insert(subsystem);
  return ::util::OkStatus();
}

::util::Status UdevMonitorFake::EnableReceiving() {
  receiving_ = true;
  return ::util::OkStatus();
}

::util::StatusOr<bool> UdevMonitorFake::GetUdevEvent(Udev::Event* event) {
  absl::MutexLock lock(&system_->udev_mutex_);
  CHECK_RETURN_IF_FALSE(receiving_);
  for (const auto& udev_filter : filters_) {
    auto filter_updates = system_->updated_udev_devices_.find(udev_filter);
    if (filter_updates != system_->updated_udev_devices_.end()) {
      // There may be an update matching this filter.
      for (const auto& updated_device : filter_updates->second) {
        auto latest_info = system_->udev_state_.find(
            std::make_pair(udev_filter, updated_device));
        if (latest_info != system_->udev_state_.end()) {
          // We've found an update! Return it and erase it -- we only return
          // each update once.
          *event = {updated_device, latest_info->second.first,
                    latest_info->second.second};
          filter_updates->second.erase(updated_device);
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
