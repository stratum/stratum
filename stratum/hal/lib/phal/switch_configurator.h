/*
 * Copyright 2019 Dell EMC
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


#ifndef STRATUM_HAL_LIB_PHAL_SWITCH_CONFIGURATOR_H_
#define STRATUM_HAL_LIB_PHAL_SWITCH_CONFIGURATOR_H_

#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/attribute_group.h"
#include "stratum/glue/status/status.h"

namespace stratum {
namespace hal {
namespace phal {

class SwitchConfigurator {
 public:
  virtual ~SwitchConfigurator() = default;

  // Virtual function to create a default config should the
  // phal_config_path flag not be set.
  virtual ::util::Status CreateDefaultConfig(PhalInitConfig* config) const = 0;

  // Virtual function implemented by each derived class to
  // read the phal_init_config file and configure the Phal DB
  virtual ::util::Status ConfigurePhalDB(const PhalInitConfig& config,
                                         AttributeGroup* root) = 0;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_SWITCH_CONFIGURATOR_H_
