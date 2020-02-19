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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_WRAPPER_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_WRAPPER_H_

#include <atomic>
#include <memory>
#include <string>
#include <thread>  // NOLINT(build/c++11)
#include <utility>
#include <vector>

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_object.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_wrapper_interface.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

/*!
 * \brief The TAIWrapper class wrap c TAI lib with c++ layer and give access
 * for TAI attributes through TAI interface objects (like Module, HostInterface
 * or NetworkInterface)
 */
class TAIWrapper : public TAIWrapperInterface {
 public:
  TAIWrapper();
  ~TAIWrapper() override;

  std::weak_ptr<Module> GetModule(std::size_t index) const override;

  std::weak_ptr<TAIObject> GetObject(const TAIPath& objectPath) const override;
  std::weak_ptr<TAIObject> GetObject(
      const TAIPathItem& pathItem) const override;
  std::weak_ptr<Module> GetModuleByLocation(const std::string& location) const;

  bool IsObjectValid(const TAIPath& path) const override {
    return !GetObject(path).expired();
  }
  bool IsModuleIdValid(std::size_t id) const override {
    return modules_.size() > id;
  }

  void ModulePresenceHandler();

 private:
  tai_status_t CreateModule(const std::string& location);

 private:
  std::vector<std::shared_ptr<Module>> modules_;
  tai_api_method_table_t api_;
  TAIPathValidator path_rule_;

  // thread stops if this value will be set to false.
  std::atomic<bool> thread_running_{true};
  // identify is TAI API initilized.
  std::atomic<bool> api_initialized_{false};
  // TAI module presence monitoring thread for plug/unplug processing.
  std::thread presence_monitoring_thread_;
  mutable std::mutex data_mux_;
}; /* class TAIWrapper */

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_WRAPPER_H_
