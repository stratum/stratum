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


#ifndef STRATUM_HAL_LIB_TAI_TAIADAPTERHOST_H_
#define STRATUM_HAL_LIB_TAI_TAIADAPTERHOST_H_

#include <string>
#include <vector>
#include <utility>

#include "stratum/hal/lib/tai/taiobject.h"

namespace stratum {
namespace hal {
namespace tai {

class Module;

/*!
 * \brief The TAIPathItem struct represents a single TAI object identifier with
 * TAI object type and unique index
 */
struct TAIPathItem {
  TAIPathItem(tai_object_type_t object_type_, std::size_t object_index_)
      : object_type(object_type_), object_index(object_index_) {}

  tai_object_type_t object_type{tai_object_type_t::TAI_OBJECT_TYPE_NULL};
  std::size_t object_index{0};

  bool isValid() const;
};

using TAIPath = std::vector<TAIPathItem>;
using TAIValidPaths = std::vector<std::vector<tai_object_type_t>>;

/*!
 * \brief The TAIPathValidator class represents validation layer for TAI object
 * path.
 * \note User should config object with valid paths. For example valid paths for
 * TAI lib is: {TAI_OBJECT_TYPE_MODULE},
 *             {TAI_OBJECT_TYPE_MODULE, TAI_OBJECT_TYPE_NETWORKIF} and
 *             {TAI_OBJECT_TYPE_MODULE, TAI_OBJECT_TYPE_HOSTIF}
 * so all other paths is invalid and will not be accepted
 */
class TAIPathValidator {
 public:
  explicit TAIPathValidator(const TAIValidPaths& validPaths)
      : valid_paths_(validPaths) {}

  static TAIPathItem ModulePath(uint64 moduleId);
  static TAIPath NetworkPath(
      const std::pair<uint64, uint32>& module_netif_pair);
  static TAIPath HostPath(const std::pair<uint64, uint32>& module_hostif_pair);

  static bool IsModule(const TAIPath& path);
  static bool IsNetwork(const TAIPath& path);
  static bool IsHost(const TAIPath& path);

  bool CheckPath(const TAIPath& path) const;

 private:
  TAIValidPaths valid_paths_;
};

/*!
 * \brief The TAIAdapterHost class wrap c TAI lib with c++ layer and give access
 * for TAI attributes through TAI interface objects (like Module, HostInterface
 * or NetworkInterface)
 */
class TAIAdapterHost {
 public:
  TAIAdapterHost();
  ~TAIAdapterHost();

  std::weak_ptr<Module> GetModule(std::size_t index) const;

  std::weak_ptr<TAIObject> GetObject(const TAIPath& objectPath) const;
  std::weak_ptr<TAIObject> GetObject(const TAIPathItem& pathItem) const;

  bool IsObjectValid(const TAIPath& path) const {
    return !GetObject(path).expired();
  }
  bool IsModuleIdValid(std::size_t id) const { return modules_.size() > id; }

 private:
  tai_status_t CreateModule(const std::string& location);

 private:
  std::vector<std::shared_ptr<Module>> modules_;
  tai_api_method_table_t api_;
  TAIPathValidator path_rule_;
}; /* class TAIAdapterHost */

}  // namespace tai
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TAI_TAIADAPTERHOST_H_
