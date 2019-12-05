// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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

#include "stratum/hal/lib/phal/udev.h"

#include "absl/memory/memory.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {

Udev::Udev() : udev_(nullptr), udev_monitor_(nullptr), fd_(0) {}

Udev::~Udev() { Shutdown().IgnoreError(); }

::util::Status Udev::Initialize(const std::string& filter) {
  absl::WriterMutexLock l(&data_lock_);
  CHECK_RETURN_IF_FALSE(udev_ == nullptr && udev_monitor_ == nullptr)
      << "Udev already initialized. Call Shutdown() first.";
  udev_ = udev_new();
  CHECK_RETURN_IF_FALSE(udev_);
  udev_monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  CHECK_RETURN_IF_FALSE(udev_monitor_);
  CHECK_RETURN_IF_FALSE(0 == udev_monitor_filter_add_match_subsystem_devtype(
                                 udev_monitor_, filter.c_str(), nullptr));
  CHECK_RETURN_IF_FALSE(0 == udev_monitor_enable_receiving(udev_monitor_));
  fd_ = udev_monitor_get_fd(udev_monitor_);
  CHECK_RETURN_IF_FALSE(fd_ > 0);

  return ::util::OkStatus();
}

::util::Status Udev::Shutdown() {
  absl::WriterMutexLock l(&data_lock_);
  if (udev_ || udev_monitor_) {
    udev_unref(udev_);
    udev_monitor_unref(udev_monitor_);
    udev_ = nullptr;
    udev_monitor_ = nullptr;
    fd_ = 0;
  }

  return ::util::OkStatus();
}

// TODO(unknown): Consider using poll() API.
::util::StatusOr<std::pair<std::string, std::string>> Udev::Check() {
  absl::ReaderMutexLock l(&data_lock_);
  std::pair<std::string, std::string> data = {"", ""};
  if (!fd_) {
    return data;
  }
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd_, &fds);
  struct timeval tv;
  tv.tv_sec = 0;   // zero-time means non-blocking.
  tv.tv_usec = 0;  // zero-time means non-blocking.
  if (select(fd_ + 1, &fds, nullptr, nullptr, &tv) > 0 && FD_ISSET(fd_, &fds)) {
    struct udev_device* udev_event = udev_monitor_receive_device(udev_monitor_);
    if (udev_event) {
      data.first = std::string(udev_device_get_action(udev_event));
      // The devpath return does not include "/sys". Add it explicitly. Note
      // that there is no need to change this in future or have a flag for it.
      // This should always be the same value.
      data.second = "/sys" + std::string(udev_device_get_devpath(udev_event));
      udev_device_unref(udev_event);
    }
  }

  return data;
}

std::unique_ptr<Udev> Udev::CreateInstance() {
  return absl::WrapUnique(new Udev());
}

}  // namespace hal
}  // namespace stratum
