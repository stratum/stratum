// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_UDEV_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_UDEV_MOCK_H_

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "stratum/hal/lib/phal/udev_interface.h"

namespace stratum {
namespace hal {

class UdevMock : public UdevInterface {
 public:
  MOCK_METHOD1(Initialize, ::util::Status(const std::string& filter));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_METHOD0(Check, ::util::StatusOr<std::pair<std::string, std::string>>());
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_UDEV_MOCK_H_
