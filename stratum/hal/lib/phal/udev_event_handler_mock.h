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


#ifndef STRATUM_HAL_LIB_PHAL_UDEV_EVENT_HANDLER_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_UDEV_EVENT_HANDLER_MOCK_H_

#include "stratum/hal/lib/phal/udev_event_handler.h"
#include "gmock/gmock.h"

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
