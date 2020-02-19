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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TEST_TAI_WRAPPER_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TEST_TAI_WRAPPER_MOCK_H_

#include <memory>

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_wrapper_interface.h"

#include "gmock/gmock.h"


namespace stratum {
namespace hal {
namespace phal {
namespace tai {

class TaiWrapperMock : public TaiWrapperInterface {
 public:
  MOCK_CONST_METHOD1(GetModule, std::weak_ptr<Module>(std::size_t index));

  MOCK_CONST_METHOD1(GetObject,
                     std::weak_ptr<TaiObject>(const TaiPath& objectPath));
  MOCK_CONST_METHOD1(GetObject,
                     std::weak_ptr<TaiObject>(const TaiPathItem& pathItem));

  MOCK_CONST_METHOD1(IsObjectValid, bool(const TaiPath& path));
  MOCK_CONST_METHOD1(IsModuleIdValid, bool(std::size_t id));
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TEST_TAI_WRAPPER_MOCK_H_
