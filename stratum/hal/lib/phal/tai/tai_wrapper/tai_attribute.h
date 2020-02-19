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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_ATTRIBUTE_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_ATTRIBUTE_H_

#include <string>

#include "external/com_github_oopt_tai/taimetadata.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

/*!
 * \brief The TaiAttribute class takes care about correct TAI attribute creating
 * and deleting with this point this object can't be copied(for now)
 * \note struct tai_attribute_t is a container for data. This struct works in
 * pair with tai_attr_metadata_t struct that tells what data should be created
 * and how to create this data. So in the constructor, we make correct data
 * creation using tai_metadata_alloc_attr_value and in destructor, we make
 * correct data deleting with tai_metadata_free_attr_value.
 */
class TaiAttribute {
 public:
  TaiAttribute(tai_attr_id_t attr_id, const tai_attr_metadata_t* metadata);
  ~TaiAttribute();

  bool IsValid() const;

  std::string SerializeAttribute() const;
  bool DeserializeAttribute(
      const std::string& buff,
      const tai_serialize_option_t& option = DefaultDeserializeOption());

  static tai_serialize_option_t DefaultDeserializeOption();
  static TaiAttribute InvalidAttributeObject();

  TaiAttribute(const TaiAttribute& src);
  TaiAttribute& operator=(const TaiAttribute& src);

  TaiAttribute(TaiAttribute&&) = default;
  TaiAttribute& operator=(TaiAttribute&&) = default;

 public:
  tai_attribute_t attr;
  const tai_attr_metadata_t* kMeta{nullptr};
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_ATTRIBUTE_H_
