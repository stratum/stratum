// Copyright 2019 Dell EMC
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_PHAL_SWITCH_CONFIGURATOR_INTERFACE_H_
#define STRATUM_HAL_LIB_PHAL_SWITCH_CONFIGURATOR_INTERFACE_H_

#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/attribute_group.h"
#include "stratum/glue/status/status.h"

namespace stratum {
namespace hal {
namespace phal {

class SwitchConfiguratorInterface {
 public:
  virtual ~SwitchConfiguratorInterface() = default;

  // Virtual function to create a default config should the
  // phal_config_path flag not be set.
  virtual ::util::Status CreateDefaultConfig(PhalInitConfig* config) const = 0;

  // Virtual function implemented by each derived class to
  // read the phal_init_config file and configure the Phal DB
  virtual ::util::Status ConfigurePhalDB(PhalInitConfig* config,
                                         AttributeGroup* root) = 0;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_SWITCH_CONFIGURATOR_INTERFACE_H_
