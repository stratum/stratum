// Copyright 2020-present Open Networking Foundation
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

#include "stratum/hal/lib/phal/tai/tai_wrapper.h"

#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/strip.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

TaiWrapper* TaiWrapper::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex TaiWrapper::init_lock_(absl::kConstInit);

TaiWrapper* TaiWrapper::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new TaiWrapper();
  }

  return singleton_;
}

TaiWrapper* TaiWrapper::GetSingleton() {
  absl::ReaderMutexLock l(&init_lock_);
  return singleton_;
}

namespace {
int HelperFunction(int a) { return a + TaiWrapper::kSomeConstant; }
}  // namespace

::util::StatusOr<int> TaiWrapper::GetFooInfo(int port) const {
  return MAKE_ERROR(ERR_UNIMPLEMENTED) << "Not implemented.";
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
