// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/glue/status/canonical_errors.h"

namespace util {

Status AbortedError(const std::string& message) {
  return Status(error::ABORTED, message);
}

Status AlreadyExistsError(const std::string& message) {
  return Status(error::ALREADY_EXISTS, message);
}

Status CancelledError(const std::string& message) {
  return Status(error::CANCELLED, message);
}

Status DataLossError(const std::string& message) {
  return Status(error::DATA_LOSS, message);
}

Status DeadlineExceededError(const std::string& message) {
  return Status(error::DEADLINE_EXCEEDED, message);
}

Status FailedPreconditionError(const std::string& message) {
  return Status(error::FAILED_PRECONDITION, message);
}

Status InternalError(const std::string& message) {
  return Status(error::INTERNAL, message);
}

Status InvalidArgumentError(const std::string& message) {
  return Status(error::INVALID_ARGUMENT, message);
}

Status NotFoundError(const std::string& message) {
  return Status(error::NOT_FOUND, message);
}

Status OutOfRangeError(const std::string& message) {
  return Status(error::OUT_OF_RANGE, message);
}

Status PermissionDeniedError(const std::string& message) {
  return Status(error::PERMISSION_DENIED, message);
}

Status UnauthenticatedError(const std::string& message) {
  return Status(error::UNAUTHENTICATED, message);
}

Status ResourceExhaustedError(const std::string& message) {
  return Status(error::RESOURCE_EXHAUSTED, message);
}

Status UnavailableError(const std::string& message) {
  return Status(error::UNAVAILABLE, message);
}

Status UnimplementedError(const std::string& message) {
  return Status(error::UNIMPLEMENTED, message);
}

Status UnknownError(const std::string& message) {
  return Status(error::UNKNOWN, message);
}

bool IsAborted(const Status& status) {
  return status.Matches(error::ABORTED);
}

bool IsAlreadyExists(const Status& status) {
  return status.Matches(error::ALREADY_EXISTS);
}

bool IsCancelled(const Status& status) {
  return status.Matches(error::CANCELLED);
}

bool IsDataLoss(const Status& status) {
  return status.Matches(error::DATA_LOSS);
}

bool IsDeadlineExceeded(const Status& status) {
  return status.Matches(error::DEADLINE_EXCEEDED);
}

bool IsFailedPrecondition(const Status& status) {
  return status.Matches(error::FAILED_PRECONDITION);
}

bool IsInternal(const Status& status) {
  return status.Matches(error::INTERNAL);
}

bool IsInvalidArgument(const Status& status) {
  return status.Matches(error::INVALID_ARGUMENT);
}

bool IsNotFound(const Status& status) {
  return status.Matches(error::NOT_FOUND);
}

bool IsOutOfRange(const Status& status) {
  return status.Matches(error::OUT_OF_RANGE);
}

bool IsPermissionDenied(const Status& status) {
  return status.Matches(error::PERMISSION_DENIED);
}

bool IsUnauthenticated(const Status& status) {
  return status.Matches(error::UNAUTHENTICATED);
}

bool IsResourceExhausted(const Status& status) {
  return status.Matches(error::RESOURCE_EXHAUSTED);
}

bool IsUnavailable(const Status& status) {
  return status.Matches(error::UNAVAILABLE);
}

bool IsUnimplemented(const Status& status) {
  return status.Matches(error::UNIMPLEMENTED);
}

bool IsUnknown(const Status& status) {
  return status.Matches(error::UNKNOWN);
}

}  // namespace util
