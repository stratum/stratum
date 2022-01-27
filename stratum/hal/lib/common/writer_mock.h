// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_WRITER_MOCK_H_
#define STRATUM_HAL_LIB_COMMON_WRITER_MOCK_H_

#include "gmock/gmock.h"
#include "stratum/hal/lib/common/writer_interface.h"

namespace stratum {
namespace hal {

// Definition of the mock version of WriterInterface for unit testing.
template <typename T>
class WriterMock : public WriterInterface<T> {
 public:
  MOCK_METHOD1_T(Write, bool(const T& msg));
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_WRITER_MOCK_H_
