// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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

#include "stratum/hal/lib/phal/onlp/onlpphal.h"

#include <string>

#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
#include "stratum/hal/lib/phal/onlp/switch_configurator.h"
#include "stratum/hal/lib/phal/sfp_adapter.h"
#include "stratum/lib/macros.h"

DEFINE_int32(max_num_transceiver_writers, 2,
             "Maximum number of channel writers for transceiver events.");

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using TransceiverEvent = PhalInterface::TransceiverEvent;
using TransceiverEventWriter = PhalInterface::TransceiverEventWriter;

OnlpPhal* OnlpPhal::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex OnlpPhal::init_lock_(absl::kConstInit);

::util::Status OnlpPhalSfpEventCallback::HandleStatusChange(
    const OidInfo& oid_info) {
  // Check OID Type
  switch (oid_info.GetType()) {
    // SFP event
    case ONLP_OID_TYPE_SFP:
      // Format TransceiverEvent
      TransceiverEvent event;
      event.slot = kDefaultSlot;
      event.port = oid_info.GetId();
      event.state = oid_info.GetHardwareState();
      RETURN_IF_ERROR(onlpphal_->HandleTransceiverEvent(event));
      break;

    // TODO(craig): we probably need to handle more than just
    //              transceiver events over time.
    default:
      return MAKE_ERROR() << "unhandled status change, oid: "
                          << oid_info.GetHeader()->id;
  }

  return ::util::OkStatus();
}

OnlpPhal::OnlpPhal()
    : onlp_interface_(nullptr),
      onlp_event_handler_(nullptr),
      sfp_event_callback_(nullptr) {}

OnlpPhal::~OnlpPhal() {}

// Initialize the onlp interface and phal DB
::util::Status OnlpPhal::Initialize() {
  absl::WriterMutexLock l(&config_lock_);

  if (!initialized_) {
    // Create the OnlpWrapper object
    RETURN_IF_ERROR(InitializeOnlpInterface());

    // Create attribute database and load initial phal DB
    RETURN_IF_ERROR(InitializePhalDB());

    // Create the OnlpEventHandler object with given OnlpWrapper
    RETURN_IF_ERROR(InitializeOnlpEventHandler());

    initialized_ = true;
  }
  return ::util::OkStatus();
}

::util::Status OnlpPhal::InitializePhalDB() {
  // Create onlp switch configurator instance
  ASSIGN_OR_RETURN(auto configurator,
                   OnlpSwitchConfigurator::Make(this, onlp_interface_.get()));

  // Create attribute database and load initial phal DB
  ASSIGN_OR_RETURN(std::move(database_),
                   AttributeDatabase::MakePhalDB(std::move(configurator)));

  // Create and run PhalDb service
  phal_db_service_ = absl::make_unique<PhalDbService>(database_.get());
  phal_db_service_->Run();

  return ::util::OkStatus();
}

::util::Status OnlpPhal::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&config_lock_);

  // TODO(unknown): Process Chassis Config here

  return ::util::OkStatus();
}

::util::Status OnlpPhal::VerifyChassisConfig(const ChassisConfig& config) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status OnlpPhal::Shutdown() {
  absl::WriterMutexLock l(&config_lock_);

  // TODO(unknown): add clean up code
  ::util::Status status;
  APPEND_STATUS_IF_ERROR(status, phal_db_service_->Teardown());

  initialized_ = false;

  return status;
}

::util::Status OnlpPhal::HandleTransceiverEvent(const TransceiverEvent& event) {
  // Send event to Sfp configurator first to ensure
  // attribute database is in order before and calls are
  // made from the upper layer components.
  const auto slot_port_pair = std::make_pair(event.slot, event.port);
  auto configurator =
      gtl::FindPtrOrNull(slot_port_to_configurator_, slot_port_pair);

  // Check to make sure we've got a configurator for this slot/port
  CHECK_RETURN_IF_FALSE(configurator != nullptr)
      << "card[" << event.slot << "]/port[" << event.port << "]: "
      << "no configurator for this transceiver";

  RETURN_IF_ERROR(configurator->HandleEvent(event.state));

  // Write the event to each writer
  return WriteTransceiverEvent(event);
}

