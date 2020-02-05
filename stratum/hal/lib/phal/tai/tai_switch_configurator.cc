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

// Make an instance of TaiSwitchConfigurator
::util::StatusOr<std::unique_ptr<TaiSwitchConfigurator>>
TaiSwitchConfigurator::Make(TaiInterface* tai_interface) {
  // Make sure we've got a valid Tai Interface
  CHECK_RETURN_IF_FALSE(tai_interface != nullptr);

  return absl::WrapUnique(new TaiSwitchConfigurator(tai_interface));
}

// Generate a default config using the TAI API.
::util::Status TaiSwitchConfigurator::CreateDefaultConfig(
    PhalInitConfig* phal_config) const {
  // TODO(plvision)
  return ::util::OkStatus();
}

// Configure the switch's attribute database with the given
// PhalInitConfig config.
::util::Status TaiSwitchConfigurator::ConfigurePhalDB(
    PhalInitConfig* phal_config, AttributeGroup* root) {
  auto mutable_root = root->AcquireMutable();

  // Add cards
  for (const auto& card : phal_config->optical_cards()) {
    // FIXME
    ASSIGN_OR_RETURN(auto card_group,
                     mutable_root->AddRepeatedChildGroup("cards"));
    auto mutable_card_group = card_group->AcquireMutable();
    RETURN_IF_ERROR(AddOpticalCard(0, mutable_card_group.get(), card));
  }

  return ::util::OkStatus();
}

::util::Status TaiSwitchConfigurator::AddOpticalCard(
    int slot, MutableAttributeGroup* mutable_card,
    const PhalOpticalCardConfig& config) {
  // Add port to attribute DB
  ASSIGN_OR_RETURN(auto port_group,
                   mutable_card->AddRepeatedChildGroup("ports"));
  auto mutable_port = port_group->AcquireMutable();

  return ::util::OkStatus();
}

}  // namespace tai
}  // namespace phal
}  // namespace hal

}  // namespace stratum
