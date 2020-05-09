// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/glue/status/status_macros.h"

#include <algorithm>

#include "absl/strings/str_cat.h"
#include "absl/base/optimization.h"
#include "stratum/glue/logging.h"

DEFINE_bool(status_macros_log_stack_trace, false,
            "If set, all errors generated will log a stack trace.");
DECLARE_bool(util_status_save_stack_trace);

namespace util {
namespace status_macros {

using ::google::LogMessage;
using ::google::LogSeverity;
using ::google::NUM_SEVERITIES;
using ::google::ERROR;
using ::google::FATAL;
using ::google::INFO;
using ::google::WARNING;

static ::util::Status MakeStatus(const util::ErrorSpace* error_space, int code,
                                 const std::string& message) {
  return ::util::Status(error_space, code, message);
}

// Log the error at the given severity, optionally with a stack trace.
// If log_severity is NUM_SEVERITIES, nothing is logged.
static void LogError(const ::util::Status& status, const char* filename,
                     int line, LogSeverity log_severity,
                     bool should_log_stack_trace) {
  if (ABSL_PREDICT_TRUE(log_severity != NUM_SEVERITIES)) {
    LogMessage log_message(filename, line, log_severity);
    log_message.stream() << status;
    // Logging actually happens in LogMessage destructor.
  }
}

// Make a ::util::Status with a code and error message, and also send
// it to LOG(<log_severity>) using the given filename and line (unless
// should_log is false, or log_severity is NUM_SEVERITIES).  If
// should_log_stack_trace is true, the stack trace is included in the log
// message (ignored if should_log is false).
static ::util::Status MakeError(const char* filename, int line,
                                const util::ErrorSpace* error_space, int code,
                                const std::string& message,
                                bool should_log, LogSeverity log_severity,
                                bool should_log_stack_trace) {
  if (ABSL_PREDICT_FALSE(code == ::util::error::OK)) {
    LOG(DFATAL) << "Cannot create error with status OK";
    error_space = ::util::Status::canonical_space();
    code = ::util::error::UNKNOWN;
  }
  const ::util::Status status = MakeStatus(error_space, code, message);
  if (ABSL_PREDICT_TRUE(should_log)) {
    LogError(status, filename, line, log_severity, should_log_stack_trace);
  }
  return status;
}

// Returns appropriate log severity based on suppression level, or
// NUM_SEVERITIES to indicate that logging should be disabled.
static LogSeverity GetSuppressedSeverity(
    LogSeverity severity, int suppressed_level) {
  if (suppressed_level == -1) {
    return WARNING;
  } else if (suppressed_level >= 0) {
    return VLOG_IS_ON(suppressed_level) ? INFO : NUM_SEVERITIES;
  } else {
    return severity;
  }
}

void LogErrorWithSuppression(const ::util::Status& status, const char* filename,
                             int line, int log_level) {
  const LogSeverity severity = GetSuppressedSeverity(ERROR, log_level);
  LogError(status, filename, line, severity,
           false /* should_log_stack_trace */);
}

// This method is written out-of-line rather than in the header to avoid
// generating a lot of inline code for error cases in all callers.
void MakeErrorStream::CheckNotDone() const {
  impl_->CheckNotDone();
}

MakeErrorStream::Impl::Impl(
    const char* file, int line, const util::ErrorSpace* error_space, int code,
    MakeErrorStream* error_stream, bool is_logged_by_default)
    : file_(file), line_(line), error_space_(error_space), code_(code),
      is_done_(false),
      should_log_(is_logged_by_default),
      log_severity_(ERROR),
      should_log_stack_trace_(FLAGS_status_macros_log_stack_trace),
      make_error_stream_with_output_wrapper_(error_stream) {}

MakeErrorStream::Impl::Impl(const ::util::Status& status, const char* file,
                            int line, MakeErrorStream* error_stream)
    : file_(file),
      line_(line),
      // Make sure we show some error, even if the call is incorrect.
      error_space_(!status.ok() ? status.error_space()
                                : ::util::Status::canonical_space()),
      code_(!status.ok() ? status.error_code() : ::util::error::UNKNOWN),
      prior_message_(status.error_message()),
      is_done_(false),
      // Error code type is not visible here, so we can't call
      // IsLoggedByDefault.
      should_log_(true),
      log_severity_(ERROR),
      should_log_stack_trace_(FLAGS_status_macros_log_stack_trace),
      make_error_stream_with_output_wrapper_(error_stream) {
  DCHECK(!status.ok()) << "Attempted to append error text to status OK";
}

MakeErrorStream::Impl::~Impl() {
  // Note: error messages refer to the public MakeErrorStream class.

  LOG_IF(DFATAL, !is_done_)
      << "MakeErrorStream destructed without getting Status: "
      << file_ << ":" << line_ << " " << stream_.str();
}

::util::Status MakeErrorStream::Impl::GetStatus() {
  // Note: error messages refer to the public MakeErrorStream class.

  // Getting a ::util::Status object out more than once is not harmful, but
  // it doesn't match the expected pattern, where the stream is constructed
  // as a temporary, loaded with a message, and then casted to Status.
  LOG_IF(DFATAL, is_done_)
      << "MakeErrorStream got Status more than once: "
      << file_ << ":" << line_ << " " << stream_.str();

  is_done_ = true;

  const std::string& stream_str = stream_.str();
  std::string str = absl::StrCat(prior_message_.c_str(), stream_str.c_str());
  if (ABSL_PREDICT_FALSE(str.empty())) {
    return MakeError(
        file_, line_, error_space_, code_,
        absl::StrCat(str.c_str(), "Error without message at ", file_, ":",
                     line_),
        true /* should_log */, ERROR /* log_severity */,
        should_log_stack_trace_);
  } else {
    const LogSeverity actual_severity = ERROR;
    return MakeError(file_, line_, error_space_, code_, str,
                     should_log_, actual_severity, should_log_stack_trace_);
  }
}

void MakeErrorStream::Impl::CheckNotDone() const {
  LOG_IF(DFATAL, is_done_)
      << "MakeErrorStream shift called after getting Status: "
      << file_ << ":" << line_ << " " << stream_.str();
}

}  // namespace status_macros
}  // namespace util
