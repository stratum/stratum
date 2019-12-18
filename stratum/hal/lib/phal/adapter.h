/*
 * Copyright 2019 Dell EMC
 * Copyright 2019-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_PHAL_ADAPTER_H_
#define STRATUM_HAL_LIB_PHAL_ADAPTER_H_

#include <memory>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/managed_attribute.h"

namespace stratum {
namespace hal {
namespace phal {

class Adapter {
 public:
  explicit Adapter(AttributeDatabaseInterface* attribute_db_interface)
      : attribute_db_interface_(attribute_db_interface) {}

  virtual ~Adapter() = default;

  ::util::StatusOr<std::unique_ptr<PhalDB>> Get(const std::vector<Path>& paths);

  ::util::Status Subscribe(const std::vector<Path>& paths,
                           std::unique_ptr<ChannelWriter<PhalDB>> writer,
                           absl::Duration poll_time);

  ::util::Status Set(const AttributeValueMap& values);

  AttributeDatabaseInterface*
      attribute_db_interface_;       // not owned by this class
  std::unique_ptr<Query> db_query_;  // used for subscribe requests
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ADAPTER_H_
