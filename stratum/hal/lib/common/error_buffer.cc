// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/common/error_buffer.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

DEFINE_int32(max_num_errors_to_track, 10,
             "Max number of error statuses to track/save in the buffer.");

namespace stratum {
namespace hal {

void ErrorBuffer::AddError(const ::util::Status& error,
                           const std::string& msg_to_prepend,
                           gtl::source_location location) {
  absl::WriterMutexLock l(&lock_);
  std::string error_message =
      absl::StrCat("(", BaseName(location.file_name()), ":", location.line(),
                   "): ", msg_to_prepend, error.error_message());
  LOG(ERROR) << error_message;
  if (static_cast<int>(errors_.size()) > FLAGS_max_num_errors_to_track) return;
  ::util::Status status = APPEND_ERROR(error.StripMessage()).without_logging()
                          << error_message;
  errors_.push_back(status);
}

// An overloaded version of AddError with no msg_to_prepend.
void ErrorBuffer::AddError(const ::util::Status& error,
                           gtl::source_location location) {
  AddError(error, "", location);
}

// Clears all the blocking errors in a thread-safe way.
void ErrorBuffer::ClearErrors() {
  absl::WriterMutexLock l(&lock_);
  errors_.clear();
}

std::vector<::util::Status> ErrorBuffer::GetErrors() const {
  absl::ReaderMutexLock l(&lock_);
  return errors_;
}

bool ErrorBuffer::ErrorExists() const {
  absl::ReaderMutexLock l(&lock_);
  return !errors_.empty();
}

}  // namespace hal
}  // namespace stratum
