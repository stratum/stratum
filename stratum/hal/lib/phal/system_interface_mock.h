// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_PHAL_SYSTEM_INTERFACE_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_SYSTEM_INTERFACE_MOCK_H_

#include <memory>
#include <string>

#include "stratum/hal/lib/phal/system_interface.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {
namespace phal {

class MockSystemInterface : public SystemInterface {
 public:
  MockSystemInterface() : SystemInterface() {}

  MOCK_CONST_METHOD2(WriteStringToFile,
                     ::util::Status(const std::string& buffer,
                                    const std::string& path));
  MOCK_CONST_METHOD2(ReadFileToString, ::util::Status(const std::string& path,
                                                      std::string* buffer));
  MOCK_CONST_METHOD1(PathExists, bool(const std::string& path));
  MOCK_CONST_METHOD0(MakeUdev, ::util::StatusOr<std::unique_ptr<Udev>>());
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_SYSTEM_INTERFACE_MOCK_H_
