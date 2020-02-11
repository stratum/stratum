/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_HOST_INTERFACE_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_HOST_INTERFACE_H_

#include <string>

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_object.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

/*!
 * \brief The HostInterface class represent TAI host interface that connects to
 * ASIC chip. A host interface object represents an interface between an optical
 * module(the hardware) and the host system, sometimes called client interfaces
 * (in our case this is TAIWrapper).
 * \note the HostInterface object should be created only in Module class
 */
class HostInterface final : public TAIObject {
 public:
  HostInterface(const tai_api_method_table_t& api,
                const tai_object_id_t module_id, const uint32_t index);
  ~HostInterface() override;

 private:
  tai_status_t GetAttributeInterface(tai_attribute_t* attr) const override;
  tai_status_t SetAttributeInterface(
      const tai_attribute_t* attr) const override;

  tai_status_t DeserializeAttribute(
      const std::string& attr, int32_t* attr_id,
      const tai_serialize_option_t* option) const override;
}; /* class HostInterface */

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_HOST_INTERFACE_H_
