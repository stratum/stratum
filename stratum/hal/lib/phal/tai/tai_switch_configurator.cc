// Copyright 2020-present Open Networking Foundation
// Copyright 2020 PLVision
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

#include "stratum/hal/lib/phal/tai/tai_switch_configurator.h"

#include <string>
#include <utility>
#include <vector>

#include "stratum/glue/gtl/map_util.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/tai/tai_optics_datasource.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

/*!
 * \brief TaiSwitchConfigurator::Make method makes an instance of
 * TaiSwitchConfigurator
 */
::util::StatusOr<std::unique_ptr<TaiSwitchConfigurator>>
TaiSwitchConfigurator::Make() {
  return absl::WrapUnique(new TaiSwitchConfigurator());
}

/*!
 * \brief TaiSwitchConfigurator::CreateDefaultConfig method generates a default
 * config using the TAI API.
 */
::util::Status TaiSwitchConfigurator::CreateDefaultConfig(
    PhalInitConfig* phal_config) const {
  auto optical_card = phal_config->add_optical_cards();
  optical_card->set_slot(1);

  return ::util::OkStatus();
}

/*!
 * \brief TaiSwitchConfigurator::ConfigurePhalDB method configures the switch's
 * attribute database with the given PhalInitConfig config.
 */
::util::Status TaiSwitchConfigurator::ConfigurePhalDB(
    PhalInitConfig* phal_config, AttributeGroup* root) {
  auto mutable_root = root->AcquireMutable();

  ASSIGN_OR_RETURN(auto card_group,
                   mutable_root->AddRepeatedChildGroup("optical_cards"));

  auto mutable_card_group = card_group->AcquireMutable();
  for (auto& card : *phal_config->mutable_optical_cards()) {
    if (!card.has_cache_policy()) {
      card.set_allocated_cache_policy(
          new CachePolicyConfig(phal_config->cache_policy()));
    }
    RETURN_IF_ERROR(
        AddOpticalCard(card.slot(), mutable_card_group.get(), card));
  }

  return ::util::OkStatus();
}

::util::Status TaiSwitchConfigurator::AddOpticalCard(
    int slot, MutableAttributeGroup* mutable_card,
    const PhalOpticalCardConfig& config) {
  ASSIGN_OR_RETURN(auto datasource,
                   TaiOpticsDataSource::Make(slot, config));

  RETURN_IF_ERROR(
      mutable_card->AddAttribute("id", datasource->GetModuleSlot()));
  RETURN_IF_ERROR(
      mutable_card->AddAttribute("vendor_name", datasource->GetModuleVendor()));
  RETURN_IF_ERROR(mutable_card->AddAttribute(
      "hardware_state", datasource->GetModuleHardwareState()));

  RETURN_IF_ERROR(mutable_card->AddAttribute(
      "frequency", datasource->GetTxLaserFrequency()));
  // for now, operational mode directly assign to modulation format
  RETURN_IF_ERROR(mutable_card->AddAttribute(
      "operational_mode", datasource->GetOperationalMode()));
  RETURN_IF_ERROR(mutable_card->AddAttribute(
      "target_output_power", datasource->GetOutputPower()));
  RETURN_IF_ERROR(mutable_card->AddAttribute(
      "output_power", datasource->GetCurrentOutputPower()));
  RETURN_IF_ERROR(mutable_card->AddAttribute(
      "input_power", datasource->GetInputPower()));

  return ::util::OkStatus();
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
