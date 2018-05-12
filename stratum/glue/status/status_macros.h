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
//   RETURN_ERROR(error_space, code_int)
//       << "Message with integer error code in specified ErrorSpace "
//       << "(Not recommended - use previous form with an enum code instead)";
//
// When calling another method, use this to propagate status easily.
//   RETURN_IF_ERROR(method(args));
//
// Use this to also append to the end of the error message when propagating
// an error:
//   RETURN_IF_ERROR_WITH_APPEND(method(args)) << " for method with " << args;
//
// Use this to propagate the status to a Stubby1 or Stubby2 RPC easily. This
// assumes an AutoClosureRunner has been set up on the RPC's associated
// closure, or it gets run some other way to signal the RPC's termination.
//   RETURN_RPC_IF_ERROR(rpc, method(args));
//
// Use this to propagate the status to a ::util::Task* instance
// calling task->Return() with the status.
//   RETURN_TASK_IF_ERROR(task, method(args));
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
// This can optionally be used to return ::util::Status::OK.
//   RETURN_OK();
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
// Using error codes is optional.  ::util::error::UNKNOWN will be used if no
// code is provided.
//
// By default, these macros work with canonical ::util::error::Code codes,
// using the canonical ErrorSpace. These macros will also work with
// project-specific ErrorSpaces and error code enums if a specialization
// of ErrorCodeOptions is defined.
//
//
// Logging:
//
// RETURN_ERROR and MAKE_ERROR log the error to LOG(ERROR) by default.
//
// Logging can be turned on or off for a specific error by using
//   RETURN_ERROR().with_logging() << "Message logged to LOG(ERROR)";
//   RETURN_ERROR().without_logging() << "Message not logged";
//   RETURN_ERROR().set_logging(false) << "Message not logged";
//   RETURN_ERROR().severity(INFO) << "Message logged to LOG(INFO)";
//
// If logging is enabled, this will make an error also log a stack trace.
//   RETURN_ERROR().with_log_stack_trace() << "Message";
//
// Logging can also be controlled within a scope using
// ScopedErrorLogSuppression.
//
//
// Assertion handling:
//
// When you would use a CHECK, CHECK_EQ, etc, you can instead use RET_CHECK
// to return a ::util::Status if the condition is not met:
//   RET_CHECK(ptr != null);
//   RET_CHECK_GT(value, 0) << "Optional additional message";
//   RET_CHECK_FAIL() << "Always fail, like a LOG(FATAL)";
//
// These are a better replacement for CHECK because they don't crash, and for
// DCHECK and LOG(DFATAL) because they don't ignore errors in opt builds.
//
// The RET_CHECK* macros can only be used in functions that return
// ::util::Status.
//
// The returned error will have the ::util::error::INTERNAL error code and the
// message will include the file and line number.  The current stack trace
// will also be logged.

#ifndef STRATUM_GLUE_STATUS_STATUS_MACROS_H_
#define STRATUM_GLUE_STATUS_STATUS_MACROS_H_

#include <memory>
#include <ostream>  // NOLINT
#include <sstream>  // NOLINT  // IWYU pragma: keep
#include <string>
#include <vector>

#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace util {

namespace status_macros {

using base_logging::LogSeverity;

// Base class for options attached to a project-specific error code enum.
// Projects that use non-canonical error codes should specialize the
// ErrorCodeOptions template below with a subclass of this class, overriding
// GetErrorSpace, and optionally other methods.
class BaseErrorCodeOptions {
 public:
  // Return the ErrorSpace to use for this error code enum.
  // Not implemented in base class - must be overridden.
  const ::util::ErrorSpace* GetErrorSpace();

