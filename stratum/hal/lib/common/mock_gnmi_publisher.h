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


#ifndef STRATUM_HAL_LIB_COMMON_MOCK_GNMI_PUBLISHER_H_
#define STRATUM_HAL_LIB_COMMON_MOCK_GNMI_PUBLISHER_H_

#include "third_party/stratum/hal/lib/common/gnmi_publisher.h"
#include "third_party/stratum/hal/lib/common/switch_mock.h"
#include "testing/base/public/gmock.h"

namespace stratum {
namespace hal {

class MockGnmiPublisher : public GnmiPublisher {
 public:
  MOCK_METHOD4(SubscribePeriodic,
               ::util::Status(const Frequency &, const ::gnmi::Path &,
                              GnmiSubscribeStream *, SubscriptionHandle *));

  MOCK_METHOD3(SubscribePoll,
               ::util::Status(const ::gnmi::Path &, GnmiSubscribeStream *,
                              SubscriptionHandle *));
  MOCK_METHOD3(SubscribeOnChange,
               ::util::Status(const ::gnmi::Path &, GnmiSubscribeStream *,
                              SubscriptionHandle *));

  MOCK_METHOD1(UnSubscribe, ::util::Status(EventHandlerRecord* h));

  MOCK_METHOD1(HandlePoll, ::util::Status(const SubscriptionHandle &));

  MOCK_METHOD2(UpdateSubscriptionWithTargetSpecificModeSpecification,
               ::util::Status(const ::gnmi::Path &path,
                              ::gnmi::Subscription *subscription));

  explicit MockGnmiPublisher(SwitchInterface *switch_interface)
      : GnmiPublisher(switch_interface), switch_interface_(switch_interface) {}

 private:
  SwitchInterface *switch_interface_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_MOCK_GNMI_PUBLISHER_H_
