// Copyright 2019 Dell EMC
// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_PHAL_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_PHAL_MOCK_H_

#include <memory>

#include "stratum/hal/lib/phal/onlp/onlp_phal_interface.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class OnlpPhalMock : public OnlpPhalInterface {
 public:
  MOCK_METHOD1(PushChassisConfig, ::util::Status(const ChassisConfig& config));
  MOCK_METHOD1(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_METHOD1(RegisterOnlpEventCallback,
               ::util::Status(OnlpEventCallback* callback));
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_PHAL_MOCK_H_
