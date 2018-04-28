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


#include "third_party/stratum/public/lib/error.h"

#include <string>

#include "util/task/status.h"

namespace stratum {

namespace error {

static const char kErrorSpaceName[] = "StratumErrorSpace";

::std::string StratumErrorSpace::space_name() { return kErrorSpaceName; }

::std::string StratumErrorSpace::code_to_string(int code) {
  if (!ErrorCode_IsValid(code)) {
    code = ERR_UNKNOWN;
  }
  return ErrorCode_Name(static_cast<ErrorCode>(code));
}

::util::error::Code StratumErrorSpace::canonical_code(
    const ::util::Status& status) {
  switch (status.error_code()) {
    case ERR_SUCCESS:
      return ::util::error::OK;
    case ERR_CANCELLED:
      return ::util::error::CANCELLED;
    case ERR_UNKNOWN:
      return ::util::error::UNKNOWN;
    case ERR_PERMISSION_DENIED:
      return ::util::error::PERMISSION_DENIED;
    case ERR_ABORTED:
      return ::util::error::ABORTED;
    case ERR_DATA_LOSS:
      return ::util::error::DATA_LOSS;
    case ERR_UNAUTHENTICATED:
      return ::util::error::UNAUTHENTICATED;
    case ERR_INTERNAL:
    case ERR_HARDWARE_ERROR:
      return ::util::error::INTERNAL;
    case ERR_INVALID_PARAM:
    case ERR_INVALID_P4_INFO:
      return ::util::error::INVALID_ARGUMENT;
    case ERR_OPER_TIMEOUT:
      return ::util::error::DEADLINE_EXCEEDED;
    case ERR_ENTRY_NOT_FOUND:
      return ::util::error::NOT_FOUND;
    case ERR_ENTRY_EXISTS:
      return ::util::error::ALREADY_EXISTS;
    case ERR_UNIMPLEMENTED:
    case ERR_OPER_NOT_SUPPORTED:
    case ERR_OPER_DISABLED:
      return ::util::error::UNIMPLEMENTED;
    case ERR_FEATURE_UNAVAILABLE:
      return ::util::error::UNAVAILABLE;
    case ERR_NO_RESOURCE:
      return ::util::error::RESOURCE_EXHAUSTED;
    case ERR_FAILED_PRECONDITION:
    case ERR_NOT_INITIALIZED:
      return ::util::error::FAILED_PRECONDITION;
    case ERR_OUT_OF_RANGE:
    case ERR_TABLE_FULL:
    case ERR_TABLE_EMPTY:
      return ::util::error::OUT_OF_RANGE;
    default:
      return ::util::error::UNKNOWN;  // Default error.
  }
}

}  // namespace error

// Singleton StratumErrorSpace.
const ::util::ErrorSpace* StratumErrorSpace() {
  return error::StratumErrorSpace::Get();
}

// Force registering of the errorspace at run-time.
static const ::util::ErrorSpace* dummy __attribute__((unused)) =
    StratumErrorSpace();

}  // namespace stratum
