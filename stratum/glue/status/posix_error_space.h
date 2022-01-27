// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// This package defines the POSIX error space (think 'errno'
// values). Given a (stable) errno value, this class can be used to
// translate that value to a string description by calling the
// 'String' method below.
//
//  e.g.: cout << PosixErrorSpace()->String(ENOSYS);
//
// As a convenience, this package provides a static ToStatus routine
// which returns a Status object referring to this error space
// with the given code and message.
//
//  e.g.: return PosixErrorToStatus(ENOSYS, "Not Implemented");
//
// Calls to PosixErrorToStatus where 'code' is zero will be short
// circuited by the implementation of Status to be equivalent to
// Status::OK, ignoring this error space and the provided message.
//

#ifndef STRATUM_GLUE_STATUS_POSIX_ERROR_SPACE_H_
#define STRATUM_GLUE_STATUS_POSIX_ERROR_SPACE_H_

#include <string>

#include "stratum/glue/status/status.h"

namespace util {

const ErrorSpace* PosixErrorSpace();

inline Status PosixErrorToStatus(int code, const std::string& message) {
  return Status(PosixErrorSpace(), code, message);
}

}  // namespace util

#endif  // STRATUM_GLUE_STATUS_POSIX_ERROR_SPACE_H_
