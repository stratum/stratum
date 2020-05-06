/*
 * Copyright 2020-present Open Networking Foundation
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
#include "stratum/hal/lib/phal/tai/tai_interface.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

// TaiSwitchConfigurator configures the PhalDb for use with the Tai Datasouce.
class TaiSwitchConfigurator final : public SwitchConfiguratorInterface {
 public:
  static ::util::StatusOr<std::unique_ptr<TaiSwitchConfigurator>>
      Make(TaiInterface* tai_interface);

  ::util::Status CreateDefaultConfig(PhalInitConfig* config) const override;

  ::util::Status ConfigurePhalDB(PhalInitConfig* config,
                                 AttributeGroup* root) override;

 private:
  explicit TaiSwitchConfigurator(TaiInterface* tai_interface) :
      tai_interface_(tai_interface) { }

  ::util::Status AddOpticalModule(MutableAttributeGroup* mutable_card,
                                  PhalOpticalModuleConfig* config);
  ::util::Status AddOpticalPort(MutableAttributeGroup* mutable_port_group,
                                const PhalOpticalModuleConfig::Port& config);

  // Default cache policy config
  CachePolicyConfig cache_policy_config_;

  // The TAI interface which allows the configurator access the TAI functions.
  TaiInterface* tai_interface_;
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_SWITCH_CONFIGURATOR_H_
