// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_PUBLIC_LIB_ERROR_H_
#define STRATUM_PUBLIC_LIB_ERROR_H_

#include "stratum/glue/status/status_macros.h"
#include "stratum/public/proto/error.pb.h"

namespace stratum {

// StratumErrorSpace returns the singleton instance to be used through
// out the code.
const ::util::ErrorSpace* StratumErrorSpace();

}  // namespace stratum

// Allow using status_macros. For example:
// return MAKE_ERROR(ERR_UNKNOWN) << "test";
namespace util {
namespace status_macros {

template <>
class ErrorCodeOptions<::stratum::ErrorCode> : public BaseErrorCodeOptions {
 public:
  const ::util::ErrorSpace* GetErrorSpace() {
    return ::stratum::StratumErrorSpace();
  }
};

}  // namespace status_macros
}  // namespace util

#endif  // STRATUM_PUBLIC_LIB_ERROR_H_
