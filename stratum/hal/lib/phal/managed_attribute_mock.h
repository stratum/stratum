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


#ifndef STRATUM_HAL_LIB_PHAL_MANAGED_ATTRIBUTE_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_MANAGED_ATTRIBUTE_MOCK_H_

#include <memory>

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "gmock/gmock.h"

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
