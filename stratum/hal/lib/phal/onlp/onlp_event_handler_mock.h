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


#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_EVENT_HANDLER_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_EVENT_HANDLER_MOCK_H_

#include "stratum/hal/lib/phal/onlp/onlp_event_handler.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class OnlpEventCallbackMock : public OnlpEventCallback {
 public:
  explicit OnlpEventCallbackMock(OnlpOid oid) : OnlpEventCallback(oid) {}
  MOCK_METHOD1(HandleStatusChange, ::util::Status(const OidInfo&));
  MOCK_METHOD1(HandleOidStatusChange, ::util::Status(const OidInfo&));
};

class OnlpEventHandlerMock : public OnlpEventHandler {
 public:
  explicit OnlpEventHandlerMock(const OnlpInterface* onlp)
      : OnlpEventHandler(onlp) {}

  MOCK_METHOD1(RegisterEventCallback,
               ::util::Status(OnlpEventCallback* callback));
  MOCK_METHOD1(UnregisterEventCallback,
               ::util::Status(OnlpEventCallback* callback));
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_EVENT_HANDLER_MOCK_H_

