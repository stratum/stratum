/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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

#ifndef STRATUM_GLUE_STATUS_CANONICAL_ERRORS_H_
#define STRATUM_GLUE_STATUS_CANONICAL_ERRORS_H_

// This file declares a set of functions for working with Status objects from
// the canonical error space. There are functions to easily generate such
// status object and function for classifying them.
//

#include <string>

#include "stratum/glue/status/status.h"

namespace util {

// Each of the functions below creates a canonical error with the given
// message. The error code of the returned status object matches the name of
// the function.
Status AbortedError(const std::string& message);
Status AlreadyExistsError(const std::string& message);
Status CancelledError(const std::string& message);
Status DataLossError(const std::string& message);
Status DeadlineExceededError(const std::string& message);
Status FailedPreconditionError(const std::string& message);
Status InternalError(const std::string& message);
Status InvalidArgumentError(const std::string& message);
Status NotFoundError(const std::string& message);
Status OutOfRangeError(const std::string& message);
Status PermissionDeniedError(const std::string& message);
Status UnauthenticatedError(const std::string& message);
Status ResourceExhaustedError(const std::string& message);
Status UnavailableError(const std::string& message);
Status UnimplementedError(const std::string& message);
Status UnknownError(const std::string& message);

// Each of the functions below returns true if the given status matches the
// canonical error code implied by the function's name. If necessary, the
// status will be converted to the canonical error space to perform the
// comparison.
bool IsAborted(const Status& status);
bool IsAlreadyExists(const Status& status);
bool IsCancelled(const Status& status);
bool IsDataLoss(const Status& status);
bool IsDeadlineExceeded(const Status& status);
bool IsFailedPrecondition(const Status& status);
bool IsInternal(const Status& status);
bool IsInvalidArgument(const Status& status);
bool IsNotFound(const Status& status);
bool IsOutOfRange(const Status& status);
bool IsPermissionDenied(const Status& status);
bool IsUnauthenticated(const Status& status);
bool IsResourceExhausted(const Status& status);
bool IsUnavailable(const Status& status);
bool IsUnimplemented(const Status& status);
bool IsUnknown(const Status& status);

}  // namespace util

#endif  // STRATUM_GLUE_STATUS_CANONICAL_ERRORS_H_
