// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_UDEV_EVENT_HANDLER_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_UDEV_EVENT_HANDLER_MOCK_H_

#include <string>

#include "gmock/gmock.h"
#include "stratum/hal/lib/phal/udev_event_handler.h"

namespace stratum {
namespace hal {
namespace phal {

class UdevEventCallbackMock : public UdevEventCallback {
 public:
  UdevEventCallbackMock(const std::string& udev_filter,
                        const std::string& dev_path)
      : UdevEventCallback(udev_filter, dev_path) {}

  MOCK_METHOD1(HandleUdevEvent, ::util::Status(const std::string& action));
};

class UdevEventHandlerMock : public UdevEventHandler {
 public:
  explicit UdevEventHandlerMock(SystemInterface* system_interface)
      : UdevEventHandler(system_interface) {}
  MOCK_METHOD1(RegisterEventCallback,
               ::util::Status(UdevEventCallback* callback));
  MOCK_METHOD1(UnregisterEventCallback,
               ::util::Status(UdevEventCallback* callback));
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_UDEV_EVENT_HANDLER_MOCK_H_
