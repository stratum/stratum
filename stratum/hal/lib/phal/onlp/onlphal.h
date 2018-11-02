/*
 * Copyright 2018 Google LLC
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


#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLPHAL_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLPHAL_H_

#include <functional>

#include "stratum/hal/lib/common/phal_interface.h"
#include "absl/synchronization/mutex.h"
//#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
#include "stratum/hal/lib/phal/onlp/onlp_event_handler.h"
#include "stratum/hal/lib/phal/onlp/sfp_datasource.h"



namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class Onlphal;

// Implements a callback for status changes on ONLP SFPs.
class OnlphalSfpEventCallback : public OnlpSfpEventCallback {
 public:
  // Creates a new OnlphalSfpEventCallback that receives callbacks for status
  // changes that occur for any SFPs.
  OnlphalSfpEventCallback() : onlphal_(nullptr) {};
  OnlphalSfpEventCallback(const OnlphalSfpEventCallback& other) = delete;
  OnlphalSfpEventCallback& operator=(const OnlphalSfpEventCallback& other) = 
      delete;
  ~OnlphalSfpEventCallback() override {};

  // Callback for handling SFP status changes - SFP plug/unplug events.
  ::util::Status HandleSfpStatusChange(const OidInfo& oid_info) override;

 private:
  friend class Onlphal;
  Onlphal* onlphal_;
};

// Class "Onlphal" is an implementation of PhalInterface which is used to
// send the Onlphal events to Stratum.
class Onlphal : public PhalInterface {
 public:
  ~Onlphal() override;

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
  ::util::Status SetPortLedState(int slot, int port, int channel,
                                 LedColor color, LedState state) override
      LOCKS_EXCLUDED(config_lock_);

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static Onlphal* CreateSingleton() LOCKS_EXCLUDED(config_lock_);

  // Onlphal is neither copyable nor movable.
  Onlphal(const Onlphal&) = delete;
  Onlphal& operator=(const Onlphal&) = delete;

  // Write Transceiver Events
  ::util::Status WriteTransceiverEvent(const TransceiverEvent& event);

 private:
  friend class OnlphalTest;

  // Private constructor.
  Onlphal();

  // One time initialization of the OnlpWrapper
  ::util::Status InitializeOnlpInterface();

  // One time initialization of the OnlpEventHandler. Need to be called after
  // InitializeOnlpWrapper() completes successfully.
  ::util::Status InitializeOnlpEventHandler();

  // One time initialization of the data sources. Need to be called after
  // InitializeOnlpWrapper() completes successfully.
  // TODO: move it to OnlpConfigurator
  ::util::Status InitializeOnlpOids();

  // Internal mutex lock for protecting the internal maps and initializing the
  // singleton instance.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static Onlphal* singleton_ GUARDED_BY(init_lock_);

  // Mutex lock for protecting the internal state when config is pushed or the
  // class is initialized so that other threads do not access the state while
  // the are being changed.
  mutable absl::Mutex config_lock_;

  // Determines if PHAL is fully initialized.
  bool initialized_ = false GUARDED_BY(config_lock_);

  // Writers to forward the Transceiver events to. They are registered by
  // external manager classes to receive the SFP Transceiver events. The managers
  // can be running in different threads. The is sorted based on the
  // the priority of the TrasnceiverEventWriter intances.
  std::multiset<TransceiverEventWriter, TransceiverEventWriterComp>
      transceiver_event_writers_ GUARDED_BY(config_lock_);

  std::unique_ptr<OnlpInterface> onlp_interface_;
  std::unique_ptr<OnlpEventHandler> onlp_event_handler_;

  // SFP Event Callback
  std::unique_ptr<OnlphalSfpEventCallback> sfp_event_callback_;

  // Map from std::pair<int, int> representing (slot, port) of singleton port
  // to the vector of OnlpOid
  std::map<std::pair<int, int>, std::shared_ptr<OnlpSfpDataSource>> 
      slot_port_to_sfp_data_;
};

}  //namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLPHAL_H_
