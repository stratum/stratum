// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

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

// TaiSwitchConfigurator configures the PhalDb for use with the Tai Datasouce.
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
