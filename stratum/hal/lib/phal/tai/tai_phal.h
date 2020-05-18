// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_PHAL_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_PHAL_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/phal_backend_interface.h"
#include "stratum/hal/lib/phal/tai/tai_interface.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

// Class "TaiPhal" is an implementation of PhalBackendInterface which is used to
// send the TaiPhal events to Stratum.
class TaiPhal final : public PhalBackendInterface {
 public:
  ~TaiPhal() override;

  // PhalInterface public methods.
  ::util::Status PushChassisConfig(const ChassisConfig& config) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status VerifyChassisConfig(const ChassisConfig& config) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status Shutdown() override LOCKS_EXCLUDED(config_lock_);

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static TaiPhal* CreateSingleton(TaiInterface* tai_interface)
      LOCKS_EXCLUDED(config_lock_, init_lock_);

  // TaiPhal is neither copyable nor movable.
  TaiPhal(const TaiPhal&) = delete;
  TaiPhal& operator=(const TaiPhal&) = delete;
  TaiPhal() = delete;

 private:
  // Private constructor.
  explicit TaiPhal(TaiInterface* tai_interface);

  // Calls all the one time start initializations.
  ::util::Status Initialize() LOCKS_EXCLUDED(config_lock_);

  // Internal mutex lock for protecting the internal maps and initializing the
  // singleton instance.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static TaiPhal* singleton_ GUARDED_BY(init_lock_);

  // Mutex lock for protecting the internal state when config is pushed or the
  // class is initialized so that other threads do not access the state while
  // the are being changed.
  mutable absl::Mutex config_lock_;

  // Determines if PHAL is fully initialized.
  bool initialized_ GUARDED_BY(config_lock_) = false;

  // The pointer for TaiInterface that allows TaiPhal to access TAI specific
  // features, for example, listen events from TAI.
  TaiInterface* tai_interface_;
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_PHAL_H_
