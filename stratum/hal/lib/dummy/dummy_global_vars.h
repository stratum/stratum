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

#ifndef STRATUM_HAL_LIB_DUMMY_DUMMY_GLOBAL_VARS_H_
#define STRATUM_HAL_LIB_DUMMY_DUMMY_GLOBAL_VARS_H_

#include "absl/synchronization/mutex.h"

namespace stratum {

namespace hal {
namespace dummy_switch {

// Lock which governs chassis state (ports, etc.) across the entire switch.
extern absl::Mutex chassis_lock;

// Flag indicating if the switch has been shut down. Initialized to false.
extern bool shutdown;

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_DUMMY_DUMMY_GLOBAL_VARS_H_
