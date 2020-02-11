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

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_wrapper_interface.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

TAIPathItem TAIPathValidator::ModulePath(uint64 moduleId) {
  return {TAI_OBJECT_TYPE_MODULE, moduleId};
}

TAIPath TAIPathValidator::NetworkPath(
    const std::pair<uint64, uint32>& module_netif_pair) {
  return {ModulePath(module_netif_pair.first),
          {TAI_OBJECT_TYPE_NETWORKIF, module_netif_pair.second}};
}

TAIPath TAIPathValidator::HostPath(
    const std::pair<uint64, uint32>& module_hostif_pair) {
  return {ModulePath(module_hostif_pair.first),
          {TAI_OBJECT_TYPE_HOSTIF, module_hostif_pair.second}};
}

bool TAIPathValidator::IsModule(const TAIPath& path) {
  if (path.empty()) return false;

  if (path.size() == 1) return path.at(0).object_type == TAI_OBJECT_TYPE_MODULE;

  return false;
}

bool TAIPathValidator::IsNetwork(const TAIPath& path) {
  if (path.empty()) return false;

  if (path.size() == 2) {
    return path.at(0).object_type == TAI_OBJECT_TYPE_MODULE &&
           path.at(1).object_type == TAI_OBJECT_TYPE_NETWORKIF;
  }

  return false;
}

bool TAIPathValidator::IsHost(const TAIPath& path) {
  if (path.empty()) return false;

  if (path.size() == 2) {
    return path.at(0).object_type == TAI_OBJECT_TYPE_MODULE &&
           path.at(1).object_type == TAI_OBJECT_TYPE_HOSTIF;
  }

  return false;
}

/*!
 * \brief TAIPathValidator::CheckPath method checks \param path is valid by
 * comparing \param path with validPaths given in constructor \return true
 * if \param path is valid
 */
bool TAIPathValidator::CheckPath(const TAIPath& path) const {
  if (path.empty()) return false;

  if (std::any_of(path.cbegin(), path.cend(), [](const TAIPathItem& tiObject) {
        return !tiObject.isValid();
      })) {
    return false;
  }

  decltype(valid_paths_)::value_type objectTypes;
  std::transform(
      path.cbegin(), path.cend(), std::back_inserter(objectTypes),
      [](const TAIPathItem& tiObject) { return tiObject.object_type; });

  return std::any_of(
      valid_paths_.cbegin(), valid_paths_.cend(),
      [objectTypes](const decltype(valid_paths_)::value_type& validPath) {
        return validPath == objectTypes;
      });
}

bool TAIPathItem::isValid() const {
  return !((object_type == tai_object_type_t::TAI_OBJECT_TYPE_NULL) ||
           (object_type == tai_object_type_t::TAI_OBJECT_TYPE_MAX));
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
