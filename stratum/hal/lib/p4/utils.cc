// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


// Implementation of p4_utils functions.

#include "stratum/hal/lib/p4/utils.h"

#include "stratum/hal/lib/p4/p4_runtime_interface.h"
#include "absl/strings/substitute.h"

namespace stratum {
namespace hal {

using pi::proto::util::P4ResourceType;

// Decodes a P4 object ID into a human-readable form.
std::string PrintP4ObjectID(int object_id) {
  P4RuntimeInterface* p4_api = P4RuntimeInterface::instance();

  // In order to spare unit tests from mocking an interface they mostly
  // don't even know exists, this code assumes that if the runtime API
  // instance isn't present, then a test is running, and it uses the
  // special "TEST" resource name.  Tests that don't like this behavior
  // can set up a mock P4RuntimeInterface.
  // TODO: During the conversion to the new P4-16 compatible P4Info,
  // some deprecated P4-14 objects may appear as "INVALID".
  std::string resource_name = "INVALID";
  if (p4_api != nullptr) {
    P4ResourceType resource_type = p4_api->GetResourceTypeFromID(
        static_cast<pi::proto::util::p4_id_t>(object_id));
    switch (resource_type) {
      case P4ResourceType::INVALID:
      case P4ResourceType::INVALID_MAX:
        break;
      case P4ResourceType::ACTION:
        resource_name = "ACTION";
        break;
      case P4ResourceType::TABLE:
        resource_name = "TABLE";
        break;
      case P4ResourceType::ACTION_PROFILE:
        resource_name = "ACTION_PROFILE";
        break;
      case P4ResourceType::COUNTER:
        resource_name = "COUNTER";
        break;
      case P4ResourceType::METER:
        resource_name = "METER";
        break;
    }
  } else {
    resource_name = "TEST";
  }

  // TODO: Put a function into PI/proto/utils to do this conversion
  // and eliminate the hack below.
  int base_id = object_id & 0xffffff;
  return absl::Substitute("$0/$1 ($2)", resource_name.c_str(), base_id,
                          object_id);
}

}  // namespace hal
}  // namespace stratum
