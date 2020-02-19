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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_NETWORK_INTERFACE_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_NETWORK_INTERFACE_H_

#include <string>

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_object.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

/*!
 * \brief The NetworkInterface class represents TAI network interface object
 * that handle optic connection. A network interface object represents hardware
 * components which transmit/receive one wavelength.
 * \note NetworkInterface object should be created only in Module class
 */
class NetworkInterface final : public TAIObject {
 public:
  NetworkInterface(const tai_api_method_table_t& api,
                   const tai_object_id_t module_id, const uint32_t index);
  ~NetworkInterface() override;

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

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_NETWORK_INTERFACE_H_
