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


#ifndef STRATUM_LIB_MACROS_H_
#define STRATUM_LIB_MACROS_H_

#include <string>

/* START GOOGLE ONLY
#include "stratum/glue/status/status_builder.h"
   END GOOGLE ONLY */
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/logging.h"
#include "stratum/public/lib/error.h"

namespace stratum {

// A macro for simplify checking and logging a condition. The error code
// return here is the one that matches the most of the uses.
#define CHECK_RETURN_IF_FALSE(cond) \
  if (ABSL_PREDICT_TRUE(cond)) {    \
  } else /* NOLINT */               \
    return MAKE_ERROR(ERR_INVALID_PARAM) << "'" << #cond << "' is false. "

// A simple class to explicitly cast the return value of an ::util::Status
// to bool.
class BooleanStatus {
 public:
  BooleanStatus(::util::Status status) : status_(status) {}  // NOLINT
  // Implicitly cast to bool.
  operator bool() const { return status_.ok(); }
  inline ::util::Status status() const { return status_; }
 private:
  ::util::Status status_;
};

inline const std::string FixMessage(const std::string& msg) {
  std::string str = msg;
  std::size_t found = str.find_last_not_of(" \t\f\v\n\r");
  if (found != std::string::npos) {
    str.erase(found + 1);
    if (str.back() != '.' && str.back() != '!' && str.back() != '?' &&
        str.back() != ';' && str.back() != ':' && str.back() != ',') {
      str += ". ";
    } else {
      str += " ";
    }
  } else {
    str.clear();
  }

  return str;
}

// A macro for simplifying creation of a new error or appending new info to an
// error based on the return value of a function that returns ::util::Status.
#define APPEND_STATUS_IF_ERROR(out, expr)                                      \
  if (const BooleanStatus __ret = (expr)) {                                    \
  } else /* NOLINT */                                                          \
    out = APPEND_ERROR(!out.ok() ? out : __ret.status().StripMessage())        \
              .without_logging()                                               \
          << (out.error_message().empty() || out.error_message().back() == ' ' \
                  ? ""                                                         \
                  : " ")                                                       \
          << FixMessage(__ret.status().error_message())

// A macro to facilitate checking whether a user/group is authorized to call an
// RPC in a specific service.
#define RETURN_IF_NOT_AUTHORIZED(checker, service, rpc, context)      \
  do {                                                                \
    ::util::Status status =                                           \
        checker->Authorize(#service, #rpc, *context->auth_context()); \
    if (!status.ok()) {                                               \
      return ::grpc::Status(ToGrpcCode(status.CanonicalCode()),       \
                            status.error_message());                  \
    }                                                                 \
  } while (0)

/* START GOOGLE ONLY
// !!! DEPRECATED: DO NOT USE THE FOLLOWING MACROS IN NEW CODE. !!!
// These macros are here to temporarily support old code until we switch it all
// to correct google3 status style. Use ::util::StatusBuilder directly in any
// new code.
// TODO(swiggett): Delete after we've fixed all users.

namespace stratum_error_impl {
inline ::util::StatusBuilder MakeStatusBuilder(gtl::source_location loc,
                                               int code) {
  return ::util::StatusBuilder(
      ::util::Status(::google::stratum::StratumErrorSpace(), code, ""), loc);
}
inline ::util::StatusBuilder MakeStatusBuilder(gtl::source_location loc) {
  return MakeStatusBuilder(loc, ::google::stratum::ERR_UNKNOWN);
}
_error_impl

#define MAKE_ERROR(...) \
  stratum_error_impl::MakeStatusBuilder(GTL_LOC, ##__VA_ARGS__)

#define APPEND_ERROR(status) \
  ::util::StatusBuilder((status), GTL_LOC).SetAppend()

#define RETURN_IF_ERROR_WITH_APPEND(expr) RETURN_IF_ERROR(expr).SetAppend()
    END GOOGLE ONLY */

// Replaced by glog CHECK_NOTNULL until it is added to Abseil
#define ABSL_DIE_IF_NULL CHECK_NOTNULL

// Stringify the result of expansion of a macro to a string
// e.g:
// #define A text
// STRINGIFY(A) => "text"
// Ref: https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html
#define STRINGIFY_INNER(s) #s
#define STRINGIFY(s) STRINGIFY_INNER(s)

}  // namespace stratum

#endif  // STRATUM_LIB_MACROS_H_
