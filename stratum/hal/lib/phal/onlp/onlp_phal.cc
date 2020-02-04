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

#include "stratum/hal/lib/phal/onlp/onlp_phal.h"

#include <string>

#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/onlp/onlp_switch_configurator.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

OnlpPhal* OnlpPhal::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex OnlpPhal::init_lock_(absl::kConstInit);

OnlpPhal::OnlpPhal() : onlp_interface_(nullptr), onlp_event_handler_(nullptr) {}

OnlpPhal::~OnlpPhal() {}

OnlpPhal* OnlpPhal::CreateSingleton(OnlpInterface* onlp_interface) {
  absl::WriterMutexLock l(&init_lock_);

  if (!singleton_) {
    singleton_ = new OnlpPhal();
  }

  auto status = singleton_->Initialize(onlp_interface);
  if (!status.ok()) {
    LOG(ERROR) << "OnlpPhal failed to initialize: " << status;
    delete singleton_;
    singleton_ = nullptr;
  }

  return singleton_;
}

// Initialize the onlp interface and phal DB
::util::Status OnlpPhal::Initialize(OnlpInterface* onlp_interface) {
  absl::WriterMutexLock l(&config_lock_);

  if (!initialized_) {
    CHECK_RETURN_IF_FALSE(onlp_interface != nullptr);
    onlp_interface_ = onlp_interface;

    // Create the OnlpEventHandler object
    ASSIGN_OR_RETURN(onlp_event_handler_,
                     OnlpEventHandler::Make(onlp_interface_));

    // Create attribute database and load initial phal DB
    ASSIGN_OR_RETURN(auto configurator,
                     OnlpSwitchConfigurator::Make(this, onlp_interface_));

    // Create attribute database and load initial phal DB
    ASSIGN_OR_RETURN(std::move(database_),
                     AttributeDatabase::MakePhalDB(std::move(configurator)));

    // Create SfpAdapter
    sfp_adapter_ = absl::make_unique<SfpAdapter>(database_.get());

    initialized_ = true;
  }
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

  onlp_interface_ = nullptr;
  onlp_event_handler_.reset();
  initialized_ = false;
  // Cancel subscriptions before deleting the database.
  sfp_adapter_.reset();
  database_.reset();

  return ::util::OkStatus();
}

::util::StatusOr<int> OnlpPhal::RegisterTransceiverEventWriter(
    std::unique_ptr<ChannelWriter<PhalInterface::TransceiverEvent>> writer,
    int priority) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return sfp_adapter_->RegisterSfpEventSubscriber(std::move(writer), priority);
}

::util::Status OnlpPhal::UnregisterTransceiverEventWriter(int id) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return sfp_adapter_->UnregisterSfpEventSubscriber(id);
}

::util::Status OnlpPhal::GetFrontPanelPortInfo(
    int slot, int port, FrontPanelPortInfo* fp_port_info) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return sfp_adapter_->GetFrontPanelPortInfo(slot, port, fp_port_info);
}

::util::Status OnlpPhal::SetPortLedState(int slot, int port, int channel,
                                         LedColor color, LedState state) {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

::util::Status OnlpPhal::RegisterOnlpEventCallback(
    OnlpEventCallback* callback) {
  CHECK_RETURN_IF_FALSE(onlp_event_handler_ != nullptr);

  return onlp_event_handler_->RegisterEventCallback(callback);
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
