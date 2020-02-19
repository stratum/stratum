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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TEST_TAI_OBJECT_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TEST_TAI_OBJECT_MOCK_H_

#include <string>

#include "gmock/gmock.h"

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_object.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

class TAIObjectMock : public TAIObject {
 public:
  TAIObjectMock() : TAIObject(tai_api_method_table_t()) {}

  MOCK_CONST_METHOD2(GetAttribute, TAIAttribute(tai_attr_id_t attr_id,
                                                tai_status_t* return_status));
  MOCK_CONST_METHOD1(SetAttribute, tai_status_t(const tai_attribute_t* attr));

  MOCK_CONST_METHOD1(GetAlocatedAttributeObject,
                     TAIAttribute(tai_attr_id_t attr_id));
  MOCK_CONST_METHOD1(GetAlocatedAttributeObject,
                     TAIAttribute(const std::string attr_id));

  MOCK_CONST_METHOD1(GetAttributeInterface,
                     tai_status_t(tai_attribute_t* attr));
  MOCK_CONST_METHOD1(SetAttributeInterface,
                     tai_status_t(const tai_attribute_t* attr));

  MOCK_CONST_METHOD3(DeserializeAttribute,
                     tai_status_t(const std::string& attr, int32_t* attr_id,
                                  const tai_serialize_option_t* option));
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TEST_TAI_OBJECT_MOCK_H_
