// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/glue/status/canonical_errors.h"

namespace util {

Status AbortedError(const std::string& message) {
  return Status(::absl::StatusCode::kAborted, message);
}

Status AlreadyExistsError(const std::string& message) {
  return Status(::absl::StatusCode::kAlreadyExists, message);
}

Status CancelledError(const std::string& message) {
  return Status(::absl::StatusCode::kCancelled, message);
}

Status DataLossError(const std::string& message) {
  return Status(::absl::StatusCode::kDataLoss, message);
}

Status DeadlineExceededError(const std::string& message) {
  return Status(::absl::StatusCode::kDeadlineExceeded, message);
}

Status FailedPreconditionError(const std::string& message) {
  return Status(::absl::StatusCode::kFailedPrecondition, message);
}

Status InternalError(const std::string& message) {
  return Status(::absl::StatusCode::kInternal, message);
}

Status InvalidArgumentError(const std::string& message) {
  return Status(::absl::StatusCode::kInvalidArgument, message);
}

Status NotFoundError(const std::string& message) {
  return Status(::absl::StatusCode::kNotFound, message);
}

Status OutOfRangeError(const std::string& message) {
  return Status(::absl::StatusCode::kOutOfRange, message);
}

Status PermissionDeniedError(const std::string& message) {
  return Status(::absl::StatusCode::kPermissionDenied, message);
}

Status UnauthenticatedError(const std::string& message) {
  return Status(::absl::StatusCode::kUnauthenticated, message);
}

Status ResourceExhaustedError(const std::string& message) {
  return Status(::absl::StatusCode::kResourceExhausted, message);
}

Status UnavailableError(const std::string& message) {
  return Status(::absl::StatusCode::kUnavailable, message);
}

Status UnimplementedError(const std::string& message) {
  return Status(::absl::StatusCode::kUnimplemented, message);
}

Status UnknownError(const std::string& message) {
  return Status(::absl::StatusCode::kUnknown, message);
}

bool IsAborted(const Status& status) {
  return status.Matches(::absl::StatusCode::kAborted);
}

bool IsAlreadyExists(const Status& status) {
  return status.Matches(::absl::StatusCode::kAlreadyExists);
}

bool IsCancelled(const Status& status) {
  return status.Matches(::absl::StatusCode::kCancelled);
}

bool IsDataLoss(const Status& status) {
  return status.Matches(::absl::StatusCode::kDataLoss);
}

bool IsDeadlineExceeded(const Status& status) {
  return status.Matches(::absl::StatusCode::kDeadlineExceeded);
}

bool IsFailedPrecondition(const Status& status) {
  return status.Matches(::absl::StatusCode::kFailedPrecondition);
}

bool IsInternal(const Status& status) {
  return status.Matches(::absl::StatusCode::kInternal);
}

bool IsInvalidArgument(const Status& status) {
  return status.Matches(::absl::StatusCode::kInvalidArgument);
}

bool IsNotFound(const Status& status) {
  return status.Matches(::absl::StatusCode::kNotFound);
}

bool IsOutOfRange(const Status& status) {
  return status.Matches(::absl::StatusCode::kOutOfRange);
}

bool IsPermissionDenied(const Status& status) {
  return status.Matches(::absl::StatusCode::kPermissionDenied);
}

bool IsUnauthenticated(const Status& status) {
  return status.Matches(::absl::StatusCode::kUnauthenticated);
}

bool IsResourceExhausted(const Status& status) {
  return status.Matches(::absl::StatusCode::kResourceExhausted);
}

bool IsUnavailable(const Status& status) {
  return status.Matches(::absl::StatusCode::kUnavailable);
}

bool IsUnimplemented(const Status& status) {
  return status.Matches(::absl::StatusCode::kUnimplemented);
}

bool IsUnknown(const Status& status) {
  return status.Matches(::absl::StatusCode::kUnknown);
}

}  // namespace util
