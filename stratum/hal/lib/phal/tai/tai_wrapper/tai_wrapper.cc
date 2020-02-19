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

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_wrapper.h"

#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "stratum/glue/logging.h"
#include "stratum/lib/utils.h"

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

#include "stratum/hal/lib/phal/tai/tai_wrapper/host_interface.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/module.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/network_interface.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

/*!
 * \brief modules_location queue used for exchange data(module's location)
 * between two separate threads
 */
static std::queue<std::pair<bool, std::string>> modules_location;
/*!
 * \brief modules_location_mux guard modules_location variable
 */
static std::mutex modules_location_mux;
/*!
 * \brief data_cv used for separate thread notification about module changes
 */
static std::condition_variable data_cv;

/*!
 * \brief module_presence function will be called once for each module present
 * toward the end of the tai_api_initialze function, and then whenever there is
 * a change
 *
 * \param present if equal to true than the module was inserted.
 *                otherwise the module was removed
 *
 * \note this function may be called in different contexts (such as interrupt
 * context or from a different thread/process)
 */
static void module_presence(bool present, char* location) {
  LOG(INFO) << "Module with location: " << location << " is "
            << (present ? "inserted" : "removed");
  // \param present should be used to create/delete the module that was
  // plugged/unplugged
  std::lock_guard<std::mutex> guard(modules_location_mux);
  modules_location.push({present, location});
  // data_cv notify thread created from TAIAdapterHost that update their state
  data_cv.notify_one();
}

TAIWrapper::TAIWrapper()
    : path_rule_({{tai_object_type_t::TAI_OBJECT_TYPE_MODULE},
                  {tai_object_type_t::TAI_OBJECT_TYPE_MODULE,
                   tai_object_type_t::TAI_OBJECT_TYPE_NETWORKIF},
                  {tai_object_type_t::TAI_OBJECT_TYPE_MODULE,
                   tai_object_type_t::TAI_OBJECT_TYPE_HOSTIF}}),
      thread_running_{true},
      api_initialized_{false},
       // this values should be set before thread starts
      presence_monitoring_thread_(&TAIWrapper::ModulePresenceHandler, this) {
  LOG(INFO) << "Initialize TAIWrapper";
  tai_service_method_table_t services;
  services.module_presence = module_presence;

  auto status = tai_api_initialize(0, &services);
  if (TAI_STATUS_SUCCESS != status) {
    LOG(ERROR) << "Failed to initialize TAIWrapper. Error status: " << status;
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

  api_initialized_ = true;
}

TAIWrapper::~TAIWrapper() {
  thread_running_ = false;

  data_cv.notify_one();
  LOG(INFO) << "Uninitialize TAIWrapper "
            << "TAI API uninitialize status: " << tai_api_uninitialize();

  std::queue<std::pair<bool, std::string>> empty;
  std::swap(modules_location, empty);

  if (presence_monitoring_thread_.joinable()) {
    presence_monitoring_thread_.join();
  }
}

tai_status_t TAIWrapper::CreateModule(const std::string& location) {
  std::lock_guard<std::mutex> lg(data_mux_);
  auto module = std::make_shared<Module>(api_, location);
  if (!module->GetId()) {
    LOG(WARNING) << "Can't create module: " << location;
    return TAI_STATUS_FAILURE;
  }

  modules_.push_back(module);
  return TAI_STATUS_SUCCESS;
}

/*!
 * \brief TAIWrapper::GetModule method \return valid module with index
 * \param index otherwise expired std::weak_ptr
 */
std::weak_ptr<Module> TAIWrapper::GetModule(std::size_t index) const {
  std::lock_guard<std::mutex> lg(data_mux_);
  if (index >= modules_.size()) {
    LOG(WARNING) << "Invalid input parameter";
    return {};
  }

  return modules_[index];
}

/*!
 * \brief TAIWrapper::GetObject method \return std::weak_ptr to object with
 * \param objectPath otherwise \return uninitialized std::weak_ptr.
 */
std::weak_ptr<TAIObject> TAIWrapper::GetObject(
    const TAIPath& objectPath) const {
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
    TAIPathItem indexObj = objectPath.at(1);
    if (indexObj.object_type == tai_object_type_t::TAI_OBJECT_TYPE_HOSTIF) {
      std::lock_guard<std::mutex> lg(data_mux_);
      return module->GetHostInterface(indexObj.object_index);
    }

    std::lock_guard<std::mutex> lg(data_mux_);
    return module->GetNetworkInterface(indexObj.object_index);
  }

  return module;
}

/*!
 * \brief TAIWrapper::GetObject overload for TAIWrapper::GetObject(
 *  const TAIPath& objectPath) with single \param pathItem TAIPathItem
 */
std::weak_ptr<TAIObject> TAIWrapper::GetObject(
    const TAIPathItem& pathItem) const {
  return GetObject(TAIPath{pathItem});
}

/*!
 * \brief TAIWrapper::GetModuleByLocation method search module's location
 * that match with \param location
 *
 * \return module with given location otherwise invalid weak_ptr
 */
std::weak_ptr<Module> TAIWrapper::GetModuleByLocation(
    const std::string& location) const {
  std::lock_guard<std::mutex> lg(data_mux_);
  for (const auto& module : modules_) {
    if (module && module->GetLocation() == location) {
      return module;
    }
  }

  return {};
}

/*!
 * \brief TAIWrapper::ModulePresenceHandler() the method is invoked in a
 * separate thread. Method is needed to update TAIWrapper state whenever there
 * is a change in an optical module's presence.
 *
 * \note This thread controls invariant state with 'data_mux_' member.
 * This method should be used only for thread execution!
 */
void TAIWrapper::ModulePresenceHandler() {
  LOG(INFO) << "Started modules location handler thread";
  while (thread_running_) {
    std::unique_lock<std::mutex> lk(modules_location_mux);

    // this condition will skip 'module_presence' handling untill TAI API isn't
    // initialized
    if (!api_initialized_) continue;

    // thread sleep and wait while new module location will be added(while
    // modules_location.empty() will return false) from TAI by calling
    // 'module_presence' callback
    data_cv.wait(
        lk, [this]() { return !modules_location.empty() || !thread_running_; });

    // stop thread
    if (!thread_running_) {
      LOG(ERROR) << "Modules location handler thread stopped";
      break;
    }

    std::pair<bool, std::string> module_location = modules_location.front();
    const bool present = module_location.first;
    const std::string location = module_location.second;
    modules_location.pop();
    lk.unlock();

    if (present) {  // create module
      CreateModule(location);
    } else {  // delete module
      std::shared_ptr<Module> module = GetModuleByLocation(location).lock();
      std::lock_guard<std::mutex> lg(data_mux_);
      modules_.erase(std::remove(modules_.begin(), modules_.end(), module),
                     modules_.end());
    }
    // TODO(unknown): There should be a code that updates ChassisConfig and
    // configuration tree according to the optical module that was inserted or
    // removed.
    // Example: Updated ChassisConfig according to inserted module.
    // optical_ports {
    //   id: {new id related to inserted module}
    //   name: {"new_name"}
    //   module_location: {new module location related to inserted module}
    //   netif_location: {new netif location related to inserted module}
    //   ...
    // }
  }
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
