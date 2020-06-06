// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_MACROS_H_
#define STRATUM_HAL_LIB_BAREFOOT_MACROS_H_

extern "C" {
#include "bf_rt/bf_rt_common.h"
}

#include "stratum/glue/status/status.h"
#include "stratum/lib/macros.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BooleanBfStatus {
 public:
  explicit BooleanBfStatus(bf_status_t status) : status_(status) {}
  operator bool() const { return status_ == BF_SUCCESS; }
  inline bf_status_t status() const { return status_; }
  inline ErrorCode error_code() const {
    switch (status_) {
      case BF_SUCCESS:
        return ERR_SUCCESS;
      case BF_NOT_READY:
        return ERR_NOT_INITIALIZED;
      case BF_INVALID_ARG:
        return ERR_INVALID_PARAM;
      case BF_ALREADY_EXISTS:
        return ERR_ENTRY_EXISTS;
      case BF_NO_SYS_RESOURCES:
      case BF_MAX_SESSIONS_EXCEEDED:
      case BF_NO_SPACE:
      case BF_EAGAIN:
        return ERR_NO_RESOURCE;
      case BF_ENTRY_REFERENCES_EXIST:
        return ERR_PERMISSION_DENIED;
      case BF_TXN_NOT_SUPPORTED:
      case BF_NOT_SUPPORTED:
        return ERR_OPER_NOT_SUPPORTED;
      case BF_HW_COMM_FAIL:
      case BF_HW_UPDATE_FAILED:
        return ERR_HARDWARE_ERROR;
      case BF_NO_LEARN_CLIENTS:
        return ERR_FEATURE_UNAVAILABLE;
      case BF_IDLE_UPDATE_IN_PROGRESS:
        return ERR_OPER_STILL_RUNNING;
      case BF_OBJECT_NOT_FOUND:
      case BF_TABLE_NOT_FOUND:
        return ERR_ENTRY_NOT_FOUND;
      case BF_NOT_IMPLEMENTED:
        return ERR_UNIMPLEMENTED;
      case BF_SESSION_NOT_FOUND:
      case BF_INIT_ERROR:
      case BF_TABLE_LOCKED:
      case BF_IO:
      case BF_UNEXPECTED:
      case BF_DEVICE_LOCKED:
      case BF_INTERNAL_ERROR:
      case BF_IN_USE:
      default:
        return ERR_INTERNAL;
    }
  }

 private:
  bf_status_t status_;
};

// TODO: Rename to RETURN_IF_BFRT_ERROR
#define BFRT_RETURN_IF_ERROR(expr)                            \
  if (const BooleanBfStatus __ret = BooleanBfStatus(expr)) {  \
  } else /* NOLINT */                                         \
    return MAKE_ERROR(__ret.error_code())                     \
           << "'" << #expr << "' failed with error message: " \
           << FixMessage(bf_err_str(__ret.status()))

#define RETURN_IF_BFRT_ERROR BFRT_RETURN_IF_ERROR

#endif  // STRATUM_HAL_LIB_BAREFOOT_MACROS_H_

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
