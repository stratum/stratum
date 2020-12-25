// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/public/lib/error.h"
#include "absl/status/status.h"

#include <string>

namespace stratum {

namespace error {

static const char kErrorSpaceName[] = "StratumErrorSpace";

class StratumErrorSpace : public ::util::ErrorSpace {
 public:
  StratumErrorSpace() : ::util::ErrorSpace(kErrorSpaceName) {}
  ~StratumErrorSpace() override {}

  ::std::string String(int code) const final {
    if (!ErrorCode_IsValid(code)) {
      code = ERR_UNKNOWN;
    }
    return ErrorCode_Name(static_cast<ErrorCode>(code));
  }

  ::absl::StatusCode CanonicalCode(const ::util::Status& status) const final {
    switch (status.error_code()) {
      case ERR_SUCCESS:
        return ::absl::StatusCode::kOk;
      case ERR_CANCELLED:
        return ::absl::StatusCode::kCancelled;
      case ERR_UNKNOWN:
        return ::absl::StatusCode::kUnknown;
      case ERR_PERMISSION_DENIED:
        return ::absl::StatusCode::kPermissionDenied;
      case ERR_ABORTED:
        return ::absl::StatusCode::kAborted;
      case ERR_DATA_LOSS:
        return ::absl::StatusCode::kDataLoss;
      case ERR_UNAUTHENTICATED:
        return ::absl::StatusCode::kUnauthenticated;
      case ERR_INTERNAL:
      case ERR_HARDWARE_ERROR:
        return ::absl::StatusCode::kInternal;
      case ERR_INVALID_PARAM:
      case ERR_INVALID_P4_INFO:
        return ::absl::StatusCode::kInvalidArgument;
      case ERR_OPER_TIMEOUT:
        return ::absl::StatusCode::kDeadlineExceeded;
      case ERR_ENTRY_NOT_FOUND:
        return ::absl::StatusCode::kNotFound;
      case ERR_ENTRY_EXISTS:
        return ::absl::StatusCode::kAlreadyExists;
      case ERR_UNIMPLEMENTED:
      case ERR_OPER_NOT_SUPPORTED:
      case ERR_OPER_DISABLED:
        return ::absl::StatusCode::kUnimplemented;
      case ERR_FEATURE_UNAVAILABLE:
        return ::absl::StatusCode::kUnavailable;
      case ERR_NO_RESOURCE:
        return ::absl::StatusCode::kResourceExhausted;
      case ERR_FAILED_PRECONDITION:
      case ERR_NOT_INITIALIZED:
        return ::absl::StatusCode::kFailedPrecondition;
      case ERR_OUT_OF_RANGE:
      case ERR_TABLE_FULL:
      case ERR_TABLE_EMPTY:
        return ::absl::StatusCode::kOutOfRange;
      default:
        return ::absl::StatusCode::kUnknown;  // Default error.
    }
  }

  // StratumErrorSpace is neither copyable nor movable.
  StratumErrorSpace(const StratumErrorSpace&) = delete;
  StratumErrorSpace& operator=(const StratumErrorSpace&) = delete;
};

}  // namespace error

// Singleton StratumErrorSpace.
const ::util::ErrorSpace* StratumErrorSpace() {
  static const ::util::ErrorSpace* space = new error::StratumErrorSpace();
  return space;
}

// Force registering of the errorspace at run-time.
static const ::util::ErrorSpace* dummy __attribute__((unused)) =
    StratumErrorSpace();

}  // namespace stratum
