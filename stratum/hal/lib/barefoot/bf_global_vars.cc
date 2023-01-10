// Copyright 2023-present Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_global_vars.h"

#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace barefoot {

ABSL_CONST_INIT absl::Mutex chassis_lock(absl::kConstInit);
bool shutdown = false;

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
