// Copyright 2023-present Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_GLOBAL_VARS_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_GLOBAL_VARS_H_

#include "absl/synchronization/mutex.h"

namespace stratum {

namespace hal {
namespace barefoot {

// Lock which governs chassis state (ports, etc.) across the entire switch.
extern absl::Mutex chassis_lock;

// Flag indicating if the switch has been shut down. Initialized to false.
extern bool shutdown;

}  // namespace barefoot
}  // namespace hal

}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_GLOBAL_VARS_H_
