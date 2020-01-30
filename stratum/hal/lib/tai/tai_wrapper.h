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


#ifndef STRATUM_HAL_LIB_TAI_TAI_WRAPPER_H_
#define STRATUM_HAL_LIB_TAI_TAI_WRAPPER_H_

#include <string>
#include <vector>
#include <utility>
#include <memory>

#include "stratum/hal/lib/tai/tai_object.h"
#include "stratum/hal/lib/tai/tai_wrapper_interface.h"

namespace stratum {
namespace hal {
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

  bool IsObjectValid(const TAIPath& path) const override {
    return !GetObject(path).expired();
  }
  bool IsModuleIdValid(std::size_t id) const override {
    return modules_.size() > id;
  }

 private:
  tai_status_t CreateModule(const std::string& location);

 private:
  std::vector<std::shared_ptr<Module>> modules_;
  tai_api_method_table_t api_;
  TAIPathValidator path_rule_;
}; /* class TAIWrapper */

}  // namespace tai
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TAI_TAI_WRAPPER_H_
