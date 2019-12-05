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

#ifndef STRATUM_HAL_LIB_PHAL_SFP_CONFIGURATOR_H_
#define STRATUM_HAL_LIB_PHAL_SFP_CONFIGURATOR_H_

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/attribute_group.h"

namespace stratum {
namespace hal {
namespace phal {

class SfpConfigurator : public AttributeGroup::RuntimeConfiguratorInterface {
 public:
  virtual ~SfpConfigurator() {}
  virtual ::util::Status HandleEvent(HwState state) = 0;

 protected:
  SfpConfigurator() {}
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_SFP_CONFIGURATOR_H_
