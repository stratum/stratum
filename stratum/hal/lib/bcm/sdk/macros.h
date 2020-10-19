// Copyright 2018-2019 Google LLC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

/*
 * The Broadcom Switch API header code upon which this file depends is:
 * Copyright 2007-2020 Broadcom Inc.
 *
 * This file depends on Broadcom's OpenNSA SDK.
 * Additional license terms for OpenNSA are available from Broadcom or online:
 *     https://www.broadcom.com/products/ethernet-connectivity/software/opennsa
 */

#ifndef STRATUM_HAL_LIB_BCM_SDK_MACROS_H_
#define STRATUM_HAL_LIB_BCM_SDK_MACROS_H_

extern "C" {
#include "bcm/error.h"
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
  explicit BooleanBcmStatus(int status) : status_(status) {}
  // Implicitly cast to bool.
  operator bool() const { return BCM_SUCCESS(status_); }
  // Return the actual value.
  inline int status() const { return status_; }
  inline ErrorCode error_code() const {
    switch (status_) {
      case BCM_E_NONE:
        return ERR_SUCCESS;
      case BCM_E_FULL:  // Table full
        return ERR_TABLE_FULL;
      case BCM_E_EMPTY:  // Table empty
        return ERR_TABLE_EMPTY;
      case BCM_E_UNAVAIL:  // Feature unavailable
        return ERR_FEATURE_UNAVAILABLE;
      case BCM_E_DISABLED:  // Operation disabled
        return ERR_OPER_DISABLED;
      case BCM_E_TIMEOUT:  // Operation timed out
        return ERR_OPER_TIMEOUT;
      case BCM_E_NOT_FOUND:  // Entry not found
        return ERR_ENTRY_NOT_FOUND;
      case BCM_E_EXISTS:  // Entry exists
        return ERR_ENTRY_EXISTS;
      case BCM_E_UNIT:   // Invalid unit
      case BCM_E_PARAM:  // Invalid parameter
      case BCM_E_BADID:  // Invalid identifier
      case BCM_E_PORT:   // Invalid port
        return ERR_INVALID_PARAM;
      case BCM_E_INIT:  // Feature not initialized
        return ERR_NOT_INITIALIZED;
      case BCM_E_MEMORY:    // Out of memory
      case BCM_E_RESOURCE:  // No resources for operation
        return ERR_NO_RESOURCE;
      case BCM_E_BUSY:  // Operation still running
        return ERR_OPER_STILL_RUNNING;
      case BCM_E_CONFIG:    // Invalid configuration
      case BCM_E_FAIL:      // Operation failed
      case BCM_E_INTERNAL:  // Internal error
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
#define RETURN_IF_BCM_ERROR(expr)                              \
  if (const BooleanBcmStatus __ret = BooleanBcmStatus(expr)) { \
  } else /* NOLINT */                                          \
    return MAKE_ERROR(__ret.error_code())                      \
           << "'" << #expr << "' failed with error message: "  \
           << FixMessage(bcm_errmsg(__ret.status()))

// A macro for simplify creating a new error or appending new info to an
// error based on the return value of a BCM function call. The caller function
// will not return. The variable given as "status" must be an object of type
// ::util::Status.
#define APPEND_STATUS_IF_BCM_ERROR(status, expr)                            \
  if (const BooleanBcmStatus __ret = BooleanBcmStatus(expr)) {              \
  } else /* NOLINT */                                                       \
    status =                                                                \
        APPEND_ERROR(!status.ok() ? status                                  \
                                  : ::util::Status(StratumErrorSpace(),     \
                                                   __ret.error_code(), "")) \
            .without_logging()                                              \
        << (status.error_message().empty() ||                               \
                    status.error_message().back() == ' '                    \
                ? ""                                                        \
                : " ")                                                      \
        << "'" << #expr << "' failed with error message: "                  \
        << FixMessage(bcm_errmsg(__ret.status()))

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_SDK_MACROS_H_
