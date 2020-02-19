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

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_attribute.h"

#include "stratum/glue/logging.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

TAIAttribute::TAIAttribute(tai_attr_id_t attr_id,
                           const tai_attr_metadata_t* metadata)
    : kMeta(metadata) {
  attr.id = attr_id;

  if (tai_metadata_alloc_attr_value(kMeta, &attr, nullptr) !=
      TAI_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to allocate memory for attr value";
  }
}

TAIAttribute::~TAIAttribute() {
  if (tai_metadata_free_attr_value(kMeta, &attr, nullptr) !=
      TAI_STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to free attr memory";
  }
}

bool TAIAttribute::IsValid() const {
  return kMeta && attr.id != TAI_INVALID_ATTRIBUTE_ID;
}

/*!
 * \brief TAIAttribute::DeserializeValue method deserialize \param buff to TAI
 * attribute value based on \param option (one of option value human, valueonly
 * and json)
 * \return true if deserializing success
 * \note that by default \param option tai_serialize_option_t::human is set to
 * true, it means that method will deserialize for example enum values:
 *    if buff = "shallow"; and option.human = true; in this case if attr.id =
 *    TAI_NETWORK_INTERFACE_ATTR_LOOPBACK_TYPE then value will be represented as
 *    attr.value.s32 = TAI_NETWORK_INTERFACE_LOOPBACK_TYPE_SHALLOW
 */
bool TAIAttribute::DeserializeAttribute(const std::string& buff,
                                        const tai_serialize_option_t& option) {
  int ret = tai_deserialize_attribute_value(buff.c_str(), kMeta, &attr.value,
                                            &option);

  if (ret < 0) LOG(ERROR) << "Can't deserialize attribute value";

  return ret == TAI_STATUS_SUCCESS;
}

tai_serialize_option_t TAIAttribute::DefaultDeserializeOption() {
  tai_serialize_option_t option;
  option.human = true;
  option.valueonly = false;
  option.json = false;

  return option;
}

TAIAttribute TAIAttribute::InvalidAttributeObject() {
  return {TAI_INVALID_ATTRIBUTE_ID, nullptr};
}

TAIAttribute::TAIAttribute(const TAIAttribute& src) {
  if (!src.kMeta) return;

  tai_metadata_deepcopy_attr_value(src.kMeta, &src.attr, &attr);
  kMeta = src.kMeta;
}

TAIAttribute& TAIAttribute::operator=(const TAIAttribute& src) {
  if (!src.kMeta) return *this;

  tai_metadata_deepcopy_attr_value(src.kMeta, &src.attr, &attr);
  kMeta = src.kMeta;

  return *this;
}

/*!
 * \brief TAIAttribute::SerializeAttribute method serialize attr member with
 * kMeta metadata help to human readable string
 * \return serialized string if success otherwise return empty string
 * \note For example kMeta.attrvaluetype = TAI_ATTR_VALUE_TYPE_S32
 *        kMeta.isenum = true
 *        kMeta.objecttype = TAI_OBJECT_TYPE_NETWORKIF
 *        kMeta.attrid = TAI_NETWORK_INTERFACE_ATTR_LOOPBACK_TYPE
 *        attr.value.s32 = 1(enum = TAI_NETWORK_INTERFACE_LOOPBACK_TYPE_SHALLOW)
 *      in this case to out string will be returned: "shallow"
 */
std::string TAIAttribute::SerializeAttribute() const {
  auto to_return = std::string();

  tai_serialize_option_t option;
  option.human = true;
  option.json = true;
  option.valueonly = true;

  constexpr std::size_t kBufsize = 128;
  char bbuf[kBufsize] = {0};
  int count = tai_serialize_attribute(bbuf, kBufsize, kMeta, &attr, &option);
  if (count == TAI_SERIALIZE_ERROR) return {};

  return {bbuf};
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
