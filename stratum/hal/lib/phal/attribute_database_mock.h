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
#include <utility>

#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "gmock/gmock.h"
#include "absl/container/flat_hash_map.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {

class AttributeDatabaseMock : public AttributeDatabaseInterface {
 public:
  MOCK_METHOD1(
      Set, ::util::Status(const AttributeValueMap& values));

  MOCK_METHOD1(DoMakeQuery, std::unique_ptr<Query>
          (const std::vector<Path>& query_paths));

  // Mock can't seem to handle the StatusOr macro returning
  // a unique pointer so we need this extra step.
  ::util::StatusOr<std::unique_ptr<Query>> MakeQuery(
    const std::vector<Path>& query_paths) override {
        return std::move(DoMakeQuery(query_paths));
  };
};

class QueryMock : public Query {
 public:
  MOCK_METHOD0(DoGet, ::util::StatusOr<std::unique_ptr<PhalDB>>());

  // Mock can't seem to handle the StatusOr macro returning
  // a unique pointer so we need this extra step.
  ::util::StatusOr<std::unique_ptr<PhalDB>> Get() override {
        return std::move(DoGet());
  };

  MOCK_METHOD1(DoSubscribe,
               ::util::StatusOr<PhalDB*>(absl::Duration polling_interval));

  // We'll override the Subscribe with a function that grabs the
  // response from the Mock function and sends it on the channel.
  ::util::Status Subscribe(std::unique_ptr<ChannelWriter<PhalDB>> writer,
                           absl::Duration polling_interval) override {
      // Grab response from mock
      ASSIGN_OR_RETURN(auto resp, DoSubscribe(polling_interval));

      // Send the response
      writer->TryWrite(*resp);

      // Now send a zero length response to cause
      // the reader to close the channel.
      writer->TryWrite(PhalDB());

      return ::util::OkStatus();
  }
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ATTRIBUTE_DATABASE_MOCK_H_
