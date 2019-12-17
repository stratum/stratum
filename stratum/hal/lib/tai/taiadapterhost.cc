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


#include "stratum/hal/lib/tai/taiadapterhost.h"

#include <string>
#include <vector>
#include <utility>

#include "stratum/glue/logging.h"
#include "stratum/lib/utils.h"

#include "absl/memory/memory.h"

#include "stratum/hal/lib/tai/hostinterface.h"
#include "stratum/hal/lib/tai/module.h"
#include "stratum/hal/lib/tai/networkinterface.h"

namespace stratum {
namespace hal {
namespace tai {

/*!
 * \brief modules_location variable used to store returned from TAI module
 * location
 */
static std::vector<std::string> modules_location;

/*!
 * \brief module_presence function will be called once for each module present
 * toward the end of the tai_api_initialze function, and then whenever there is
 * a change
 */
static void module_presence(bool /*present*/, char* location) {
  // \param present should be used to delete the module that was unplugged (I
  // think)
  modules_location.emplace_back(location);
}

TAIAdapterHost::TAIAdapterHost()
    : path_rule_({{tai_object_type_t::TAI_OBJECT_TYPE_MODULE},
                  {tai_object_type_t::TAI_OBJECT_TYPE_MODULE,
                   tai_object_type_t::TAI_OBJECT_TYPE_NETWORKIF},
                  {tai_object_type_t::TAI_OBJECT_TYPE_MODULE,
                   tai_object_type_t::TAI_OBJECT_TYPE_HOSTIF}}) {
  LOG(INFO) << "Initialize TAIAdapterHost";
  tai_service_method_table_t services;
  services.module_presence = module_presence;

  auto status = tai_api_initialize(0, &services);
  if (TAI_STATUS_SUCCESS != status) {
    LOG(ERROR) << "Failed to initialize TAIAdapterHost. Error status: "
               << status;
    return;
  }

  status =
      tai_api_query(TAI_API_MODULE, reinterpret_cast<void**>(&api_.module_api));
  if (TAI_STATUS_SUCCESS != status) {
    LOG(ERROR) << "Failed to query MODULE API" << std::endl;
    return;
  }

  status = tai_api_query(TAI_API_NETWORKIF,
                         reinterpret_cast<void**>(&api_.netif_api));
  if (TAI_STATUS_SUCCESS != status) {
    LOG(ERROR) << "Failed to query NETWORKIF API" << std::endl;
    return;
  }

  status =
      tai_api_query(TAI_API_HOSTIF, reinterpret_cast<void**>(&api_.hostif_api));
  if (TAI_STATUS_SUCCESS != status) {
    LOG(ERROR) << "Failed to query HOSTIF API" << std::endl;
    return;
  }

  for (const auto& location : modules_location) {
    CreateModule(location);
  }
}

TAIAdapterHost::~TAIAdapterHost() {
  LOG(INFO) << "Uninitialize TAIAdapterHost";
  LOG(INFO) << "TAI API uninitialize status: " << tai_api_uninitialize();
  // need to clear cause of static variable
  modules_location.clear();
}

tai_status_t TAIAdapterHost::CreateModule(const std::string& location) {
  auto module = std::make_shared<Module>(api_, location);
  if (!module->GetId()) {
    LOG(WARNING) << "Can't create module: " << location;
    return TAI_STATUS_FAILURE;
  }

  modules_.push_back(module);
  LOG(INFO) << __FUNCTION__ << " end";
  return TAI_STATUS_SUCCESS;
}

/*!
 * \brief TAIAdapterHost::GetModule method \return valid module with index
 * \param index otherwise expired std::weak_ptr
 */
std::weak_ptr<Module> TAIAdapterHost::GetModule(std::size_t index) const {
  LOG(INFO) << __FUNCTION__;
  if (index >= modules_.size()) {
    LOG(WARNING) << "Invalid input parameter";
    return {};
  }

  LOG(INFO) << __FUNCTION__ << " end";
  return modules_[index];
}

std::weak_ptr<TAIObject> TAIAdapterHost::GetObject(
    const TAIPath& objectPath) const {
  LOG(INFO) << __FUNCTION__;
  if (!path_rule_.CheckPath(objectPath)) {
    LOG(WARNING)
        << "Can't find required module! Please check is object path is valid";
    return {};
  }

  std::weak_ptr<Module> weak_module_ptr =
      GetModule(objectPath.front().object_index);
  if (weak_module_ptr.expired()) {
    LOG(WARNING) << "Invalid object, std::weak_ptr expired";
    return {};
  }

  std::shared_ptr<Module> module = weak_module_ptr.lock();
  if (objectPath.size() > 1) {
    std::weak_ptr<TAIObject> tai_object;
    TAIPathItem indexObj = objectPath.at(1);
    if (indexObj.object_type == tai_object_type_t::TAI_OBJECT_TYPE_HOSTIF) {
      return module->GetHostInterface(indexObj.object_index);
    }

    return module->GetNetworkInterface(indexObj.object_index);
  }

  LOG(INFO) << __FUNCTION__ << " end";
  return module;
}

std::weak_ptr<TAIObject> TAIAdapterHost::GetObject(
    const TAIPathItem& pathItem) const {
  return GetObject(TAIPath{pathItem});
}

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
  LOG(INFO) << __FUNCTION__;
  return {ModulePath(module_hostif_pair.first),
          {TAI_OBJECT_TYPE_HOSTIF, module_hostif_pair.second}};
}

bool TAIPathValidator::IsModule(const TAIPath& path) {
  LOG(INFO) << __FUNCTION__;
  if (path.empty()) return false;

  if (path.size() == 1) return path.at(0).object_type == TAI_OBJECT_TYPE_MODULE;

  return false;
}

bool TAIPathValidator::IsNetwork(const TAIPath& path) {
  LOG(INFO) << __FUNCTION__;
  if (path.empty()) return false;

  if (path.size() == 2) {
    return path.at(0).object_type == TAI_OBJECT_TYPE_MODULE &&
           path.at(1).object_type == TAI_OBJECT_TYPE_NETWORKIF;
  }

  return false;
}

bool TAIPathValidator::IsHost(const TAIPath& path) {
  LOG(INFO) << __FUNCTION__;
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
  LOG(INFO) << __FUNCTION__;
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
}  // namespace hal
}  // namespace stratum
