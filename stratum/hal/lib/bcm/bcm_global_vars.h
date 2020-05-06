// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BCM_BCM_GLOBAL_VARS_H_
#define STRATUM_HAL_LIB_BCM_BCM_GLOBAL_VARS_H_

#include "absl/synchronization/mutex.h"

namespace stratum {

namespace hal {
namespace bcm {

// Lock which governs chassis state (ports, etc.) across the entire switch.
extern absl::Mutex chassis_lock;

// Flag indicating if the switch has been shut down. Initialized to false.
extern bool shutdown;

}  // namespace bcm
}  // namespace hal

}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_GLOBAL_VARS_H_
