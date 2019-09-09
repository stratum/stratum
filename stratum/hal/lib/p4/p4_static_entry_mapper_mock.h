/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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


// This is a mock implementation of P4StaticEntryMapper.

#ifndef STRATUM_HAL_LIB_P4_P4_STATIC_ENTRY_MAPPER_MOCK_H_
#define STRATUM_HAL_LIB_P4_P4_STATIC_ENTRY_MAPPER_MOCK_H_

#include "stratum/hal/lib/p4/p4_static_entry_mapper.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {

class P4StaticEntryMapperMock : public P4StaticEntryMapper {
 public:
  MOCK_METHOD2(HandlePrePushChanges,
               ::util::Status(const ::p4::v1::WriteRequest& new_static_config,
                              ::p4::v1::WriteRequest* out_request));
  MOCK_METHOD2(HandlePostPushChanges,
               ::util::Status(const ::p4::v1::WriteRequest& new_static_config,
                              ::p4::v1::WriteRequest* out_request));
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_STATIC_ENTRY_MAPPER_MOCK_H_
