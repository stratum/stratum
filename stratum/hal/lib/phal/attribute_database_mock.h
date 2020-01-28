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

#ifndef STRATUM_HAL_LIB_PHAL_ATTRIBUTE_DATABASE_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_ATTRIBUTE_DATABASE_MOCK_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "gmock/gmock.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"

namespace stratum {
namespace hal {
namespace phal {

class AttributeDatabaseMock : public AttributeDatabaseInterface {
 public:
  MOCK_METHOD1(Set, ::util::Status(const AttributeValueMap& values));
  MOCK_METHOD1(MakeQuery, ::util::StatusOr<std::unique_ptr<Query>>(
                              const std::vector<Path>& query_paths));
};

class QueryMock : public Query {
 public:
  MOCK_METHOD0(Get, ::util::StatusOr<std::unique_ptr<PhalDB>>());
  MOCK_METHOD2(Subscribe,
               ::util::Status(std::unique_ptr<ChannelWriter<PhalDB>> subscriber,
                              absl::Duration polling_interval));
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ATTRIBUTE_DATABASE_MOCK_H_
