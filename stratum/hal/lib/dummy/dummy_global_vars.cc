// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/dummy/dummy_global_vars.h"
#include "absl/synchronization/mutex.h"

namespace stratum {

namespace hal {
namespace dummy_switch {

ABSL_CONST_INIT absl::Mutex chassis_lock(absl::kConstInit);
bool shutdown = false;

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum
