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

#if defined(WITH_TAI)
#include "stratum/hal/lib/phal/tai/tai_phal.h"
#include "stratum/hal/lib/phal/tai/tai_switch_configurator.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_manager.h"
#include "stratum/lib/utils.h"
#endif  // defined(WITH_TAI)

namespace stratum {
namespace hal {
namespace dummy_switch {

DEFINE_string(phal_config_path1, "",
              "The path to read the PhalInitConfig proto file from.");

// Instances
DummyPhal* phal_singleton_ = nullptr;

DummyPhal::DummyPhal()
    : xcvr_event_writer_id_(kInvalidWriterId),
      dummy_box_(DummyBox::GetSingleton()) {}
DummyPhal::~DummyPhal() {}

::util::Status DummyPhal::PushChassisConfig(const ChassisConfig& config) {
  // TODO(Yi Tseng): Implement this function
  absl::WriterMutexLock l(&phal_lock_);

  if (!initialized_) {
    // Do init stuff here
    std::unique_ptr<stratum::hal::phal::AttributeGroup> root_group =
        stratum::hal::phal::AttributeGroup::From(
            stratum::hal::phal::PhalDB::descriptor());
    std::vector<
        std::unique_ptr<stratum::hal::phal::SwitchConfiguratorInterface>>
        configurators;

#if defined(WITH_TAI)
    {
      auto* tai_manager =
          stratum::hal::phal::tai::TAIManager::CreateSingleton();
      auto* tai_phal =
          stratum::hal::phal::tai::TaiPhal::CreateSingleton(tai_manager);

      // Push chassis config to TAI PHAL to be able to convert node/port to the
      // related module/netif id.
      tai_phal->PushChassisConfig(config);
      node_port_id_to_module_netif_id_ = [tai_phal](
          uint64 node_id, uint32 port_id)
          -> ::util::StatusOr<std::pair<uint32, uint32>> {
        return tai_phal->GetRelatedTAIModuleAndNetworkId(node_id, port_id);
      };

      phal_interfaces_.push_back(tai_phal);
      ASSIGN_OR_RETURN(
          auto configurator,
          stratum::hal::phal::tai::TaiSwitchConfigurator::Make(tai_manager));
      configurators.push_back(std::move(configurator));
    }
#else
    {
      // TAI disabled. Set error message as return-result.
      node_port_id_to_module_netif_id_ = [](
          uint64 /*node_id*/, uint32 /*port_id*/)
          -> ::util::StatusOr<std::pair<uint32, uint32>> {
        return MAKE_ERROR(ERR_INTERNAL) << "TAI is not initialized!";
      };
    }
#endif  // defined(WITH_TAI)

    PhalInitConfig phal_config;
    if (FLAGS_phal_config_path1.empty()) {
      if (configurators.empty()) {
        LOG(ERROR)
            << "No phal_config_path specified and no switch configurator "
               "found! This is probably not what you want. Did you forget to "
               "specify any '--define phal_with_*=true' Bazel flags?";
      }
      for (const auto& configurator : configurators) {
        RETURN_IF_ERROR(configurator->CreateDefaultConfig(&phal_config));
      }
    } else {
      RETURN_IF_ERROR(
          ReadProtoFromTextFile(FLAGS_phal_config_path1, &phal_config));
    }

    // Now load the config into the attribute database
    for (const auto& configurator : configurators) {
      RETURN_IF_ERROR(
          configurator->ConfigurePhalDB(&phal_config, root_group.get()));
    }

    // Create attribute database
    ASSIGN_OR_RETURN(std::move(database_),
                     stratum::hal::phal::AttributeDatabase::MakePhalDb(
                         std::move(root_group)));

    // Create OpticAdapter
    optics_adapter_ =
        absl::make_unique<stratum::hal::phal::OpticsAdapter>(database_.get());

    initialized_ = true;
  }

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

::util::Status DummyPhal::GetOpticalTransceiverInfo(
    uint64 node_id, uint32 port_id, OpticalChannelInfo* oc_info) {
  if (!initialized_)
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";

  const auto status_or_module_netif_id
      = node_port_id_to_module_netif_id_(node_id, port_id);

  if (!status_or_module_netif_id.ok())
    return status_or_module_netif_id.status();

  const auto module_netif_id = status_or_module_netif_id.ValueOrDie();
  return optics_adapter_->GetOpticalTransceiverInfo(
      module_netif_id.first, module_netif_id.second, oc_info);
}

::util::Status DummyPhal::SetOpticalTransceiverInfo(
    uint64 node_id, uint32 port_id, const OpticalChannelInfo& oc_info) {
  if (!initialized_)
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";

  const auto status_or_module_netif_id
      = node_port_id_to_module_netif_id_(node_id, port_id);

  if (!status_or_module_netif_id.ok())
    return status_or_module_netif_id.status();

  const auto module_netif_id = status_or_module_netif_id.ValueOrDie();

  return optics_adapter_->SetOpticalTransceiverInfo(
      module_netif_id.first, module_netif_id.second, oc_info);
}

::util::Status DummyPhal::SetPortLedState(int slot, int port, int channel,
                                          LedColor color, LedState state) {
  absl::ReaderMutexLock l(&phal_lock_);
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
