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


#include "stratum/hal/lib/phal/system_real.h"

#include "stratum/glue/status/posix_error_space.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace phal {
namespace {
absl::Mutex singleton_lock_;
const SystemReal* singleton_instance_ GUARDED_BY(singleton_lock_);
}  // namespace

// TODO: Test this code. This needs to be tested on a device with
// actual udev support, since its correctness is entirely dependent on correct
// usage of the udev interface.

UdevReal::~UdevReal() {
  if (udev_ != nullptr) {
    udev_unref(udev_);
  }
}

UdevMonitorReal::~UdevMonitorReal() {
  if (monitor_ != nullptr) {
    udev_monitor_unref(monitor_);
  }
}

const SystemInterface* SystemReal::GetSingleton()
    LOCKS_EXCLUDED(singleton_lock_) {
  absl::MutexLock lock(&singleton_lock_);
  if (singleton_instance_ == nullptr) singleton_instance_ = new SystemReal();
  return singleton_instance_;
}

bool SystemReal::PathExists(const std::string& path) const {
  return ::google::hercules::PathExists(path);
}

::util::Status SystemReal::ReadFileToString(const std::string& path,
                                            std::string* buffer) const {
  return ::google::hercules::ReadFileToString(path, buffer);
}

::util::Status SystemReal::WriteStringToFile(const std::string& buffer,
                                             const std::string& path) const {
  return ::google::hercules::WriteStringToFile(buffer, path);
}

::util::StatusOr<std::unique_ptr<Udev>> SystemReal::MakeUdev() const {
  struct udev* udev = udev_new();
  if (udev == nullptr)
    return ::util::PosixErrorToStatus(errno, "udev_new failed");
  return {absl::make_unique<UdevReal>(udev)};
}

::util::StatusOr<std::unique_ptr<UdevMonitor>> UdevReal::MakeUdevMonitor() {
  struct udev_monitor* udev_monitor =
      udev_monitor_new_from_netlink(udev_, "udev");
  if (udev_monitor == nullptr) {
    return ::util::PosixErrorToStatus(errno,
                                      "udev_monitor_new_from_netlink failed");
  }
  int fd = udev_monitor_get_fd(udev_monitor);
  if (fd < 0)
    return ::util::PosixErrorToStatus(errno, "udev_monitor_get_fd failed");
  return {absl::make_unique<UdevMonitorReal>(udev_monitor, fd)};
}

::util::StatusOr<std::vector<std::pair<std::string, std::string>>>
UdevReal::EnumerateSubsystem(const std::string& subsystem) {
  struct udev_enumerate* enumerate = udev_enumerate_new(udev_);
  if (enumerate == nullptr)
    return ::util::PosixErrorToStatus(errno, "udev_enumerate_new failed");
  udev_enumerate_add_match_subsystem(enumerate, subsystem.c_str());
  udev_enumerate_scan_devices(enumerate);
  struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
  struct udev_list_entry* device_entry;
  std::vector<std::pair<std::string, std::string>> found_devices;
  udev_list_entry_foreach(device_entry, devices) {
    const char* path = udev_list_entry_get_name(device_entry);
    struct udev_device* device = udev_device_new_from_syspath(udev_, path);
    if (!device) continue;
    const char* dev_path_cstr = udev_device_get_devpath(device);
    if (dev_path_cstr == nullptr) {
      udev_device_unref(device);
      return MAKE_ERROR() << "Could not get device path for udev device.";
    }
    std::string dev_path = "/sys" + std::string(dev_path_cstr);
    // We pretend that the most recent event was an "add", indicating that the
    // device is present and operational.
    // TODO: Check for other possible states -- e.g. the device could
    // be present, but disabled, failed, etc.
    found_devices.push_back(std::make_pair(dev_path, "add"));
    udev_device_unref(device);
  }
  udev_enumerate_unref(enumerate);
  return found_devices;
}

::util::Status UdevMonitorReal::AddFilter(const std::string& subsystem) {
  CHECK_RETURN_IF_FALSE(!receiving_)
      << "Cannot add a filter to a receiving udev monitor.";
  if (udev_monitor_filter_add_match_subsystem_devtype(
          monitor_, subsystem.c_str(), "") != 0) {
    return ::util::PosixErrorToStatus(
        errno, "Failed to add udev subsystem " + subsystem);
  }
  filters_.insert(subsystem);
  return ::util::OkStatus();
}

::util::Status UdevMonitorReal::EnableReceiving() {
  CHECK_RETURN_IF_FALSE(!receiving_) << "Udev monitor is already receiving.";
  if (udev_monitor_enable_receiving(monitor_) != 0) {
    return ::util::PosixErrorToStatus(errno,
                                      "udev_monitor_enable_receiving failed");
  }
  receiving_ = true;
  return ::util::OkStatus();
}

::util::StatusOr<bool> UdevMonitorReal::GetUdevEvent(Udev::Event* event) {
  int fd = fd_;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  struct timeval tv;
  tv.tv_sec = 0;   // non-blocking.
  tv.tv_usec = 0;  // non-blocking.
  while (select(fd + 1, &fds, nullptr, nullptr, &tv) > 0 &&
         FD_ISSET(fd, &fds)) {
    struct udev_device* udev_event = udev_monitor_receive_device(monitor_);
    if (udev_event) {
      // We need to check that the subsytem matches the expected set of filters,
      // since udev allows spurious events.
      const char* subsystem_cstr = udev_device_get_subsystem(udev_event);
      CHECK_RETURN_IF_FALSE(subsystem_cstr)
          << "Could not get subsystem for udev device.";
      std::string subsystem = std::string(subsystem_cstr);
      if (filters_.count(subsystem) == 0)
        continue;  // This is a spurious event.
      const char* dev_path_cstr = udev_device_get_devpath(udev_event);
      if (dev_path_cstr == nullptr) {
        udev_device_unref(udev_event);
        return MAKE_ERROR() << "Could not get device path for udev device.";
      }
      std::string dev_path = "/sys" + std::string(dev_path_cstr);
      UdevSequenceNumber seqnum = udev_device_get_seqnum(udev_event);
      const char* action_ptr = udev_device_get_action(udev_event);
      std::string action = (action_ptr == nullptr ? "" : action_ptr);
      udev_device_unref(udev_event);
      *event = {dev_path, seqnum, action};
      return true;
    }
    break;
  }
  return false;
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
