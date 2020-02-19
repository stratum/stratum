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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_MODULE_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_MODULE_H_

#include <memory>
#include <string>
#include <vector>

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_object.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

class HostInterface;
class NetworkInterface;

/*!
 * \brief The Module class represents TAI module object (an optical module
 * itself) that contains TAI host and network interfaces
 * \note the Module object should be created only in TAIWrapper class
 */
class Module final : public TAIObject {
 public:
  Module(const tai_api_method_table_t& api, const std::string& location);
  ~Module() override;

  bool IsHostInterfaceValid(std::size_t index) const;
  bool IsNetworkInterfaceValid(std::size_t index) const;

  std::weak_ptr<HostInterface> GetHostInterface(std::size_t index) const;
  std::weak_ptr<NetworkInterface> GetNetworkInterface(std::size_t index) const;

  std::string GetLocation() const;

 private:
  tai_status_t CreateNetif(uint32_t index);
  tai_status_t CreateHostif(uint32_t index);

  tai_status_t GetAttributeInterface(tai_attribute_t* attr) const override;
  tai_status_t SetAttributeInterface(
      const tai_attribute_t* attr) const override;

  tai_status_t DeserializeAttribute(
      const std::string& attr, int32_t* attr_id,
      const tai_serialize_option_t* option) const override;

 private:
  std::string location_;
  std::vector<std::shared_ptr<HostInterface>> host_ifs_;
  std::vector<std::shared_ptr<NetworkInterface>> net_ifs_;
}; /* class Module */

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_MODULE_H_
