// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_GLUE_STATUS_STATUS_H_
#define STRATUM_GLUE_STATUS_STATUS_H_

#include <atomic>
#include <iosfwd>
#include <string>

#include "absl/base/attributes.h"
#include "absl/strings/str_cat.h"
#include "stratum/glue/gtl/source_location.h"
#include "stratum/glue/logging.h"

// TODO(unknown): Move to Abseil-status when it is available.
//
namespace util {
namespace error {
enum Code {
  OK = 0,
  CANCELLED = 1,
  UNKNOWN = 2,
  INVALID_ARGUMENT = 3,
  DEADLINE_EXCEEDED = 4,
  NOT_FOUND = 5,
  ALREADY_EXISTS = 6,
  PERMISSION_DENIED = 7,
  RESOURCE_EXHAUSTED = 8,
  FAILED_PRECONDITION = 9,
  ABORTED = 10,
  OUT_OF_RANGE = 11,
  UNIMPLEMENTED = 12,
  INTERNAL = 13,
  UNAVAILABLE = 14,
  DATA_LOSS = 15,
  UNAUTHENTICATED = 16,
  DO_NOT_USE_RESERVED_FOR_FUTURE_EXPANSION_USE_DEFAULT_IN_SWITCH_INSTEAD_ = 20,
  // **DO NOT ADD ANYTHING TO THIS**
};
static const enum Code Code_MIN = Code::OK;
static const enum Code Code_MAX = Code::UNAUTHENTICATED;
inline bool Code_IsValid(int c) { return (c >= Code_MIN) && (c <= Code_MAX); }
}  // end namespace error
}  // end namespace util

namespace util {

using google::LogSeverity;

// An ErrorSpace is a collection of related numeric error codes.  For
// example, all Posix errno values may be placed in the same
// ErrorSpace, all bigtable errors may be placed in the same
// ErrorSpace, etc.
//
// We recommend that new APIs use the canonical error space (and the
// corresponding two-argument constructor below) instead of creating a
// new error space per API
class ErrorSpace;

class Status final {
 public:
  // Creates a "successful" status.
  Status();

  // Create a status in the canonical error space with the specified
  // code, and error message.  If "code == 0", error_message is
  // ignored and a Status object identical to Status::OK is
  // constructed.
  Status(::util::error::Code code, const std::string& error_message);

  // Creates a status in the specified "space", "code" and the
  // associated error message.  If "code == 0", (space,msg) are ignored
  // and a Status object identical to Status::OK is constructed.
  //
  // New APIs should use the canonical error space and the preceding
  // two-argument constructor.
  //
  // REQUIRES: space != NULL
  Status(const ErrorSpace* space, int code, const std::string& msg);

  Status(const Status&);
  Status& operator=(const Status& x);
  ~Status();

  // For backwards compatibility, provide aliases of some the
  // canonical error codes defined in codes.proto.
  enum Code {
    OK_CODE = 0,         // No error
    CANCELLED_CODE = 1,  // For cancelled operations
    UNKNOWN_CODE = 2,    // For unknown spaces/codes
  };

  // Some pre-defined Status objects
  static const Status& OK;  // Identical to 0-arg constructor
  static const Status& CANCELLED;
  static const Status& UNKNOWN;

  // Return the canonical error space.
  static const ErrorSpace* canonical_space();

  // Store the specified error in this Status object. If "code == 0",
  // (space,msg) are ignored and a Status object identical to Status::OK
  // is constructed.
  // REQUIRES: code == 0 OR space != NULL
  void SetError(const ErrorSpace* space, int code, const std::string& msg);

  // If "ok()", stores "new_status" into *this.  If "!ok()", preserves
  // the current "error_code()/error_message()/error_space()",
  // but may augment with additional information about "new_status".
  //
  // Convenient way of keeping track of the first error encountered.
  // Instead of:
  //   if (overall_status.ok()) overall_status = new_status
  // Use:
  //   overall_status.Update(new_status);
  void Update(const Status& new_status);

  // Clear this status object to contain the OK code and no error message.
  void Clear();

  // Accessor
  bool ok() const ABSL_MUST_USE_RESULT;
  int error_code() const;
  const std::string& error_message() const;
  const ErrorSpace* error_space() const;

  // Returns the canonical code for this Status value.  Automatically
  // converts to the canonical space if necessary.
  ::util::error::Code CanonicalCode() const;

  // Sets the equivalent canonical code for a Status with a
  // non-canonical error space.
  void SetCanonicalCode(int canonical_code);

  bool operator==(const Status& x) const;
  bool operator!=(const Status& x) const;

  // Returns true iff this->CanonicalCode() == expected.
  bool Matches(::util::error::Code expected) const;

  // Returns true iff this has the same error_space, error_code,
  // and canonical_code as "x".  I.e., the two Status objects are
  // identical except possibly for the error message.
  bool Matches(const Status& x) const;

  // Return a combination of the error code name and message.
  std::string ToString() const;

  // Returns a copy of the status object in the canonical error space.  This
  // will use the canonical code from the status protocol buffer (if present) or
  // the result of passing this status to the ErrorSpace CanonicalCode method.
  Status ToCanonical() const;

