// Copyright 2018-present Open Networking Foundation
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

#include "stratum/hal/lib/dummy/dummy_global_vars.h"
#include "absl/synchronization/mutex.h"

namespace stratum {

namespace hal {
namespace dummy_switch {

absl::Mutex chassis_lock;
bool shutdown = false;

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum
