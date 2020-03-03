// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

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
