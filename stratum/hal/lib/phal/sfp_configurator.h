// Copyright 2019 Dell EMC
// SPDX-License-Identifier: Apache-2.0


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
