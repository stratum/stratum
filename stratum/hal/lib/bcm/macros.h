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


#ifndef STRATUM_HAL_LIB_BCM_MACROS_H_
#define STRATUM_HAL_LIB_BCM_MACROS_H_

extern "C" {
#include "shr/shr_error.h"
//#include "soc/error.h"
}

#include "stratum/glue/status/status.h"
#include "stratum/lib/macros.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {
namespace bcm {

// A simple class to explicitly cast the return value of an BCM API to bool.
// This is used in BCM_RET_CHECK Macro.
class BooleanBcmStatus {
 public:
  BooleanBcmStatus(int status) : status_(status) {}
  // Implicitly cast to bool.
  operator bool() const { return SHR_SUCCESS(status_); }
  // Return the actual value.
  inline int status() const { return status_; }
  inline ErrorCode error_code() const {
    switch (status_) {
      case SHR_E_NONE:
        return ERR_SUCCESS;
      case SHR_E_FULL:       // Table full
        return ERR_TABLE_FULL;
      case SHR_E_EMPTY:      // Table empty
        return ERR_TABLE_EMPTY;
      case SHR_E_UNAVAIL:    // Feature unavailable
        return ERR_FEATURE_UNAVAILABLE;
      case SHR_E_DISABLED:   // Operation disabled
        return ERR_OPER_DISABLED;
      case SHR_E_TIMEOUT:    // Operation timed out
        return ERR_OPER_TIMEOUT;
      case SHR_E_NOT_FOUND:  // Entry not found
        return ERR_ENTRY_NOT_FOUND;
      case SHR_E_EXISTS:     // Entry exists
        return ERR_ENTRY_EXISTS;
      case SHR_E_UNIT:       // Invalid unit
      case SHR_E_PARAM:      // Invalid parameter
      case SHR_E_BADID:      // Invalid identifier
      case SHR_E_PORT:       // Invalid port
        return ERR_INVALID_PARAM;
      case SHR_E_INIT:       // Feature not initialized
        return ERR_NOT_INITIALIZED;
      case SHR_E_MEMORY:     // Out of memory
      case SHR_E_RESOURCE:   // No resources for operation
        return ERR_NO_RESOURCE;
      case SHR_E_BUSY:       // Operation still running
        return ERR_OPER_STILL_RUNNING;
      case SHR_E_CONFIG:     // Invalid configuration
      case SHR_E_FAIL:       // Operation failed
      case SHR_E_INTERNAL:   // Internal error
        return ERR_INTERNAL;
      default:
        return ERR_UNKNOWN;
    }
  }

 private:
  int status_;
};

// A macro for simplify checking and logging the return value of a BCM function
// call.
#define RETURN_IF_BCM_ERROR(expr) \
  if (const BooleanBcmStatus __ret = (expr)) { \
  } else  /* NOLINT */ \
    return MAKE_ERROR(__ret.error_code()) \
           << "'" << #expr << "' failed with error message: " \
           << FixMessage(shr_errmsg(__ret.status()))

// A macro for simplify creating a new error or appending new info to an
// error based on the return value of a BCM function call. The caller function
// will not return. The variable given as "status" must be an object of type
// ::util::Status.
#define APPEND_STATUS_IF_BCM_ERROR(status, expr) \
  if (const BooleanBcmStatus __ret = (expr)) { \
  } else  /* NOLINT */ \
    status = APPEND_ERROR( \
        !status.ok() \
        ? status \
        : ::util::Status(StratumErrorSpace(), __ret.error_code(), "")) \
        .without_logging() \
        << (status.error_message().empty() || \
            status.error_message().back() == ' ' ? "" : " ") \
        << "'" << #expr << "' failed with error message: " \
        << FixMessage(shr_errmsg(__ret.status()))

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_MACROS_H_
