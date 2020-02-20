/*
 * Copyright 2020-present Open Networking Foundation
 * Copyright 2020 PLVision
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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_SWITCH_CONFIGURATOR_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_SWITCH_CONFIGURATOR_H_

#include <map>
#include <memory>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/attribute_group.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/switch_configurator_interface.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

/*!
 * \brief TaiSwitchConfigurator class configure relationship between
 * TaiOpticsDataSource and Database.
 */
class TaiSwitchConfigurator final : public SwitchConfiguratorInterface {
 public:
  static ::util::StatusOr<std::unique_ptr<TaiSwitchConfigurator>> Make();

  ::util::Status CreateDefaultConfig(PhalInitConfig* config) const override;

  ::util::Status ConfigurePhalDB(PhalInitConfig* config,
                                 AttributeGroup* root) override;

 private:
  TaiSwitchConfigurator() = default;

  ::util::Status AddOpticalCard(int slot, MutableAttributeGroup* mutable_card,
                                const PhalOpticalCardConfig& config);

  // Default cache policy config
  CachePolicyConfig cache_policy_config_;
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_SWITCH_CONFIGURATOR_H_
