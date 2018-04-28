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


// This file implements P4RuntimeReal.

#include "third_party/stratum/hal/lib/p4/p4_runtime_real.h"

namespace stratum {
namespace hal {

pi::proto::util::P4ResourceType P4RuntimeReal::GetResourceTypeFromID(
    pi::proto::util::p4_id_t object_id) {
  return pi::proto::util::resource_type_from_id(object_id);
}

P4RuntimeInterface* P4RuntimeReal::GetSingleton() {
  P4RuntimeInterface* singleton = P4RuntimeInterface::instance();
  if (!singleton) {
    singleton = new P4RuntimeReal;
    P4RuntimeInterface::set_instance(singleton);
  }

  return singleton;
}

}  // namespace hal
}  // namespace stratum
