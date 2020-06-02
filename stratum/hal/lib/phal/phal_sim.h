// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_PHAL_PHAL_SIM_H_
#define STRATUM_HAL_LIB_PHAL_PHAL_SIM_H_

#include <functional>
#include <set>
#include <map>
#include <memory>
#include <utility>

#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/sfp_configurator.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {

// Class "PhalSim" is an implementation of PhalInterface which is used to
// simulate the PHAL events to Stratum.
class PhalSim : public PhalInterface {
 public:
  ~PhalSim() override;

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
      int module, int network_interface, OpticalTransceiverInfo* ot_info)
      override LOCKS_EXCLUDED(config_lock_);
  ::util::Status SetOpticalTransceiverInfo(
      int module, int network_interface, const OpticalTransceiverInfo& ot_info)
      override LOCKS_EXCLUDED(config_lock_);
  ::util::Status SetPortLedState(int slot, int port, int channel,
                                 LedColor color, LedState state) override
      LOCKS_EXCLUDED(config_lock_);

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static PhalSim* CreateSingleton() LOCKS_EXCLUDED(config_lock_);

  // PhalSim is neither copyable nor movable.
  PhalSim(const PhalSim&) = delete;
  PhalSim& operator=(const PhalSim&) = delete;

 private:
  // Private constructor.
  PhalSim();

  static constexpr int kMaxNumTransceiverEventWriters = 8;

  // Internal mutex lock for protecting the internal maps and initializing the
  // singleton instance.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static PhalSim* singleton_ GUARDED_BY(init_lock_);

  // Mutex lock for protecting the internal state when config is pushed or the
  // class is initialized so that other threads do not access the state while
  // the are being changed.
  mutable absl::Mutex config_lock_;

  // Writers to forward the Transceiver events to. They are registered by
  // external manager classes to receive the SFP Transceiver events. The
  // managers can be running in different threads. The is sorted based on the
  // the priority of the TransceiverEventWriter intances.
  std::multiset<TransceiverEventWriter, TransceiverEventWriterComp>
      transceiver_event_writers_ GUARDED_BY(config_lock_);

  // Map from std::pair<int, int> representing (slot, port) of singleton port
  // to the vector of sfp datasource id
  std::map<std::pair<int, int>,
    ::stratum::hal::phal::SfpConfigurator*> slot_port_to_configurator_;

  // Determines if PHAL is fully initialized.
  bool initialized_ GUARDED_BY(config_lock_);
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_PHAL_SIM_H_