  // If this->Matches(x), return without doing anything.
  // Else die with an appropriate error message.
  void CheckMatches(const Status& x) const;

  // Ignores any errors. This method does nothing except potentially suppress
  // complaints from any tools that are checking that errors are not dropped on
  // the floor.
  void IgnoreError() const;

  // Swap the contents of *this with *that
  void Swap(util::Status* that) {
    Rep* t = this->rep_;
    this->rep_ = that->rep_;
    that->rep_ = t;
  }

  // Returns a copy of the status object with error message stripped off.
  // Useful for comparing against expected status when error message
  // might vary, e.g.
  //     EXPECT_EQ(expected_status, real_status.StripMessage());
  Status StripMessage() const;

 private:
  // Reference-counted representation
  static const unsigned int kGlobalRef = 0;
  struct Rep {
    std::atomic<unsigned int> ref;  // reference count.
    int code;                       // code >= 0
    int canonical_code;             // 0 means use space to calculate
    const ErrorSpace* space_ptr;    // NULL means canonical_space()
    std::string* message_ptr;       // NULL means empty
  };
  Rep* rep_;  // Never NULL.

  static void UnrefSlow(Rep*);
  inline static void Ref(Rep* r) {
    // Do not adjust refs for globals
    if (r->ref != kGlobalRef) {
      ++r->ref;
    }
  }
  inline static void Unref(Rep* r) {
    // Do not adjust refs for globals
    if (r->ref != kGlobalRef) {
      UnrefSlow(r);
    }
  }

  void InternalSet(const ErrorSpace* space, int code, const std::string& msg,
                   int canonical_code);

  // Returns the canonical code from the status protocol buffer (if present) or
  // the result of passing this status to the ErrorSpace CanonicalCode method.
  int RawCanonicalCode() const;

  // REQUIRES: !ok()
  // Ensures rep_ is not shared with any other Status.
  void PrepareToModify();

  static Rep* NewRep(const ErrorSpace*, int code, const std::string&,
                     int canonical_code);
  static void ResetRep(Rep* rep, const ErrorSpace*, int code,
                       const std::string&, int canonical_code);
  static bool EqualsSlow(const ::util::Status& a, const ::util::Status& b);

  // Machinery for linker initialization of the global Status objects.
  struct Pod;
  static Rep global_reps[];
  static const Pod globals[];
  static const std::string* EmptyString();
};

// Base class for all error spaces.  An error space is a collection
// of related error codes.  All error codes are non-zero.
// Zero always means success.
//
// NOTE:
// All ErrorSpace objects must be created before the end of the module
// initializer phase (see "base/googleinit.h"). In particular, ErrorSpace
// objects should not be lazily created unless some mechanism forces this to
// occur in the module initializer phase. In most cases, ErrorSpace objects
// should just be created by a module initializer e.g.:
//
//     REGISTER_MODULE_INITIALIZER(ThisModule, {
//         ThisModule::InitErrorSpace();
//         ... other module initialization
//     });
//
// This rule ensures that ErrorSpace::Find(), which cannot be called until
// after the module initializer phase, will see a complete ErrorSpace
// registry.
//
class ErrorSpace {
 public:
  // Return the name of this error space
  const std::string& SpaceName() const { return name_; }

  // Return a string corresponding to the specified error code.
  virtual std::string String(int code) const = 0;

  // Return the equivalent canonical code for the given status. ErrorSpace
  // implementations should override this method to provide a custom
  // mapping. The default is to always return UNKNOWN. It is an error to pass a
  // Status that does not belong to this space; ErrorSpace implementations
  // should return UNKNOWN in that case.
  virtual ::util::error::Code CanonicalCode(
      const ::util::Status& status) const {
    return error::UNKNOWN;
  }

  // Find the error-space with the specified name.  Return the
  // space object, or NULL if not found.
  //
  // NOTE: Do not call Find() until after InitGoogle() returns.
  // Otherwise, some module intializers that register error spaces may not
  // have executed and Find() might not locate the error space of
  // interest.
  static ErrorSpace* Find(const std::string& name);

  // ErrorSpace is neither copyable nor movable.
  ErrorSpace(const ErrorSpace&) = delete;
  ErrorSpace& operator=(const ErrorSpace&) = delete;

 protected:
  explicit ErrorSpace(const char* name);

  // Prevent deletions of ErrorSpace objects by random clients
  virtual ~ErrorSpace();

