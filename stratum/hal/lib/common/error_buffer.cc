// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <libgen.h>
// FIXME(boc) google only
// #include "file/base/path.h"
// end google only
#include <string>

#include "gflags/gflags.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"

DEFINE_int32(max_num_errors_to_track, 10,
             "Max number of error statuses to track/save in the buffer.");

namespace stratum {
namespace hal {

std::string Basename(std::string path) {
  // file::Basename(location.file_name())
  char* c_path = strdup(path.c_str());
  return basename(c_path);
}

void ErrorBuffer::AddError(const ::util::Status& error,
                           const std::string& msg_to_prepend,
                           gtl::source_location location) {
  absl::WriterMutexLock l(&lock_);
  std::string error_message = absl::StrCat(
      "(", Basename(location.file_name()), ":", location.line(),
      "): ", msg_to_prepend, error.error_message());
  LOG(ERROR) << error_message;
  if (static_cast<int>(errors_.size()) > FLAGS_max_num_errors_to_track) return;
  // NOLINTNEXTLINE
  ::util::Status status = APPEND_ERROR(error.StripMessage())//.SetNoLogging() FIXME
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