  // Returns true if errors with this code should be logged upon creation, by
  // default.  (Default can be overridden with modifiers on MakeErrorStream.)
  // Can be overridden to customize default logging per error code.
  bool IsLoggedByDefault(int code) { return true; }
};

// Template that should be specialized for any project-specific error code enum.
template <typename ERROR_CODE_ENUM_TYPE>
class ErrorCodeOptions;

// Specialization for the canonical error codes and canonical ErrorSpace.
template <>
class ErrorCodeOptions< ::util::error::Code> : public BaseErrorCodeOptions {
 public:
  const ::util::ErrorSpace* GetErrorSpace() {
    return ::util::Status::canonical_space();
  }
};

// Stream object used to collect error messages in MAKE_ERROR macros or
// append error messages with APPEND_ERROR.
// It accepts any arguments with operator<< to build an error string, and
// then has an implicit cast operator to ::util::Status, which converts the
// logged string to a Status object and returns it, after logging the error.
// At least one call to operator<< is required; a compile time error will be
// generated if none are given. Errors will only be logged by default for
// certain status codes, as defined in IsLoggedByDefault. This class will
// give DFATAL errors if you don't retrieve a ::util::Status exactly once before
// destruction.
//
// The class converts into an intermediate wrapper object
// MakeErrorStreamWithOutput to check that the error stream gets at least one
// item of input.
class MakeErrorStream {
 public:
  // Wrapper around MakeErrorStream that only allows for output. This is created
  // as output of the first operator<< call on MakeErrorStream. The bare
  // MakeErrorStream does not have a ::util::Status operator. The net effect of
  // that is that you have to call operator<< at least once or else you'll get
  // a compile time error.
  class MakeErrorStreamWithOutput {
   public:
    explicit MakeErrorStreamWithOutput(MakeErrorStream* error_stream)
        : wrapped_error_stream_(error_stream) {}

    template <typename T>
    MakeErrorStreamWithOutput& operator<<(const T& value) {
      *wrapped_error_stream_ << value;
      return *this;
    }

    // Implicit cast operators to ::util::Status and ::util::StatusOr.
    // Exactly one of these must be called exactly once before destruction.
    operator ::util::Status() {
      return wrapped_error_stream_->GetStatus();
    }
    template <typename T>
    operator ::util::StatusOr<T>() {
      return wrapped_error_stream_->GetStatus();
    }

    // MakeErrorStreamWithOutput is neither copyable nor movable.
    MakeErrorStreamWithOutput(const MakeErrorStreamWithOutput&) = delete;
    MakeErrorStreamWithOutput& operator=(const MakeErrorStreamWithOutput&) =
        delete;

   private:
    MakeErrorStream* wrapped_error_stream_;
  };

  // Make an error with ::util::error::UNKNOWN.
  MakeErrorStream(const char* file, int line)
      : impl_(new Impl(file, line,
                       ::util::Status::canonical_space(),
                       ::util::error::UNKNOWN, this)) {}

  // Make an error with the given error code and error_space.
  MakeErrorStream(const char* file, int line,
                  const ::util::ErrorSpace* error_space, int code)
      : impl_(new Impl(file, line, error_space, code, this)) {}

  // Make an error that appends additional messages onto a copy of status.
  MakeErrorStream(::util::Status status, const char* file, int line)
      : impl_(new Impl(status, file, line, this)) {}

  // Make an error with the given code, inferring its ErrorSpace from
  // code's type using the specialized ErrorCodeOptions.
  template <typename ERROR_CODE_TYPE>
  MakeErrorStream(const char* file, int line, ERROR_CODE_TYPE code)
    : impl_(new Impl(
          file, line,
          ErrorCodeOptions<ERROR_CODE_TYPE>().GetErrorSpace(),
          code, this,
          ErrorCodeOptions<ERROR_CODE_TYPE>().IsLoggedByDefault(code))) {}

  template <typename T>
  MakeErrorStreamWithOutput& operator<<(const T& value) {
    CheckNotDone();
    impl_->stream_ << value;
    return impl_->make_error_stream_with_output_wrapper_;
  }

  // Disable sending this message to LOG(ERROR), even if this code is usually
  // logged. Some error codes are logged by default, and others are not.
  // Usage:
  //   return MAKE_ERROR().without_logging() << "Message";
  MakeErrorStream& without_logging() {
    impl_->should_log_ = false;
    return *this;
  }

  // Send this message to LOG(ERROR), even if this code is not usually logged.
  // Usage:
  //   return MAKE_ERROR().with_logging() << "Message";
  MakeErrorStream& with_logging() {
    impl_->should_log_ = true;
    return *this;
  }

