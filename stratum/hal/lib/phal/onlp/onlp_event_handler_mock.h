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


#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_EVENT_HANDLER_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_EVENT_HANDLER_MOCK_H_

#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
//#include "stratum/hal/lib/phal/onlp/onlp_wrapper_fake.h"
#include "stratum/hal/lib/phal/onlp/onlp_event_handler.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class OnlpSfpEventCallbackMock : public OnlpSfpEventCallback {
 public:
  OnlpSfpEventCallbackMock() {}
  MOCK_METHOD1(HandleSfpStatusChange, ::util::Status(const OidInfo&));
};

class OnlpEventHandlerMock : public OnlpEventHandler {
 public:
  explicit OnlpEventHandlerMock(const OnlpInterface* onlp)
      : OnlpEventHandler(onlp) {}

  MOCK_METHOD1(RegisterOidEventCallback,
               ::util::Status(OnlpOidEventCallback* callback));
  MOCK_METHOD1(UnregisterOidEventCallback,
               ::util::Status(OnlpOidEventCallback* callback));
  MOCK_METHOD1(RegisterSfpEventCallback,
               ::util::Status(OnlpSfpEventCallback* callback));
  MOCK_METHOD1(UnregisterSfpEventCallback,
               ::util::Status(OnlpSfpEventCallback* callback));

};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_EVENT_HANDLER_MOCK_H_

