// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_EVENT_HANDLER_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_EVENT_HANDLER_MOCK_H_

#include "gmock/gmock.h"
#include "stratum/hal/lib/phal/onlp/onlp_event_handler.h"

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
