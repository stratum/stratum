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

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_object.h"

#include "stratum/glue/logging.h"
#include "stratum/lib/utils.h"

#include "absl/memory/memory.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

TAIObject::TAIObject(const tai_api_method_table_t& api) : api_(api) {}

const tai_attr_metadata_t* TAIObject::GetMetadata(tai_attr_id_t attr_id) const {
  const tai_object_type_t kObjectType = GetObjectType();
  if (kObjectType == TAI_OBJECT_TYPE_NULL) return nullptr;

  const tai_attr_metadata_t* meta =
      tai_metadata_get_attr_metadata(kObjectType, attr_id);
  if (!meta) LOG(ERROR) << "Metadata not found";

  return meta;
}

/*!
 * \brief TAIObject::GetObjectType method \return current object type
 */
tai_object_type_t TAIObject::GetObjectType() const {
  const tai_object_type_t kObjectType = tai_object_type_query(id_);
  if (kObjectType == TAI_OBJECT_TYPE_NULL) {
    LOG(ERROR) << "TAIObject type isn't valid.";
  }

  return kObjectType;
}

tai_object_id_t TAIObject::GetId() const { return id_; }

/*!
 * \brief TAIObject::GetAlocatedAttributeObject method create and \return valid
 * TAIAttribute object based on \param attr_id with correct tai_attr_metadata_t
 * and allocated tai_attribute_t
 */
TAIAttribute TAIObject::GetAlocatedAttributeObject(
    tai_attr_id_t attr_id) const {
  if (attr_id == TAI_INVALID_ATTRIBUTE_ID) {
    return TAIAttribute::InvalidAttributeObject();
  }

  const tai_attr_metadata_t* kMeta = GetMetadata(attr_id);
  if (!kMeta) {
    return TAIAttribute::InvalidAttributeObject();
  }

  return {attr_id, kMeta};
}

/*!
 * \brief TAIObject::GetAlocatedAttributeObject method is overloaded with string
 * \param attr_name
 */
TAIAttribute TAIObject::GetAlocatedAttributeObject(
    const std::string attr_name) const {
  if (attr_name.empty()) {
    LOG(WARNING) << "Parameter \"attr_name\" is empty";
    TAIAttribute::InvalidAttributeObject();
  }

  int64_t attr_id = DeserializeAttrName(attr_name);
  if (attr_id < 0) {
    LOG(WARNING) << "Deserialize attribute name returned invalid status";
    TAIAttribute::InvalidAttributeObject();
  }

  return GetAlocatedAttributeObject(static_cast<tai_attr_id_t>(attr_id));
}

/*!
 * \brief TAIObject::GetAttribute method get attribute \param attr_id from
 * specific TAIObject and sets to \param attr_value
 * \return TAI_STATUS_SUCCESS if success else return some of TAI_STATUS_CODE
 */
TAIAttribute TAIObject::GetAttribute(tai_attr_id_t attr_id,
                                     tai_status_t* return_status) const {
  TAIAttribute attr = GetAlocatedAttributeObject(attr_id);
  if (!attr.IsValid()) {
    LOG(ERROR) << "Failed to allocate attr value";
    if (return_status) *return_status = TAI_STATUS_NO_MEMORY;
    return attr;
  }

  tai_status_t ret = GetAttributeInterface(&attr.attr);

  if (ret == TAI_STATUS_BUFFER_OVERFLOW) {
    LOG(ERROR) << "Buffer overflow";
    if (return_status) {
      *return_status = TAI_STATUS_BUFFER_OVERFLOW;
    }
  }

  if (ret != TAI_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to get attribute";
    if (return_status) {
      *return_status = TAI_STATUS_FAILURE;
    }
  }

  if (return_status) *return_status = ret;

  return attr;
}

/*!
 * \brief TAIObject::SetAttribute method sets given \param attr_value to
 * \param attr_id attribute
 * \return TAI_STATUS_SUCCESS if success else return some of TAI_STATUS_CODE
 */
tai_status_t TAIObject::SetAttribute(const tai_attribute_t* attr) const {
  if (!attr) {
    LOG(ERROR) << "Failed to set attribute";
    return TAI_STATUS_FAILURE;
  }

  int ret = SetAttributeInterface(attr);

  if (ret < 0) LOG(ERROR) << "Failed to set attribute. error code: " << ret;

  return ret;
}

/*!
 * \brief TAIObject::DeserializeAttrName method converts \param attr_name from
 * string to concrete attribute id or \return -1
 */
int64_t TAIObject::DeserializeAttrName(const std::string& attr_name) const {
  if (attr_name.empty()) {
    LOG(ERROR) << "Invalid input parameter";
    return -1;
  }

  int32_t attr_id = 0;
  tai_serialize_option_t option;
  option.human = true;
  option.json = false;
  option.valueonly = false;

  int ret = DeserializeAttribute(attr_name.c_str(), &attr_id, &option);
  if (ret < 0) return -1;

  const tai_attr_metadata_t* kMeta =
      GetMetadata(static_cast<tai_attr_id_t>(attr_id));
  if (!kMeta) {
    return -1;
  }

  return kMeta->attrid;
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
