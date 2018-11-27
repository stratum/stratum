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


#include "stratum/hal/lib/dummy/dummy_phal.h"

#include <memory>
#include <utility>

#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/macros.h"
#include "absl/base/macros.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

// Global lock for Dummy PHAL
::absl::Mutex phal_lock_;

// Instances
DummyPhal* phal_singleton_ = nullptr;

DummyPhal::DummyPhal():
  xcvr_event_writer_id_(kInvalidWriterId),
  dummy_sdk_(DummySDK::GetSingleton()) {}
DummyPhal::~DummyPhal() {}

::util::Status DummyPhal::PushChassisConfig(const ChassisConfig& config) {
  // TODO(Yi Tseng): Implement this function
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::Status DummyPhal::VerifyChassisConfig(const ChassisConfig& config) {
  // TODO(Yi Tseng): Implement this function
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::Status DummyPhal::Shutdown() {
  // TODO(Yi Tseng): Implement this function
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::StatusOr<int> DummyPhal::RegisterTransceiverEventWriter(
    std::unique_ptr<ChannelWriter<TransceiverEvent>> writer, int priority) {
  LOG(INFO) << __FUNCTION__;
  ASSIGN_OR_RETURN(xcvr_event_writer_id_,
      dummy_sdk_->RegisterTransceiverEventWriter(std::move(writer), priority));
  return ::util::OkStatus();
}

::util::Status DummyPhal::UnregisterTransceiverEventWriter(int id) {
  LOG(INFO) << __FUNCTION__;
  return dummy_sdk_->UnregisterTransceiverEventWriter(xcvr_event_writer_id_);
}

::util::Status DummyPhal::GetFrontPanelPortInfo(
    int slot, int port, FrontPanelPortInfo* fp_port_info) {
  // TODO(Yi Tseng): Implement this function and data store.
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::Status DummyPhal::SetPortLedState(int slot, int port, int channel,
                                         LedColor color, LedState state) {
  // TODO(Yi Tseng): Implement this function
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

DummyPhal* DummyPhal::CreateSingleton() {
  LOG(INFO) << __FUNCTION__;
  if (phal_singleton_ == nullptr) {
    phal_singleton_ = new DummyPhal();
  }
  return phal_singleton_;
}

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

