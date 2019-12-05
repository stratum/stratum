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

#include "absl/base/macros.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

// Instances
DummyPhal* phal_singleton_ = nullptr;

DummyPhal::DummyPhal()
    : xcvr_event_writer_id_(kInvalidWriterId),
      dummy_box_(DummyBox::GetSingleton()) {}
DummyPhal::~DummyPhal() {}

::util::Status DummyPhal::PushChassisConfig(const ChassisConfig& config) {
  // TODO(Yi Tseng): Implement this function
  absl::WriterMutexLock l(&phal_lock_);
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::Status DummyPhal::VerifyChassisConfig(const ChassisConfig& config) {
  // TODO(Yi Tseng): Implement this function
  absl::ReaderMutexLock l(&phal_lock_);
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::Status DummyPhal::Shutdown() {
  // TODO(Yi Tseng): Implement this function
  absl::WriterMutexLock l(&phal_lock_);
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::StatusOr<int> DummyPhal::RegisterTransceiverEventWriter(
    std::unique_ptr<ChannelWriter<TransceiverEvent>> writer, int priority) {
  absl::ReaderMutexLock l(&phal_lock_);
  LOG(INFO) << __FUNCTION__;
  ASSIGN_OR_RETURN(
      xcvr_event_writer_id_,
      dummy_box_->RegisterTransceiverEventWriter(std::move(writer), priority));
  return ::util::OkStatus();
}

::util::Status DummyPhal::UnregisterTransceiverEventWriter(int id) {
  absl::ReaderMutexLock l(&phal_lock_);
  LOG(INFO) << __FUNCTION__;
  return dummy_box_->UnregisterTransceiverEventWriter(xcvr_event_writer_id_);
}

::util::Status DummyPhal::GetFrontPanelPortInfo(
    int slot, int port, FrontPanelPortInfo* fp_port_info) {
  absl::ReaderMutexLock l(&phal_lock_);
  // TODO(Yi Tseng): Implement this function and data store.
  LOG(INFO) << __FUNCTION__;
  fp_port_info->set_hw_state(HwState::HW_STATE_PRESENT);
  fp_port_info->set_vendor_name("Dummy vendor");
  std::ostringstream serial;
  serial << "dummy-" << slot << "-" << port;
  fp_port_info->set_serial_number(serial.str());
  fp_port_info->set_media_type(MEDIA_TYPE_QSFP_COPPER);
  fp_port_info->set_physical_port_type(PHYSICAL_PORT_TYPE_QSFP_CAGE);
  fp_port_info->set_part_number("dummy_part_no");
  return ::util::OkStatus();
}

::util::Status DummyPhal::SetPortLedState(int slot, int port, int channel,
                                          LedColor color, LedState state) {
  absl::ReaderMutexLock l(&phal_lock_);
  // TODO(Yi Tseng): Implement this function
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

// Register the configurator so we can use later
::util::Status DummyPhal::RegisterSfpConfigurator(
    int slot, int port, ::stratum::hal::phal::SfpConfigurator* configurator) {
  LOG(INFO) << __FUNCTION__;

  const std::pair<int, int> slot_port_pair = std::make_pair(slot, port);

  slot_port_to_configurator_[slot_port_pair] = configurator;

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
