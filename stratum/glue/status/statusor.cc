/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "third_party/stratum/glue/status/statusor.h"

#include <errno.h>

#include "third_party/stratum/glue/logging.h"
#include "third_party/stratum/glue/status/posix_error_space.h"

namespace util {
namespace internal {

::util::Status StatusOrHelper::HandleInvalidStatusCtorArg() {
  const char* kMessage =
      "Status::OK is not a valid constructor argument to StatusOr<T>";
  LOG(DFATAL) << kMessage;
  // In optimized builds, we will fall back on an EINVAL status.
  // TODO: Change this to ::util::error::INVALID_ARGUMENT.
  return ::util::PosixErrorToStatus(EINVAL, kMessage);
}

::util::Status StatusOrHelper::HandleNullObjectCtorArg() {
  const char* kMessage =
      "NULL is not a valid constructor argument to StatusOr<T*>";
  LOG(DFATAL) << kMessage;
  // In optimized builds, we will fall back on an EINVAL status.
  // TODO: Change this to ::util::error::INVALID_ARGUMENT.
  return ::util::PosixErrorToStatus(EINVAL, kMessage);
}

void StatusOrHelper::Crash(const ::util::Status& status) {
  LOG(FATAL) << "Attempting to fetch value instead of handling error "
             << status;
}

}  // namespace internal
}  // namespace util
