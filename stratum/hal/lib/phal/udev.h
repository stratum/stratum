// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_UDEV_H_
#define STRATUM_HAL_LIB_PHAL_UDEV_H_

#include <libudev.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/udev_interface.h"

namespace stratum {
namespace hal {

// Class "Udev" is a wrapper around the linux libudev and implements the
// UdevInterface class is a thread-safe way. Note that this class is used only
// in LegacyPhal, an implementation of PhalInterface based on the legacy
// Sandcastle stack. The newer version of PHAL will not use this class.
class Udev : public UdevInterface {
 public:
  ~Udev() override;

  ::util::Status Initialize(const std::string& filter) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status Shutdown() override LOCKS_EXCLUDED(data_lock_);
  ::util::StatusOr<std::pair<std::string, std::string>> Check() override
      LOCKS_EXCLUDED(data_lock_);

  // Creates the instance. Returns nullptr if there is any issue.
  static std::unique_ptr<Udev> CreateInstance();

  // Udev is neither copyable nor movable.
  Udev(const Udev&) = delete;
  Udev& operator=(const Udev&) = delete;

 private:
  // Private constructor.
  Udev();

  // Mutex lock for protecting the internal state.
  mutable absl::Mutex data_lock_;

  struct udev* udev_ GUARDED_BY(data_lock_);
  struct udev_monitor* udev_monitor_ GUARDED_BY(data_lock_);
  int fd_ GUARDED_BY(data_lock_);  // FD for the monitor.
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_UDEV_H_
