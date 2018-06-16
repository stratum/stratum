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


// This is a mock implementation of a P4RuntimeInterface.

#ifndef STRATUM_HAL_LIB_P4_P4_RUNTIME_MOCK_H_
#define STRATUM_HAL_LIB_P4_P4_RUNTIME_MOCK_H_

#include "stratum/hal/lib/p4/p4_runtime_interface.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {

class P4RuntimeMock : public P4RuntimeInterface {
 public:
  MOCK_METHOD1(GetResourceTypeFromID, p4::config::v1::P4Ids::Prefix(
      pi::proto::util::p4_id_t object_id));
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_RUNTIME_MOCK_H_