::util::StatusOr<int> OnlpPhal::RegisterTransceiverEventWriter(
    std::unique_ptr<ChannelWriter<TransceiverEvent>> writer, int priority) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  CHECK_RETURN_IF_FALSE(transceiver_event_writers_.size() <
                        static_cast<size_t>(FLAGS_max_num_transceiver_writers))
      << "Can only support " << FLAGS_max_num_transceiver_writers
      << " transceiver event Writers.";

  // Find the next available ID for the Writer.
  int next_id = kInvalidWriterId;
  for (int id = 1;
       id <= static_cast<int>(transceiver_event_writers_.size()) + 1; ++id) {
    auto it = std::find_if(
        transceiver_event_writers_.begin(), transceiver_event_writers_.end(),
        [id](const TransceiverEventWriter& w) { return w.id == id; });
    if (it == transceiver_event_writers_.end()) {
      // This id is free. Pick it up.
      next_id = id;
      break;
    }
  }
  CHECK_RETURN_IF_FALSE(next_id != kInvalidWriterId)
      << "Could not find a new ID for the Writer. next_id=" << next_id << ".";

  transceiver_event_writers_.insert({std::move(writer), priority, next_id});

  // Note: register callback only after writer registered. Only register
  //       callback once.
  if (sfp_event_callback_ == nullptr) {
    // Create OnlpSfpEventCallback
    std::unique_ptr<OnlpPhalSfpEventCallback> callback(
        new OnlpPhalSfpEventCallback());
    sfp_event_callback_ = std::move(callback);
    sfp_event_callback_->onlpphal_ = this;

    // Register OnlpSfpEventCallback
    ::util::Status result = onlp_event_handler_->RegisterSfpEventCallback(
        sfp_event_callback_.get());
    CHECK_RETURN_IF_FALSE(result.ok())
        << "Failed to register SFP event callback.";
  }

  return next_id;
}

::util::Status OnlpPhal::UnregisterTransceiverEventWriter(int id) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  auto it = std::find_if(
      transceiver_event_writers_.begin(), transceiver_event_writers_.end(),
      [id](const TransceiverEventWriter& h) { return h.id == id; });
  CHECK_RETURN_IF_FALSE(it != transceiver_event_writers_.end())
      << "Could not find a transceiver event Writer with ID " << id << ".";

  transceiver_event_writers_.erase(it);

  // Unregister OnlpSfpEventCallback if no more writer registered
  if (transceiver_event_writers_.size() == 0 &&
      sfp_event_callback_ != nullptr) {
    ::util::Status result = onlp_event_handler_->UnregisterSfpEventCallback(
        sfp_event_callback_.get());
    CHECK_RETURN_IF_FALSE(result.ok())
        << "Failed to unregister SFP event callback.";
    sfp_event_callback_ = nullptr;
  }

  return ::util::OkStatus();
}

::util::Status OnlpPhal::GetFrontPanelPortInfo(
    int slot, int port, FrontPanelPortInfo* fp_port_info) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  // translate slot/port to card_id/port_id for the PhalDB
  const auto slot_port_pair = std::make_pair(slot, port);
  // Check to make sure we've got a configurator for this slot/port
  auto configurator =
      gtl::FindPtrOrNull(slot_port_to_configurator_, slot_port_pair);
  CHECK_RETURN_IF_FALSE(configurator != nullptr)
      << "No configurator for "
      << "slot " << slot << " port " << port << ".";

  auto card_id = configurator->GetCardId();
  auto port_id = configurator->GetPortId();

  // Call the SfpAdapter to query the Phal Attribute DB
  SfpAdapter adapter(database_.get());
  return adapter.GetFrontPanelPortInfo(card_id, port_id, fp_port_info);
}

OnlpPhal* OnlpPhal::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);

  if (!singleton_) {
    singleton_ = new OnlpPhal();
    singleton_->Initialize();
  }

  return singleton_;
}

::util::Status OnlpPhal::WriteTransceiverEvent(const TransceiverEvent& event) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  for (auto it = transceiver_event_writers_.begin();
       it != transceiver_event_writers_.end(); ++it) {
    it->writer->Write(event, absl::InfiniteDuration());
  }

  return ::util::OkStatus();
}

::util::Status OnlpPhal::SetPortLedState(int slot, int port, int channel,
                                         LedColor color, LedState state) {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

::util::Status OnlpPhal::InitializeOnlpInterface() {
  // Create the OnlpInterface object
  ASSIGN_OR_RETURN(onlp_interface_, OnlpWrapper::Make());
  return ::util::OkStatus();
}

::util::Status OnlpPhal::InitializeOnlpEventHandler() {
  // Create the OnlpEventHandler object
  ASSIGN_OR_RETURN(onlp_event_handler_,
                   OnlpEventHandler::Make(onlp_interface_.get()));
  return ::util::OkStatus();
}

// Register the configurator so we can use later
::util::Status OnlpPhal::RegisterSfpConfigurator(
    int slot, int port, SfpConfigurator* configurator) {
  const auto slot_port_pair = std::make_pair(slot, port);

  auto onlp_configurator = dynamic_cast<OnlpSfpConfigurator*>(configurator);
  CHECK_RETURN_IF_FALSE(onlp_configurator != nullptr)
      << "Can't register configurator for slot " << slot << " port " << port
      << " because it is not of OnlpSfpConfigurator class";

  CHECK_RETURN_IF_FALSE(gtl::InsertIfNotPresent(
      &slot_port_to_configurator_, slot_port_pair, onlp_configurator))
      << "slot: " << slot << " port: " << port << " already registered";

  return ::util::OkStatus();
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