  // Determine whether to log this message based on the value of <should_log>.
  MakeErrorStream& set_logging(bool should_log) {
    impl_->should_log_ = should_log;
    return *this;
  }

  // Log the status at this LogSeverity: INFO, WARNING, or ERROR.
  // Setting severity to NUM_SEVERITIES will disable logging.
  MakeErrorStream& severity(LogSeverity log_severity) {
    impl_->log_severity_ = log_severity;
    return *this;
  }

  // When this message is logged (see with_logging()), include the stack trace.
  MakeErrorStream& with_log_stack_trace() {
    impl_->should_log_stack_trace_ = true;
    return *this;
  }

  // When this message is logged, omit the stack trace, even if
  // with_log_stack_trace() was previously called.
  MakeErrorStream& without_log_stack_trace() {
    impl_->should_log_stack_trace_ = false;
    return *this;
  }

  // Adds RET_CHECK failure text to error message.
  MakeErrorStreamWithOutput& add_ret_check_failure(const char* condition) {
    return *this << "RET_CHECK failure (" << impl_->file_ << ":" << impl_->line_
                 << ") " << condition << " ";
  }

  // Adds RET_CHECK_FAIL text to error message.
  MakeErrorStreamWithOutput& add_ret_check_fail_failure() {
    return *this << "RET_CHECK_FAIL failure (" << impl_->file_ << ":"
                 << impl_->line_ << ") ";
  }

  // MakeErrorStream is neither copyable nor movable.
  MakeErrorStream(const MakeErrorStream&) = delete;
  MakeErrorStream& operator=(const MakeErrorStream&) = delete;

 private:
  class Impl {
   public:
    Impl(const char* file, int line,
         const ::util::ErrorSpace* error_space, int  code,
         MakeErrorStream* error_stream,
         bool is_logged_by_default = true);
    Impl(const ::util::Status& status, const char* file, int line,
         MakeErrorStream* error_stream);

    ~Impl();

    // This must be called exactly once before destruction.
    ::util::Status GetStatus();

    void CheckNotDone() const;

    // Impl is neither copyable nor movable.
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

   private:
    const char* file_;
    int line_;
    const ::util::ErrorSpace* error_space_;
    int code_;

    std::string prior_message_;
    bool is_done_;  // true after Status object has been returned
    std::ostringstream stream_;
    bool should_log_;
    LogSeverity log_severity_;
    bool should_log_stack_trace_;

    // Wrapper around the MakeErrorStream object that has a ::util::Status
    // conversion. The first << operator called on MakeErrorStream will return
    // this object, and only this object can implicitly convert to
    // ::util::Status. The net effect of this is that you'll get a compile time
    // error if you call MAKE_ERROR etc. without adding any output.
    MakeErrorStreamWithOutput make_error_stream_with_output_wrapper_;

    friend class MakeErrorStream;
  };

  void CheckNotDone() const;

  // Returns the status. Used by MakeErrorStreamWithOutput.
  ::util::Status GetStatus() const { return impl_->GetStatus(); }

