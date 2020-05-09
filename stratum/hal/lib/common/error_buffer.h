// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_COMMON_ERROR_BUFFER_H_
#define STRATUM_HAL_LIB_COMMON_ERROR_BUFFER_H_

#include <string>
#include <vector>

#include "stratum/glue/status/status.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/gtl/source_location.h"

namespace stratum {
namespace hal {

// The class "ErrorBuffer" is a thread-safe buffer for all the critical errors
// HAL components may encounter. It can be safely passed to different HAL
// services to log the critical errors they enounter.
// TODO(unknown): Unable to use CircularBuffer now due to some base dependency.
// Needs investigation.
// TODO(unknown): Try logging the time as well along with the error.
class ErrorBuffer {
 public:
  ErrorBuffer() {}
  virtual ~ErrorBuffer() {}

  // Adds an error to errors_ in a thread-safe way, while making sure the size
  // never goes above a limit.
  void AddError(const ::util::Status& error, const std::string& msg_to_prepend,
                gtl::source_location location) LOCKS_EXCLUDED(lock_);

  // An overloaded version of AddError with no msg_to_prepend.
  void AddError(const ::util::Status& error, gtl::source_location location)
      LOCKS_EXCLUDED(lock_);

  // Clears all the blocking errors in a thread-safe way.
  void ClearErrors() LOCKS_EXCLUDED(lock_);

  // Returns the list of errors.
  std::vector<::util::Status> GetErrors() const LOCKS_EXCLUDED(lock_);

  // Whether there is any error saved in the buffer.
  bool ErrorExists() const LOCKS_EXCLUDED(lock_);

  // ErrorBuffer is neither copyable nor movable.
  ErrorBuffer(const ErrorBuffer&) = delete;
  ErrorBuffer& operator=(const ErrorBuffer&) = delete;

 private:
  // Mutex lock for protecting the internal blocking error vector.
  mutable absl::Mutex lock_;

  // A vector of all the blocking (aka critical) errors HAL has encountered.
  std::vector<::util::Status> errors_ GUARDED_BY(lock_);
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_ERROR_BUFFER_H_
