#ifndef PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_GNMI_PUBLISHER_MOCK_H_
#define PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_GNMI_PUBLISHER_MOCK_H_

#include "platforms/networking/hercules/hal/lib/common/gnmi_publisher.h"
#include "platforms/networking/hercules/hal/lib/common/switch_mock.h"
#include "testing/base/public/gmock.h"

namespace google {
namespace hercules {
namespace hal {

// A mock class for GnmiPublisher class.
class GnmiPublisherMock : public GnmiPublisher {
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

  explicit GnmiPublisherMock(SwitchInterface *switch_interface)
      : GnmiPublisher(switch_interface), switch_interface_(switch_interface) {}

 private:
  SwitchInterface *switch_interface_;
};

}  // namespace hal
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_GNMI_PUBLISHER_MOCK_H_
