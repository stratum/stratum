// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_PHAL_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_PHAL_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/onlp/onlp_event_handler.h"
#include "stratum/hal/lib/phal/onlp/onlp_phal_interface.h"
#include "stratum/hal/lib/phal/onlp/onlp_sfp_configurator.h"
#include "stratum/hal/lib/phal/onlp/onlp_sfp_datasource.h"
#include "stratum/hal/lib/phal/sfp_adapter.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

// Class "OnlpPhal" is an implementation of PhalInterface which is used to
// send the OnlpPhal events to Stratum.
class OnlpPhal final : public OnlpPhalInterface {
 public:
  ~OnlpPhal() override;

  // PhalInterface public methods.
  ::util::Status PushChassisConfig(const ChassisConfig& config) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status VerifyChassisConfig(const ChassisConfig& config) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status Shutdown() override LOCKS_EXCLUDED(config_lock_);
  ::util::Status RegisterOnlpEventCallback(OnlpEventCallback* callback)
      EXCLUSIVE_LOCKS_REQUIRED(config_lock_) override;

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static OnlpPhal* CreateSingleton(OnlpInterface* onlp_interface)
      LOCKS_EXCLUDED(config_lock_, init_lock_);

  // OnlpPhal is neither copyable nor movable.
  OnlpPhal(const OnlpPhal&) = delete;
  OnlpPhal& operator=(const OnlpPhal&) = delete;

//JR
  ::util::Status SetSfpFrequencyOnlp(uint32 port_number, uint32 frequency)
      LOCKS_EXCLUDED(config_lock_);

 private:
  friend class OnlpPhalCli;
  friend class OnlpPhalTest;
  friend class OnlpPhalMock;
  friend class OnlpSwitchConfiguratorTest;

  // Private constructor.
  OnlpPhal();

  // Calls all the one time start initialisations
  ::util::Status Initialize(OnlpInterface* onlp_interface)
      LOCKS_EXCLUDED(config_lock_);

  // One time initialization of the data sources. Need to be called after
  // InitializeOnlpWrapper() completes successfully.
  // TODO(unknown): move it to OnlpConfigurator
  ::util::Status InitializeOnlpOids() EXCLUSIVE_LOCKS_REQUIRED(config_lock_);

  // Internal mutex lock for protecting the internal maps and initializing the
  // singleton instance.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static OnlpPhal* singleton_ GUARDED_BY(init_lock_);

  // Mutex lock for protecting the internal state when config is pushed or the
  // class is initialized so that other threads do not access the state while
  // the are being changed.
  mutable absl::Mutex config_lock_;

  // Determines if PHAL is fully initialized.
  bool initialized_ GUARDED_BY(config_lock_) = false;

  // Not owned by this class.
  OnlpInterface* onlp_interface_ GUARDED_BY(config_lock_);

  // Owned by the class.
  std::unique_ptr<OnlpEventHandler> onlp_event_handler_
      GUARDED_BY(config_lock_);
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_PHAL_H_
