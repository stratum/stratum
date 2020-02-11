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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_WRAPPER_INTERFACE_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_WRAPPER_INTERFACE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_object.h"

namespace stratum {
namespace hal {
namespace phal {
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

  bool operator==(const TAIPathItem& path_item) const {
    return (object_type == path_item.object_type) &&
           (object_index == path_item.object_index);
  }
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
 * \brief The TAIWrapperInterface exists to provide an interface to TAI  layer
 * and all classes which rely on that layer.
 */
class TAIWrapperInterface {
 public:
  virtual ~TAIWrapperInterface() = default;

  virtual std::weak_ptr<Module> GetModule(std::size_t index) const = 0;

  virtual std::weak_ptr<TAIObject> GetObject(
      const TAIPath& objectPath) const = 0;
  virtual std::weak_ptr<TAIObject> GetObject(
      const TAIPathItem& pathItem) const = 0;

  virtual bool IsObjectValid(const TAIPath& path) const = 0;
  virtual bool IsModuleIdValid(std::size_t id) const = 0;
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_WRAPPER_INTERFACE_H_
