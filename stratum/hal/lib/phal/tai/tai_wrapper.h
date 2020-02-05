/*
 * Copyright 2020-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_H_

#include <bitset>
#include <memory>
#include <string>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

// A interface for TAI calls.
// This class wraps c-style direct TAI calls with Google-style c++ calls that
// return ::util::Status.
class TaiInterface {
 public:
  virtual ~TaiInterface() {}

  // Some function
  virtual ::util::StatusOr<int> GetFooInfo(int port) const = 0;

  // Get the link state of an optics module.
  virtual ::util::StatusOr<int> GetLinkState(int port) const = 0;
};

// An TaiInterface implementation that makes real calls into TAI.
// Note that this wrapper performs TAI setup and teardown, so only one may be
// allocated at any given time.
class TaiWrapper : public TaiInterface {
 public:
  // Public variable foo size in bar.
  static constexpr int kSomeConstant = 2;

  ::util::StatusOr<int> GetFooInfo(int port) const override;
  ::util::StatusOr<int> GetLinkState(int port) const override;

  // Creates a singleton instance.
  static TaiWrapper* CreateSingleton() LOCKS_EXCLUDED(init_lock_);
  ~TaiWrapper() = default;

  // Return the singleton instance to be used in the SDK callbacks.
  static TaiWrapper* GetSingleton() LOCKS_EXCLUDED(init_lock_);

  // TaiWrapper is neither copyable nor movable.
  TaiWrapper(const TaiWrapper& other) = delete;
  TaiWrapper& operator=(const TaiWrapper& other) = delete;

 private:
  // Private constructor.
  TaiWrapper() = default;

  // Some private variable.
  static constexpr absl::Duration kWriteTimeout = absl::InfiniteDuration();

  // RW mutex lock for protecting the TAI functions.
  mutable absl::Mutex tai_lock_;

  // The singleton instance.
  static TaiWrapper* singleton_ GUARDED_BY(init_lock_);

  // RW mutex lock for protecting the singleton instance initialization and
  // reading it back from other threads. Unlike other singleton classes, we
  // use RW lock as we need the pointer to class to be returned.
  static absl::Mutex init_lock_;
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_H_
