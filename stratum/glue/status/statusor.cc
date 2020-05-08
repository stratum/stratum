// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/glue/status/statusor.h"

#include <errno.h>
#include <algorithm>

#include "stratum/glue/logging.h"
#include "stratum/glue/status/posix_error_space.h"

namespace util {
namespace internal {

::util::Status StatusOrHelper::HandleInvalidStatusCtorArg() {
  const char* kMessage =
      "Status::OK is not a valid constructor argument to StatusOr<T>";
  LOG(DFATAL) << kMessage;
  // In optimized builds, we will fall back on an EINVAL status.
  // TODO(unknown): Change this to ::util::error::INVALID_ARGUMENT.
  return ::util::PosixErrorToStatus(EINVAL, kMessage);
}

::util::Status StatusOrHelper::HandleNullObjectCtorArg() {
  const char* kMessage =
      "NULL is not a valid constructor argument to StatusOr<T*>";
  LOG(DFATAL) << kMessage;
  // In optimized builds, we will fall back on an EINVAL status.
  // TODO(unknown): Change this to ::util::error::INVALID_ARGUMENT.
  return ::util::PosixErrorToStatus(EINVAL, kMessage);
}

void StatusOrHelper::Crash(const ::util::Status& status) {
  LOG(FATAL) << "Attempting to fetch value instead of handling error "
             << status;
}

}  // namespace internal
}  // namespace util
