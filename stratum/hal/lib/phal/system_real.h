// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_PHAL_SYSTEM_REAL_H_
#define STRATUM_HAL_LIB_PHAL_SYSTEM_REAL_H_

#include <libudev.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <memory>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/system_interface.h"

namespace stratum {
namespace hal {
namespace phal {

class UdevReal : public Udev {
 public:
  explicit UdevReal(struct udev* udev) : udev_(udev) {}
  ~UdevReal() override;

  ::util::StatusOr<std::unique_ptr<UdevMonitor>> MakeUdevMonitor() override;
  ::util::StatusOr<std::vector<std::pair<std::string, std::string>>>
  EnumerateSubsystem(const std::string& subsystem) override;

 protected:
  struct udev* udev_;
};

class UdevMonitorReal : public UdevMonitor {
 public:
  UdevMonitorReal(struct udev_monitor* m, int monitor_fd)
      : receiving_(false), monitor_(m), fd_(monitor_fd), filters_({}) {}
  ~UdevMonitorReal() override;

  ::util::Status AddFilter(const std::string& subsystem) override;
  ::util::Status EnableReceiving() override;
  ::util::StatusOr<bool> GetUdevEvent(Udev::Event* event) override;

 protected:
  bool receiving_;
  struct udev_monitor* monitor_;
  int fd_;
  std::set<std::string> filters_;
};

// A thin wrapper for real system calls.
class SystemReal : public SystemInterface {
 public:
  static const SystemInterface* GetSingleton();
  bool PathExists(const std::string& path) const override;
  ::util::Status ReadFileToString(const std::string& path,
                                  std::string* buffer) const override;
  ::util::Status WriteStringToFile(const std::string& buffer,
                                   const std::string& path) const override;
  ::util::StatusOr<std::unique_ptr<Udev>> MakeUdev() const override;

 private:
  SystemReal() {}
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_SYSTEM_REAL_H_
