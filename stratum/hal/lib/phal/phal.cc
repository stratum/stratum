// Copyright 2020-present Open Networking Foundation
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

#include "stratum/hal/lib/phal/phal.h"

#include <string>
#include <vector>

#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/switch_configurator_interface.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

#if defined(WITH_ONLP)
#include "stratum/hal/lib/phal/onlp/onlp_phal.h"
#include "stratum/hal/lib/phal/onlp/onlp_switch_configurator.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
#endif  // defined(WITH_ONLP)

#if defined(WITH_TAI)
#include "stratum/hal/lib/phal/tai/tai_phal.h"
#include "stratum/hal/lib/phal/tai/tai_switch_configurator.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_manager.h"
#endif  // defined(WITH_TAI)

DECLARE_string(phal_config_path);

namespace stratum {
namespace hal {
namespace phal {

using TransceiverEvent = PhalInterface::TransceiverEvent;
using TransceiverEventWriter = PhalInterface::TransceiverEventWriter;

Phal* Phal::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex Phal::init_lock_(absl::kConstInit);

Phal::Phal() : database_(nullptr) {}

Phal::~Phal() {}

Phal* Phal::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);

  if (!singleton_) {
    singleton_ = new Phal();
  }

  return singleton_;
}

::util::Status Phal::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&config_lock_);

  if (!initialized_) {
    // Do init stuff here
    std::unique_ptr<AttributeGroup> root_group =
        AttributeGroup::From(PhalDB::descriptor());
    std::vector<std::unique_ptr<SwitchConfiguratorInterface>> configurators;

    // Set up ONLP
#if defined(WITH_ONLP)
    {
      auto* onlp_wrapper = onlp::OnlpWrapper::CreateSingleton();
      auto* onlp_phal = onlp::OnlpPhal::CreateSingleton(onlp_wrapper);
      phal_interfaces_.push_back(onlp_phal);
      ASSIGN_OR_RETURN(auto configurator, onlp::OnlpSwitchConfigurator::Make(
                                              onlp_phal, onlp_wrapper));
      configurators.push_back(std::move(configurator));
    }
#endif  // defined(WITH_ONLP)

#if defined(WITH_TAI)
    {
      auto* tai_manager = tai::TAIManager::CreateSingleton();
      auto* tai_phal = tai::TaiPhal::CreateSingleton(tai_manager);
      tai_phal->PushChassisConfig(config);
      phal_interfaces_.push_back(tai_phal);
      ASSIGN_OR_RETURN(auto configurator,
                       tai::TaiSwitchConfigurator::Make(tai_manager));
      configurators.push_back(std::move(configurator));
    }
#endif  // defined(WITH_TAI)

    PhalInitConfig phal_config;
    if (FLAGS_phal_config_path.empty()) {
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
          ReadProtoFromTextFile(FLAGS_phal_config_path, &phal_config));
    }

    // Now load the config into the attribute database
    for (const auto& configurator : configurators) {
      RETURN_IF_ERROR(
          configurator->ConfigurePhalDB(&phal_config, root_group.get()));
    }

    // Create attribute database
    ASSIGN_OR_RETURN(std::move(database_),
                     AttributeDatabase::MakePhalDb(std::move(root_group)));

    // Create SfpAdapter
    sfp_adapter_ = absl::make_unique<SfpAdapter>(database_.get());

    // Create OpticsAdapter
    optics_adapter_ = absl::make_unique<OpticsAdapter>(database_.get());

    initialized_ = true;
  }

  // TODO(unknown): Process Chassis Config here

  return ::util::OkStatus();
}

::util::Status Phal::VerifyChassisConfig(const ChassisConfig& config) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status Phal::Shutdown() {
  absl::WriterMutexLock l(&config_lock_);

  sfp_adapter_.reset();

  for (const auto& phal_interface : phal_interfaces_) {
    phal_interface->Shutdown();
  }
  phal_interfaces_.clear();

  // Delete database last.
  database_.reset();
  initialized_ = false;

  return ::util::OkStatus();
}

::util::StatusOr<int> Phal::RegisterTransceiverEventWriter(
    std::unique_ptr<ChannelWriter<PhalInterface::TransceiverEvent>> writer,
    int priority) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return sfp_adapter_->RegisterSfpEventSubscriber(std::move(writer), priority);
}

::util::Status Phal::UnregisterTransceiverEventWriter(int id) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return sfp_adapter_->UnregisterSfpEventSubscriber(id);
}

::util::Status Phal::GetFrontPanelPortInfo(int slot, int port,
                                           FrontPanelPortInfo* fp_port_info) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return sfp_adapter_->GetFrontPanelPortInfo(slot, port, fp_port_info);
}

::util::Status Phal::GetOpticalTransceiverInfo(int slot, int port,
                                               OpticalChannelInfo* oc_info) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return optics_adapter_->GetOpticalTransceiverInfo(slot, port, oc_info);
}

::util::Status Phal::SetOpticalTransceiverInfo(
    int slot, int port, const OpticalChannelInfo& oc_info) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return optics_adapter_->SetOpticalTransceiverInfo(slot, port, oc_info);
}

::util::Status Phal::SetPortLedState(int slot, int port, int channel,
                                     LedColor color, LedState state) {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
