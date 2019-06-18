// Copyright 2019 Dell EMC
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

#include "stratum/hal/lib/phal/onlp/onlpphal_mock.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

ABSL_CONST_INIT absl::Mutex OnlpPhalMock::init_lock_(absl::kConstInit);
OnlpPhalMock* OnlpPhalMock::singleton_ GUARDED_BY(OnlpPhalMock::init_lock_) =
    nullptr;

OnlpPhalMock::OnlpPhalMock() : onlp_interface_(nullptr) {}

OnlpPhalMock::~OnlpPhalMock() {}

// Note: don't call anything, leave that to the test function
::util::Status OnlpPhalMock::Initialize() {
  absl::WriterMutexLock l(&config_lock_);
  initialized_ = true;
  return ::util::OkStatus();
}

::util::Status OnlpPhalMock::InitializeOnlpInterface() {
  absl::WriterMutexLock l(&config_lock_);

  if (initialized_) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "InitializeOnlpInterface() can be called only before "
           << "the class is initialized";
  }

  // Create the OnlpInterface object
  ASSIGN_OR_RETURN(onlp_interface_, MockOnlpWrapper::Make());

  return ::util::OkStatus();
}

OnlpPhalMock* OnlpPhalMock::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);

  if (!singleton_) {
    singleton_ = new OnlpPhalMock();
    singleton_->Initialize();
  }

  return singleton_;
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
