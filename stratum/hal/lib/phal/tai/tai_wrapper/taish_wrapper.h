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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAISH_WRAPPER_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAISH_WRAPPER_H_

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

class TaishWrapper : public TaiWrapperInterface {
 public:
  TaishWrapper();
  ~TaishWrapper() override;

  std::weak_ptr<Module> GetModule(std::size_t index) const override;
  std::weak_ptr<TaiObject> GetObject(const TaiPath& objectPath) const override;
  std::weak_ptr<TaiObject>
  GetObject(const TaiPathItem& pathItem) const override;

  bool IsObjectValid(const TaiPath& path) const override;
  bool IsModuleIdValid(std::size_t id) const override;
}; /* class TaishWrapper */

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAISH_WRAPPER_H_
