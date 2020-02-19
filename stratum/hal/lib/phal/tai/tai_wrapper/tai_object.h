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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_OBJECT_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_OBJECT_H_

#include <string>

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_attribute.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

/*!
 * \brief The tai_api_method_table_t struct contains method table retrieved
 * with tai_api_query()
 */
struct tai_api_method_table_t {
  tai_module_api_t* module_api{nullptr};
  tai_host_interface_api_t* hostif_api{nullptr};
  tai_network_interface_api_t* netif_api{nullptr};
};

/*!
 * \brief The TaiObject class is base class for each tai object (Module,
 * HostInterface and NetworkInterface) and contains all common methods.
 * Example: on picture bellow presents schematic TAI adapter that has one Module
 * with two HostInterfaces and one NetworkInterface all this objects called TAI
 * objects(count of TAI object can be different in real case). As you can see
 * HostInterface objects communicate with system hardware (like Ethernet ASIC),
 * NetworkInterface is responsible for optical connection and all settings
 * related to this and Module is optical module itself that contains
 * HostInterface and NetworkInterface.
 *                      _________________________________
 * _________           |                                 |
 *          |        ___________                         |
 *          |_______|   Host    |                        |
 *          |-------| Interface |                        |
 *          |        -----------                    ___________
 *          |          |                           |           | Optical fiber
 * Ethernet |          |             Module        |  Network  |===============
 *   ASIC   |          |                           | Interface |===============
 *          |        ___________                   |           |
 *          |_______|    Host   |                   -----------
 *          |-------| Interface |                        |
 *          |        -----------                         |
 *          |          |                                 |
 * ---------            ---------------------------------
 */
class TaiObject {
 public:
  explicit TaiObject(const tai_api_method_table_t& api);
  virtual ~TaiObject() = default;

  virtual TaiAttribute GetAttribute(tai_attr_id_t attr_id,
                                    tai_status_t* return_status) const;
  virtual tai_status_t SetAttribute(const tai_attribute_t* attr) const;

  tai_object_id_t GetId() const;

  virtual TaiAttribute GetAlocatedAttributeObject(tai_attr_id_t attr_id) const;
  virtual TaiAttribute GetAlocatedAttributeObject(
      const std::string attr_id) const;

  // deleted copy/move functionality
  TaiObject(const TaiObject&) = delete;
  TaiObject& operator=(const TaiObject&) = delete;
  TaiObject(TaiObject&&) = delete;
  TaiObject& operator=(TaiObject&&) = delete;

 protected:
  /*!
   * \brief GetAttributeInterface pure virtual method used for get correct TAI
   * interface
   * \param attr item where got attribute will be set
   * \return TAI_STATUS_SUCCESS if everything is fine else return error
   * status code
   */
  virtual tai_status_t GetAttributeInterface(tai_attribute_t* attr) const = 0;

  /*!
   * \brief SetAttributeInterface pure virtual method used to get correct TAI
   * interface
   * \param attr item from where new attribute will be set to TAI object
   * \return TAI_STATUS_SUCCESS if everything is fine else return error
   * status code
   */
  virtual tai_status_t SetAttributeInterface(
      const tai_attribute_t* attr) const = 0;

  /*!
   * \brief DeserializeAttribute pure virtual method used to get correct TAI
   * deserialize_attr_ function for correct object type
   * \note \param attr_id  will can be changed
   * \return -1 if something go wrong
   */
  virtual tai_status_t DeserializeAttribute(
      const std::string& attr, int32_t* attr_id,
      const tai_serialize_option_t* option) const = 0;

  tai_object_type_t GetObjectType() const;
  int64_t DeserializeAttrName(const std::string& attr_name) const;

  const tai_attr_metadata_t* GetMetadata(tai_attr_id_t attr_id) const;

 protected:
  const tai_api_method_table_t& api_;
  tai_object_id_t id_{TAI_NULL_OBJECT_ID};
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_OBJECT_H_