 private:
  const std::string name_;
};

// ::util::Status success comparison.
// This is better than CHECK((val).ok()) because the embedded
// error string gets printed by the CHECK_EQ.
#define CHECK_OK(val) CHECK_EQ(::util::Status::OK, (val))
#define QCHECK_OK(val) QCHECK_EQ(::util::Status::OK, (val))
#define DCHECK_OK(val) DCHECK_EQ(::util::Status::OK, (val))

// -----------------------------------------------------------------
// Implementation details follow

inline Status::Status() : rep_(&global_reps[0]) {}

inline Status::Status(const Status& x) : rep_(x.rep_) { Ref(rep_); }

inline Status& Status::operator=(const Status& x) {
  Rep* old_rep = rep_;
  if (x.rep_ != old_rep) {
    Ref(x.rep_);
    rep_ = x.rep_;
    Unref(old_rep);
  }
  return *this;
}

inline void Status::Update(const Status& new_status) {
  if (ok()) {
    *this = new_status;
  }
}

inline Status::~Status() { Unref(rep_); }

inline bool Status::ok() const { return rep_->code == 0; }

inline int Status::error_code() const { return rep_->code; }

inline const std::string& Status::error_message() const {
  return rep_->message_ptr ? *rep_->message_ptr : *EmptyString();
}

inline const ErrorSpace* Status::error_space() const {
  return rep_->space_ptr ? rep_->space_ptr : canonical_space();
}

inline bool Status::Matches(const Status& x) const {
  return (this->error_code() == x.error_code() &&
          this->error_space() == x.error_space() &&
          this->RawCanonicalCode() == x.RawCanonicalCode());
}

inline bool Status::operator==(const Status& x) const {
  return (this->rep_ == x.rep_) || EqualsSlow(*this, x);
}

inline bool Status::operator!=(const Status& x) const { return !(*this == x); }

inline bool Status::Matches(::util::error::Code expected) const {
  return CanonicalCode() == expected;
}

extern std::ostream& operator<<(std::ostream& os, const Status& x);

// Returns an OK status, equivalent to a default constructed instance. This was
// recently introduced in google3 in CL/132673373 and now everything is being
// moved to use this instead.
Status OkStatus();

#ifndef SWIG
inline Status OkStatus() { return Status(); }
#endif  // SWIG

class ABSL_MUST_USE_RESULT StatusBuilder {
 public:
  StatusBuilder(::util::error::Code code,
                ::stratum::gtl::source_location location)
      : code_(code),
        line_(location.line()),
        file_(location.file_name()),
        log_severity_(INFO),
        log_verbose_level_(0),
        log_type_(LogType::kDisabled) {}

  StatusBuilder& Log(LogSeverity severity) {
    if (code_ == ::util::error::Code::OK) return *this;
    log_type_ = LogType::kLog;
    log_severity_ = severity;
    return *this;
  }

  StatusBuilder& VLog(int level) {
    if (code_ == ::util::error::Code::OK) return *this;
    log_type_ = LogType::kVLog;
    log_verbose_level_ = level;
    return *this;
  }

  StatusBuilder& LogError() { return Log(ERROR); }
  StatusBuilder& LogWarning() { return Log(WARNING); }
  StatusBuilder& LogInfo() { return Log(INFO); }

  StatusBuilder& operator<<(absl::string_view value) {
    stream_.append(value.data(), value.size());
    return *this;
  }

  template <typename T>
  StatusBuilder& operator<<(const T& value) {
    absl::StrAppend(&stream_, value);
    return *this;
  }

  operator Status() const& {
    Status status(code_, stream_);
    if (log_type_ == LogType::kDisabled) return status;
    google::LogMessage log_message(file_.c_str(), line_, log_severity_);
    log_message.stream() << status;
    return status;
  }

  int line() const { return line_; }
  const std::string& file() const { return file_; }

 private:
  enum class LogType {
    kDisabled,
    kLog,
    kVLog,
  };

  const ::util::error::Code code_;
  const int line_;
  const std::string file_;
  LogSeverity log_severity_;
  int log_verbose_level_;
  LogType log_type_;
  std::string stream_;
};

inline StatusBuilder AbortedErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::ABORTED, location);
}

inline StatusBuilder AlreadyExistsErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::ALREADY_EXISTS, location);
}

inline StatusBuilder CancelledErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::CANCELLED, location);
}

inline StatusBuilder DataLossErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::DATA_LOSS, location);
}

inline StatusBuilder DeadlineExceededErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::DEADLINE_EXCEEDED, location);
}

inline StatusBuilder FailedPreconditionErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::FAILED_PRECONDITION, location);
}

inline StatusBuilder InternalErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::INTERNAL, location);
}

inline StatusBuilder InvalidArgumentErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::INVALID_ARGUMENT, location);
}

inline StatusBuilder NotFoundErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::NOT_FOUND, location);
}

inline StatusBuilder OutOfRangeErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::OUT_OF_RANGE, location);
}

inline StatusBuilder PermissionDeniedErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::PERMISSION_DENIED, location);
}

inline StatusBuilder UnauthenticatedErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::UNAUTHENTICATED, location);
}

inline StatusBuilder ResourceExhaustedErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::RESOURCE_EXHAUSTED, location);
}

inline StatusBuilder UnavailableErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::UNAVAILABLE, location);
}

inline StatusBuilder UnimplementedErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::UNIMPLEMENTED, location);
}

inline StatusBuilder UnknownErrorBuilder(
    ::stratum::gtl::source_location location) {
  return StatusBuilder(::util::error::Code::UNKNOWN, location);
}

}  // namespace util

#endif  // STRATUM_GLUE_STATUS_STATUS_H_
