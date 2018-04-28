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


// This library contains helper macros and methods to make returning errors
// and propagating statuses easier.
//
// We use ::util::Status for error codes.  Methods that return status should
// have signatures like
//   ::util::Status Method(arg, ...);
// or
//   ::util::StatusOr<ValueType> Method(arg, ...);
//
// Inside the method, to return errors, use the macros
//   RETURN_ERROR() << "Message with ::util::error::UNKNOWN code";
//   RETURN_ERROR(code_enum)
//       << "Message with an error code, in that error_code's ErrorSpace "
//       << "(See ErrorCodeOptions below)";
//
// When calling another method, use this to propagate status easily.
//   RETURN_IF_ERROR(method(args));
//
// Use this to also append to the end of the error message when propagating
// an error:
//   RETURN_IF_ERROR_WITH_APPEND(method(args)) << " for method with " << args;
//
//
// For StatusOr results, you can extract the value or return on error.
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(arg));
// Or:
//   ValueType value;
//   ASSIGN_OR_RETURN(value, MaybeGetValue(arg));
//
// WARNING: ASSIGN_OR_RETURN expands into multiple statements; it cannot be used
//  in a single statement (e.g. as the body of an if statement without {})!
//
//
// To construct an error without immediately returning it, use MAKE_ERROR,
// which supports the same argument types as RETURN_ERROR.
//   ::util::Status status = MAKE_ERROR(...) << "Message";
//
// To add additional text onto an existing error, use
//   ::util::Status new_status = APPEND_ERROR(status) << ", additional details";
//
// These macros can also be assigned to a ::util::StatusOr variable:
//   ::util::StatusOr<T> status_or = MAKE_ERROR(...) << "Message";
//
// They can also be used to return from a function that returns
// ::util::StatusOr:
//   ::util::StatusOr<T> MyFunction() {
//     RETURN_ERROR(...) << "Message";
//   }
//
//
// Error codes:
//
// Using error codes is optional.  ::stratum::ERR_UNKNOWN will be used
// if no code is provided.
//
// By default, these macros work with canonical ::util::error::Code codes from
// util/task/codes.proto, using the canonical ErrorSpace.
// These macros will also work with project-specific ErrorSpaces and error
// code enums if you define a specialization of ErrorCodeOptions.
//
//
// Logging:
//
// RETURN_ERROR and MAKE_ERROR do not log the error by default.
//
// Logging can be turned on or off for a specific error by using
//   RETURN_ERROR().LogError() << "Message logged to LOG(ERROR)";
//   RETURN_ERROR().Log(INFO) << "Message logged to LOG(INFO)";
//   RETURN_ERROR().SetNoLogging() << "Message not logged";
//
// If logging is enabled, this will make an error also log a stack trace.
//   RETURN_ERROR().LogWithStackTrace(INFO) << "Message";
//
//
//
// Error payloads:
//
// Payload protos can be added to a ::util::Status object using
//   RETURN_ERROR().Attach(p1) << "Message";

#ifndef STRATUM_GLUE_STATUS_STATUS_MACROS_H_
#define STRATUM_GLUE_STATUS_STATUS_MACROS_H_

#include "third_party/stratum/public/lib/error.h"
#include "util/task/status_builder.h"
#include "util/task/status_macros.h"  // IWYU pragma: export

// !!! DEPRECATED: DO NOT USE THE FOLLOWING MACROS IN NEW CODE. !!!
// These macros are here to temporarily support old code until we switch it all
// to correct google3 status style. Use ::util::StatusBuilder directly in any
// new code.
// TODO: Delete after we've fixed all users.

namespace hercules_error_impl {
inline ::util::StatusBuilder MakeStatusBuilder(gtl::source_location loc,
                                               int code) {
  return ::util::StatusBuilder(
      ::util::Status(::stratum::StratumErrorSpace(), code, ""), loc);
}
inline ::util::StatusBuilder MakeStatusBuilder(gtl::source_location loc) {
  return MakeStatusBuilder(loc, ::stratum::ERR_UNKNOWN);
}
}  // namespace stratum_error_impl

#define MAKE_ERROR(...) \
  hercules_error_impl::MakeStatusBuilder(GTL_LOC, ##__VA_ARGS__)

#define RETURN_ERROR return MAKE_ERROR

#define APPEND_ERROR(status) \
  ::util::StatusBuilder((status), GTL_LOC).SetAppend()

#define RETURN_IF_ERROR_WITH_APPEND(expr) RETURN_IF_ERROR(expr).SetAppend()

#endif  // STRATUM_GLUE_STATUS_STATUS_MACROS_H_
