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

#include "stratum/hal/lib/phal/tai/tai_wrapper/module.h"

#include <memory>
#include <string>
#include <vector>

#include "stratum/hal/lib/phal/tai/tai_wrapper/host_interface.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/network_interface.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

Module::Module(const tai_api_method_table_t& api, const std::string& location)
    : TAIObject(api), location_(location) {
  LOG(INFO) << "Create Module with location: " << location;

  std::vector<tai_attribute_t> list;
  tai_attribute_t attr;
  tai_status_t status;

  attr.id = TAI_MODULE_ATTR_LOCATION;
  attr.value.charlist.count = static_cast<uint>(location.size());
  attr.value.charlist.list = const_cast<char*>(location_.c_str());
  list.push_back(attr);

  status = api_.module_api->create_module(
      &id_, static_cast<uint32_t>(list.size()), list.data());
  if (TAI_STATUS_SUCCESS != status) {
    LOG(WARNING) << "Can't create Module. Error status: " << status;
    return;
  }

  list.clear();
  attr.id = TAI_MODULE_ATTR_NUM_HOST_INTERFACES;
  list.push_back(attr);
  attr.id = TAI_MODULE_ATTR_NUM_NETWORK_INTERFACES;
  list.push_back(attr);
  status = api_.module_api->get_module_attributes(
      id_, static_cast<uint32_t>(list.size()), list.data());

  if (TAI_STATUS_SUCCESS != status) {
    LOG(WARNING) << "Can't get host/network interfaces count: " << status;
    return;
  }

  for (uint32_t i = 0; i < list[0].value.u32; ++i) CreateHostif(i);

  for (uint32_t i = 0; i < list[1].value.u32; ++i) CreateNetif(i);
}

Module::~Module() {
  LOG(INFO) << "Remove Module with id: " << id_;

  for (auto netif : net_ifs_) netif.reset();

  for (auto hostif : host_ifs_) hostif.reset();

  api_.module_api->remove_module(id_);
}

tai_status_t Module::CreateHostif(uint32_t index) {
  auto hostif = std::make_shared<HostInterface>(api_, id_, index);
  if (!hostif->GetId()) {
    return TAI_STATUS_FAILURE;
  }

  host_ifs_.push_back(hostif);
  return TAI_STATUS_SUCCESS;
}

bool Module::IsHostInterfaceValid(std::size_t index) const {
  return host_ifs_.size() > index;
}

bool Module::IsNetworkInterfaceValid(std::size_t index) const {
  return net_ifs_.size() > index;
}

/*!
 * \brief Module::GetHostInterface method \return HostInterface object by
 * \param index
 */
std::weak_ptr<HostInterface> Module::GetHostInterface(std::size_t index) const {
  if (!IsHostInterfaceValid(index)) {
    return {};
  }

  return host_ifs_[index];
}

/*!
 * \brief Module::GetNetworkInterface method \return NetworkInterface object by
 * \param index
 * \note The Module takes ownership of the returned object.
 */
std::weak_ptr<NetworkInterface> Module::GetNetworkInterface(
    std::size_t index) const {
  if (!IsNetworkInterfaceValid(index)) {
    return {};
  }

  return net_ifs_[index];
}

tai_status_t Module::CreateNetif(uint32_t index) {
  auto netif = std::make_shared<NetworkInterface>(api_, id_, index);
  if (!netif->GetId()) {
    return TAI_STATUS_FAILURE;
  }

  net_ifs_.push_back(netif);
  return TAI_STATUS_SUCCESS;
}

tai_status_t Module::GetAttributeInterface(tai_attribute_t* attr) const {
  return api_.module_api->get_module_attribute(id_, attr);
}

tai_status_t Module::SetAttributeInterface(const tai_attribute_t* attr) const {
  return api_.module_api->set_module_attribute(id_, attr);
}

tai_status_t Module::DeserializeAttribute(
    const std::string& attr, int32_t* attr_id,
    const tai_serialize_option_t* option) const {
  return tai_deserialize_module_attr(attr.c_str(), attr_id, option);
}

std::string Module::GetLocation() const { return location_; }

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
