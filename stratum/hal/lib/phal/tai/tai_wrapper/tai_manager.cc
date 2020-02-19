/*
 * Copyright 2020-present Open Networking Foundation
 * Copyright 2020 PLVision
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

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_manager.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

TAIManager* TAIManager::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex TAIManager::init_lock_(absl::kConstInit);

TAIManager* TAIManager::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) singleton_ = new TAIManager(absl::make_unique<TAIWrapper>());

  return singleton_;
}

TAIManager* TAIManager::GetSingleton() {
  absl::ReaderMutexLock l(&init_lock_);
  return singleton_;
}

/*!
 * \brief TAIManager::IsObjectValid check is \param path is valid
 * \return true if valid
 * \note Thread-safe. A non-const method because of TAI wrapper mutex lock.
 */
bool TAIManager::IsObjectValid(const TAIPath& path) {
  // Lock the TAI wrapper mutex first to get data thread-safely.
  bool result;
  {
    absl::ReaderMutexLock wrapper_lock(&tai_wrapper_mutex_);
    result = tai_wrapper_->IsObjectValid(path);
  }  // Interaction with TAI wrapper (and objects owned by it) is finished.

  return result;
}

TAIManager::TAIManager(std::unique_ptr<TAIWrapperInterface> wrapper)
    : tai_wrapper_(std::move(wrapper)) {}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