  // Store the actual data on the heap to reduce stack frame sizes.
  std::unique_ptr<Impl> impl_;
};

// Make an error ::util::Status, building message with LOG-style shift
// operators.  The error also gets sent to LOG(ERROR).
//
// Takes an optional error code parameter. Uses ::util::error::UNKNOWN by
// default.  Returns a ::util::Status object that must be returned or stored.
//
// Examples:
//   return MAKE_ERROR() << "Message";
//   return MAKE_ERROR(INTERNAL_ERROR) << "Message";
//   ::util::Status status = MAKE_ERROR() << "Message";
#define MAKE_ERROR(...) \
  ::util::status_macros::MakeErrorStream(__FILE__, __LINE__, ##__VA_ARGS__)

// Return a new error based on an existing error, with an additional string
// appended.  Otherwise behaves like MAKE_ERROR, including logging the error by
// default.
// Requires !status.ok().
// Example:
//   status = APPEND_ERROR(status) << ", more details";
//   return APPEND_ERROR(status) << ", more details";
#define APPEND_ERROR(status) \
  ::util::status_macros::MakeErrorStream((status), __FILE__, __LINE__)

// Shorthand to make an error (with MAKE_ERROR) and return it.
//   if (error) {
//     RETURN_ERROR() << "Message";
//   }
#define RETURN_ERROR return MAKE_ERROR

// Return success.
#define RETURN_OK() \
  return ::util::Status::OK

// Wraps a ::util::Status so it can be assigned and used in an if-statement.
// Implicitly converts from status and to bool.
namespace internal {
class UtilStatusConvertibleToBool {
 public:
  // Implicity conversion from a status to wrap.
  // NOLINTNEXTLINE Need implicit conversion to allow in if-statement.
  UtilStatusConvertibleToBool(::util::Status status)
      : status_(status) { }
  // Implicity cast to bool. True on ok() and false on error.
  operator bool() const { return ABSL_PREDICT_TRUE(status_.ok()); }
  // Returns the wrapped status.
  ::util::Status status() const {
    return status_;
  }
 private:
  ::util::Status status_;
};
}  // namespace internal

// Run a command that returns a ::util::Status.  If the called code returns an
// error status, return that status up out of this method too.
//
// Example:
//   RETURN_IF_ERROR(DoThings(4));
#define RETURN_IF_ERROR(expr)                                                \
  do {                                                                       \
    /* Using _status below to avoid capture problems if expr is "status". */ \
    const ::util::Status _status = (expr);                                   \
    if (ABSL_PREDICT_FALSE(!_status.ok())) {                                 \
      LOG(ERROR) << "Return Error: " << #expr << " failed with " << _status; \
      return _status;                                                        \
    }                                                                        \
  } while (0)

// This is like RETURN_IF_ERROR, but instead of propagating the existing error
// Status, it constructs a new Status and can append additional messages.
//
// This has slightly worse performance that RETURN_IF_ERROR in both OK and ERROR
// case. (see status_macros_benchmark.cc for details)
//
// Example:
//   RETURN_IF_ERROR_WITH_APPEND(DoThings(4)) << "Things went wrong for " << 4;
//
// Args:
//   expr: Command to run that returns a ::util::Status.
#define RETURN_IF_ERROR_WITH_APPEND(expr)                                     \
  /* Using _status below to avoid capture problems if expr is "status". */    \
  /* We also need the error to be in the else clause, to avoid a dangling  */ \
  /* else in the client code. (see test for example). */                      \
  if (const ::util::status_macros::internal::UtilStatusConvertibleToBool      \
          _status = (expr)) {                                                 \
  } else /* NOLINT */                                                         \
    for (LOG(ERROR) << "Return error: " << #expr << " failed with "           \
                    << _status.status();                                      \
         true;)                                                               \
    return ::util::status_macros::MakeErrorStream(_status.status(), __FILE__, \
                                                  __LINE__)                   \
        .without_logging()

// Internal helper for concatenating macro values.
#define STATUS_MACROS_CONCAT_NAME_INNER(x, y) x##y
#define STATUS_MACROS_CONCAT_NAME(x, y) STATUS_MACROS_CONCAT_NAME_INNER(x, y)

#define ASSIGN_OR_RETURN_IMPL(statusor, lhs, rexpr)                       \
  auto statusor = (rexpr);                                                \
  if (ABSL_PREDICT_FALSE(!statusor.ok())) {                               \
    LOG(ERROR) << "Return Error: " << #rexpr << " at " << __FILE__ << ":" \
               << __LINE__;                                               \
    return statusor.status();                                             \
  }                                                                       \
  lhs = statusor.ConsumeValueOrDie();

// Executes an expression that returns a ::util::StatusOr, extracting its value
// into the variable defined by lhs (or returning on error).
//
// Example: Declaring and initializing a new value
//   ASSIGN_OR_RETURN(const ValueType& value, MaybeGetValue(arg));
//
// Example: Assigning to an existing value
//   ValueType value;
//   ASSIGN_OR_RETURN(value, MaybeGetValue(arg));
//
// Example: Assigning std::unique_ptr<T>
//   ASSIGN_OR_RETURN(std::unique_ptr<T> ptr, MaybeGetPtr(arg));
//
// The value assignment example would expand into:
//   StatusOr<ValueType> status_or_value = MaybeGetValue(arg);
//   if (!status_or_value.ok()) return status_or_value.status();
//   value = status_or_value.ConsumeValueOrDie();
//
// WARNING: ASSIGN_OR_RETURN expands into multiple statements; it cannot be used
//  in a single statement (e.g. as the body of an if statement without {})!
#define ASSIGN_OR_RETURN(lhs, rexpr) \
  ASSIGN_OR_RETURN_IMPL( \
      STATUS_MACROS_CONCAT_NAME(_status_or_value, __COUNTER__), lhs, rexpr);

// If condition is false, this macro returns, from the current function, a
// ::util::Status with the ::util::error::INTERNAL code.
// For example:
//   RET_CHECK(condition) << message;
// is equivalent to:
//   if(!condition) {
//     return MAKE_ERROR() << message;
//   }
// Note that the RET_CHECK macro includes some more information in the error
// and logs a stack trace.
//
// Intended to be used as a replacement for CHECK where crashes are
// unacceptable. The containing function must return a ::util::Status.
#define RET_CHECK(condition)                                             \
  while (ABSL_PREDICT_FALSE(!(condition)))                               \
    while (::util::status_macros::helper_log_always_return_true())       \
  return ::util::status_macros::MakeErrorStream(__FILE__, __LINE__,      \
                                                ::util::error::INTERNAL) \
      .with_log_stack_trace()                                            \
      .add_ret_check_failure(#condition)

///////
// Implementation code for RET_CHECK_EQ, RET_CHECK_NE, etc.

// Wraps a string*, allowing it to be written to an ostream and deleted.
// This is needed because the RET_CHECK_OP macro needs to free the memory
// after the error message is logged.
namespace internal {
struct ErrorDeleteStringHelper {
  explicit ErrorDeleteStringHelper(std::string* str_in) : str(str_in) { }
  ~ErrorDeleteStringHelper() { delete str; }
  std::string* str;

  // ErrorDeleteStringHelper is neither copyable nor movable.
  ErrorDeleteStringHelper(const ErrorDeleteStringHelper&) = delete;
  ErrorDeleteStringHelper& operator=(const ErrorDeleteStringHelper&) = delete;
};

}  // namespace internal

// Helper macros for binary operators.
// Don't use these macro directly in your code, use RET_CHECK_EQ et al below.

// The definition of RET_CHECK_OP is modeled after that of CHECK_OP_LOG in
// logging.h.

// Unlike google3 version of status_macros.h, we don't use the error string
// building macros from logging.h, since the depot3 versions have a memory leak.
// b/19991103
template<class t1, class t2>
std::string* MakeRetCheckOpString(
    const t1& v1, const t2& v2, const char* names) {
  std::stringstream ss;
  ss << names << " (" << v1 << " vs. " << v2 << ")";
  return new std::string(ss.str());
}
#define DEFINE_RET_CHECK_OP_IMPL(name, op)                             \
  template <class t1, class t2>                                        \
  inline std::string* RetCheck##name##Impl(const t1& v1, const t2& v2, \
                                           const char* names) {        \
    if (ABSL_PREDICT_TRUE(v1 op v2)) {                                 \
      return NULL;                                                     \
    } else {                                                           \
      LOG(ERROR) << "Return Error: "                                   \
                 << " at " << __FILE__ << ":" << __LINE__;             \
      return MakeRetCheckOpString(v1, v2, names);                      \
    }                                                                  \
  }                                                                    \
  inline std::string* RetCheck##name##Impl(int v1, int v2,             \
                                           const char* names) {        \
    return RetCheck##name##Impl<int, int>(v1, v2, names);              \
  }
DEFINE_RET_CHECK_OP_IMPL(_EQ, ==)
DEFINE_RET_CHECK_OP_IMPL(_NE, !=)
DEFINE_RET_CHECK_OP_IMPL(_LE, <=)
DEFINE_RET_CHECK_OP_IMPL(_LT, < )
DEFINE_RET_CHECK_OP_IMPL(_GE, >=)
DEFINE_RET_CHECK_OP_IMPL(_GT, > )
#undef DEFINE_RET_CHECK_OP_IMPL

#if defined(STATIC_ANALYSIS)
// Only for static analysis tool to know that it is equivalent to comparison.
#define RET_CHECK_OP(name, op, val1, val2) RET_CHECK((val1) op (val2))
#elif !defined(NDEBUG)
// In debug mode, avoid constructing CheckOpStrings if possible,
// to reduce the overhead of RET_CHECK statements.
#define RET_CHECK_OP(name, op, val1, val2) \
  while (std::string* _result = \
         ::util::status_macros::RetCheck##name##Impl(      \
              google::GetReferenceableValue(val1),         \
              google::GetReferenceableValue(val2),         \
              #val1 " " #op " " #val2))                               \
    return ::util::status_macros::MakeErrorStream(__FILE__, __LINE__, \
                                                  ::util::error::INTERNAL) \
        .with_log_stack_trace() \
        .add_ret_check_failure( \
             ::util::status_macros::internal::ErrorDeleteStringHelper( \
                 _result).str->c_str())
#else
// In optimized mode, use CheckOpString to hint to compiler that
// the while condition is unlikely.
#define RET_CHECK_OP(name, op, val1, val2) \
  while (CheckOpString _result = \
         ::util::status_macros::RetCheck##name##Impl(      \
              google::GetReferenceableValue(val1),         \
              google::GetReferenceableValue(val2),         \
              #val1 " " #op " " #val2))                               \
    return ::util::status_macros::MakeErrorStream(__FILE__, __LINE__, \
                                                  ::util::error::INTERNAL) \
        .with_log_stack_trace() \
        .add_ret_check_failure( \
             ::util::status_macros::internal::ErrorDeleteStringHelper( \
                 _result.str_).str->c_str())
#endif  // STATIC_ANALYSIS, !NDEBUG

// End of implementation code for RET_CHECK_EQ, RET_CHECK_NE, etc.
///////////////

// If the two values fail the comparison, this macro returns, from the current
// function, a ::util::Status with code ::util::error::INTERNAL.
// For example,
//   RET_CHECK_EQ(val1, val2) << message;
// is equivalent to
//   if(!(val1 == val2)) {
//     return MAKE_ERROR() << message;
//   }
// Note that the RET_CHECK macro includes some more information in the error
// and logs a stack trace.
//
// Intended to be used as a replacement for CHECK_* where crashes are
// unacceptable. The containing function must return a ::util::Status.
#define RET_CHECK_EQ(val1, val2) RET_CHECK_OP(_EQ, ==, val1, val2)
#define RET_CHECK_NE(val1, val2) RET_CHECK_OP(_NE, !=, val1, val2)
#define RET_CHECK_LE(val1, val2) RET_CHECK_OP(_LE, <=, val1, val2)
#define RET_CHECK_LT(val1, val2) RET_CHECK_OP(_LT, < , val1, val2)
#define RET_CHECK_GE(val1, val2) RET_CHECK_OP(_GE, >=, val1, val2)
#define RET_CHECK_GT(val1, val2) RET_CHECK_OP(_GT, > , val1, val2)

// Unconditionally returns an error.  Use in place of RET_CHECK(false).
// Example:
//   if (a) {
//     ...
//   } else if (b) {
//     ...
//   } else {
//     RET_CHECK_FAIL() << "Failed to satisfy a or b";
//   }
#define RET_CHECK_FAIL() \
  LOG(ERROR) << "Return Error: " << " at "                          \
             << __FILE__ << ":" << __LINE__;                        \
  return ::util::status_macros::MakeErrorStream(__FILE__, __LINE__, \
                                                ::util::error::INTERNAL) \
      .with_log_stack_trace() \
      .add_ret_check_fail_failure()

}  // namespace status_macros
}  // namespace util

#endif  // STRATUM_GLUE_STATUS_STATUS_MACROS_H_
