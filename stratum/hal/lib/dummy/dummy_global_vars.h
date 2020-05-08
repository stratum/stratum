// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

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
