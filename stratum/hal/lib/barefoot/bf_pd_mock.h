// Copyright 2020-present Open Networking Founcation
// SPDX-License-Identifier: Apache-2.0

#include "gmock/gmock.h"
#include "stratum/hal/lib/barefoot/bf_pd_interface.h"

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_PD_MOCK_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_PD_MOCK_H_

namespace stratum {
namespace hal {
namespace barefoot {

class BFPdMock : public BFPdInterface {
 public:
  MOCK_METHOD1(GetPcieCpuPort, ::util::StatusOr<int>(int unit));
  MOCK_METHOD2(SetTmCpuPort, ::util::Status(int unit, int port));
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_PD_MOCK_H_
