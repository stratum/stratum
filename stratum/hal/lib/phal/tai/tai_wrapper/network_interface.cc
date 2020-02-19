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

#include <string>
#include <vector>

#include "stratum/hal/lib/phal/tai/tai_wrapper/network_interface.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

NetworkInterface::NetworkInterface(const tai_api_method_table_t& api,
                                   const tai_object_id_t module_id,
                                   const uint32_t index)
    : TAIObject(api) {
  LOG(INFO) << "Create NetworkInterface with index: " << index;
  std::vector<tai_attribute_t> list;
  tai_attribute_t attr;

  attr.id = TAI_NETWORK_INTERFACE_ATTR_INDEX;
  attr.value.u32 = index;

  list.push_back(attr);
  auto status = api_.netif_api->create_network_interface(
      &id_, module_id, static_cast<uint32_t>(list.size()), list.data());
  if (TAI_STATUS_SUCCESS != status) {
    LOG(WARNING) << "Can't create NetworkInterface. Error status: " << status;
  }
}

NetworkInterface::~NetworkInterface() {
  LOG(INFO) << "Remove NetworkInterface with id: " << id_;
  api_.netif_api->remove_network_interface(id_);
}

tai_status_t NetworkInterface::GetAttributeInterface(
    tai_attribute_t* attr) const {
  return api_.netif_api->get_network_interface_attribute(id_, attr);
}

tai_status_t NetworkInterface::SetAttributeInterface(
    const tai_attribute_t* attr) const {
  return api_.netif_api->set_network_interface_attribute(id_, attr);
}

tai_status_t NetworkInterface::DeserializeAttribute(
    const std::string& attr, int32_t* attr_id,
    const tai_serialize_option_t* option) const {
  return tai_deserialize_network_interface_attr(attr.c_str(), attr_id, option);
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
