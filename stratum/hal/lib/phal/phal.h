// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_PHAL_H_
#define STRATUM_HAL_LIB_PHAL_PHAL_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/optics_adapter.h"
#include "stratum/hal/lib/phal/phal_backend_interface.h"
#include "stratum/hal/lib/phal/sfp_adapter.h"

namespace stratum {
namespace hal {
namespace phal {

// Class "Phal" is an implementation of PhalInterface. It provides an interface
// to the system hardware. It delegates calls to the specific implementations.
class Phal : public PhalInterface {
 public:
  ~Phal() override;

  // PhalInterface public methods.
  ::util::Status PushChassisConfig(const ChassisConfig& config) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status VerifyChassisConfig(const ChassisConfig& config) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status Shutdown() override LOCKS_EXCLUDED(config_lock_);
  ::util::StatusOr<int> RegisterTransceiverEventWriter(
      std::unique_ptr<ChannelWriter<TransceiverEvent>> writer,
      int priority) override LOCKS_EXCLUDED(config_lock_);
  ::util::Status UnregisterTransceiverEventWriter(int id) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status GetFrontPanelPortInfo(
      int slot, int port, FrontPanelPortInfo* fp_port_info) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status GetOpticalTransceiverInfo(
      int module, int network_interface,
      OpticalTransceiverInfo* ot_info) override LOCKS_EXCLUDED(config_lock_);
  ::util::Status SetOpticalTransceiverInfo(
      int module, int network_interface,
      const OpticalTransceiverInfo& ot_info) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status SetPortLedState(int slot, int port, int channel,
                                 LedColor color, LedState state) override
      LOCKS_EXCLUDED(config_lock_);

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static Phal* CreateSingleton() LOCKS_EXCLUDED(config_lock_, init_lock_);

  // Phal is neither copyable nor movable.
  Phal(const Phal&) = delete;
  Phal& operator=(const Phal&) = delete;

 private:
  // Private constructor.
  Phal();

  // Internal mutex lock for initializing the singleton instance.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static Phal* singleton_ GUARDED_BY(init_lock_);

  // Mutex lock for protecting the internal state when config is pushed or the
  // class is initialized so that other threads do not access the state while
  // the are being changed.
  mutable absl::Mutex config_lock_;

  // Determines if Phal is fully initialized.
  bool initialized_ GUARDED_BY(config_lock_) = false;

  // Owned by this class.
  std::unique_ptr<AttributeDatabase> database_ GUARDED_BY(config_lock_);

  // Owned by this class.
  std::unique_ptr<SfpAdapter> sfp_adapter_ GUARDED_BY(config_lock_);

  // Owned by this class.
  std::unique_ptr<OpticsAdapter> optics_adapter_ GUARDED_BY(config_lock_);

  // Store backend interfaces for later Shutdown. Not owned by this class.
  std::vector<PhalBackendInterface*> phal_interfaces_ GUARDED_BY(config_lock_);
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum
#endif  // STRATUM_HAL_LIB_PHAL_PHAL_H_
