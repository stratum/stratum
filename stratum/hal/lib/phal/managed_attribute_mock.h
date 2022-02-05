// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_MANAGED_ATTRIBUTE_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_MANAGED_ATTRIBUTE_MOCK_H_

#include <memory>

#include "gmock/gmock.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/datasource.h"

namespace stratum {
namespace hal {
namespace phal {

class ManagedAttributeMock : public ManagedAttribute {
 public:
  MOCK_CONST_METHOD0(GetValue, Attribute());
  MOCK_CONST_METHOD0(GetDataSource, DataSource*());
  MOCK_CONST_METHOD0(CanSet, bool());
  MOCK_METHOD1(Set, ::util::Status(Attribute value));
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_MANAGED_ATTRIBUTE_MOCK_H_
