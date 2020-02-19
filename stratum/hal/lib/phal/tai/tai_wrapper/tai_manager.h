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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_MANAGER_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_MANAGER_H_

#include <memory>
#include <utility>
#include <string>

#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_wrapper.h"

#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_attribute.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/types_converter.h"

#include "stratum/lib/macros.h"

#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

/*!
 * \brief The TaiManager class provide single access point for user<->TaiWrapper
 * host interaction.
 */
class TaiManager {
 public:
  // Creates a singleton instance.
  static TaiManager* CreateSingleton() LOCKS_EXCLUDED(init_lock_);
  // Return the singleton instance to be used in the TAI calls.
  static TaiManager* GetSingleton() LOCKS_EXCLUDED(init_lock_);

  template <typename T>
  ::util::StatusOr<T> GetValue(
      const tai_attr_id_t& attr_id,
      const std::pair<uint64, uint32>& module_netif_pair) const;

  template <typename T>
  ::util::Status SetValue(
      const T& value_to_set, tai_attr_id_t attr_id,
      const std::pair<uint64, uint32>& module_netif_pair) const;

  bool IsObjectValid(const TaiPath& path);

  // Not copyable or movable
  TaiManager(const TaiManager&) = delete;
  TaiManager& operator=(const TaiManager&) = delete;
  TaiManager(TaiManager&&) = delete;
  TaiManager& operator=(const TaiManager&&) = delete;

 private:
  template <typename T>
  static bool SetValueToTaiAttribute(TaiAttribute* tai_attribute,
                                     const T& value_to_set);
  template <typename T>
  static T TaiAttributeToResponse(const TaiAttribute& attribute);

 protected:
  explicit TaiManager(std::unique_ptr<TaiWrapperInterface> wrapper);

 private:
  // The "TaiWrapper" class manages TAI objects' lifetime, as well as the access
  // to them.
  // Now, to experience a thread-safe TAI manager interaction, this mutex should
  // be LOCKED BEFORE
  //
  //   * any TaiManager method call
  //
  // and UNLOCKED AFTER
  //
  //   * no TAI manager methods calls follow next in the scope of some single
  //     action (e.g., get/set a single attribute);
  //   * all TaiObject pointers retrieved from the TaiManager are released.
  //
  //  That means, if we retrieve a weak_ptr from the TaiManager, we MUST NOT
  //  unlock the mutex until the pointer is released.
  //
  mutable absl::Mutex tai_wrapper_mutex_;
  std::unique_ptr<TaiWrapperInterface> tai_wrapper_;

  // RW mutex lock for protecting the singleton instance initialization and
  // reading it back from other threads. Unlike other singleton classes, we
  // use RW lock as we need the pointer to class to be returned.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static TaiManager* singleton_;
  GUARDED_BY(init_lock_);
};

/*!
 * \brief TaiManager::GetValue method make TAI get value by \param attr_id TAI
 * attribute
 * \param attr_id it's TAI attribute from what value will be getted
 * \param module_netif_pair it's pair of module id and related to it networkif
 * id interface
 * \return valid value or invalid ::util::Status
 * \note method is thread safe a thread-safe.
 */
template <typename T>
::util::StatusOr<T> TaiManager::GetValue(
    const tai_attr_id_t& attr_id,
    const std::pair<uint64, uint32>& module_netif_pair) const {
  absl::ReaderMutexLock wrapper_lock(&tai_wrapper_mutex_);

  const std::shared_ptr<TaiObject> tai_object =
      tai_wrapper_->GetObject(TaiPathValidator::NetworkPath(module_netif_pair))
          .lock();
  if (!tai_object) {
    std::stringstream error_msg;
    error_msg << "Location of module id: " << module_netif_pair.first
              << " or network interface id: " << module_netif_pair.second
              << " is not valid";

    LOG(ERROR) << error_msg.str();
    RETURN_ERROR(ERR_INTERNAL) << error_msg.str();
  }

  // Retrieve the requested attribute from the TAI object.
  tai_status_t return_code;
  const TaiAttribute tai_attr = tai_object->GetAttribute(attr_id, &return_code);
  if (return_code != TAI_STATUS_SUCCESS) {
    std::stringstream error_msg;
    error_msg << "Can't get the attribute. TAI return code: " << return_code;

    LOG(ERROR) << error_msg.str();
    RETURN_ERROR(ERR_INTERNAL) << error_msg.str();
  }

  return ::util::StatusOr<T>(TaiAttributeToResponse<T>(tai_attr));
}

/*!
 * \brief TaiManager::SetValue method sets value \param value_to_set to TAI attr
 * contained in \param attr_id
 * \param value_to_set value that will be setted to \param attr_id
 * \param attr_id it's TAI attribute to what \param value_to_set will be setted
 * \param module_netif_pair it's pair of module id and related to it networkif
 * id interface
 * \return ::util::Status::OK_CODE if success or ::util::Status::error_code
 * otherwise
 * \note method is a thread-safe
 */
template <typename T>
::util::Status TaiManager::SetValue(
    const T& value_to_set, tai_attr_id_t attr_id,
    const std::pair<uint64, uint32>& module_netif_pair) const {
  absl::WriterMutexLock wrapper_lock(&tai_wrapper_mutex_);

  // Retrieve related TAI object.
  const std::shared_ptr<TaiObject> tai_object =
      tai_wrapper_->GetObject(TaiPathValidator::NetworkPath(module_netif_pair))
          .lock();
  if (!tai_object) {
    std::stringstream error_msg;
    error_msg << "Location of module id: " << module_netif_pair.first
              << " or network interface id: " << module_netif_pair.second
              << " is not valid";

    LOG(ERROR) << error_msg.str();
    return MAKE_ERROR(ERR_INTERNAL) << error_msg.str();
  }

  TaiAttribute tai_attribute = tai_object->GetAlocatedAttributeObject(attr_id);

  if (!SetValueToTaiAttribute(&tai_attribute, value_to_set))
    return MAKE_ERROR(ERR_INTERNAL) << "Not valid";

  if (!tai_attribute.IsValid() || !tai_object) {
    const std::string error_msg = "Unsupported set request";

    LOG(ERROR) << error_msg;
    return MAKE_ERROR(ERR_INTERNAL) << error_msg;
  }

  // Set the configured attribute to the TAI object.
  const tai_status_t return_code =
      tai_object->SetAttribute(&tai_attribute.attr);
  if (return_code != TAI_STATUS_SUCCESS) {
    std::stringstream error_msg;
    error_msg << "Can't set the attribute. TAI return code: " << return_code;

    LOG(ERROR) << error_msg.str();
    return MAKE_ERROR(ERR_INTERNAL) << error_msg.str();
  }

  return ::util::OkStatus();
}

/*!
 * \brief TaiManager::SetValueToTaiAttribute method sets
 * value \param value_to_set to special field in \param tai_attribute.
 * \param tai_attribute it's correctly created tai_attribute that will be setted
 * to TAI that should be initialized with \param value_to_set
 * \param value_to_set it's value that will be setted to TAI
 * \return true if success
 */
template <typename T>
bool TaiManager::SetValueToTaiAttribute(TaiAttribute* tai_attribute,
                                        const T& value_to_set) {
  if (!tai_attribute) {
    return false;
  }

  if (tai_attribute->attr.id == TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ) {
    tai_attribute->attr.value.u64 =
        TypesConverter::MegahertzToHertz(value_to_set);
    return true;
  }
  if (tai_attribute->attr.id == TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER) {
    tai_attribute->attr.value.flt = value_to_set;
    return true;
  }
  if (tai_attribute->attr.id == TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT) {
    tai_attribute->attr.value.s32 =
        TypesConverter::OperationalModeToModulation(value_to_set);
    return true;
  }

  return false;
}

/*!
 * \brief TaiManager::TaiAttributeToResponse method extract TAI value
 * from \param attribute and return it.
 * \param attribute contains value that returned TAI lib
 * \return value contained in \param attribute
 */
template <typename T>
T TaiManager::TaiAttributeToResponse(const TaiAttribute& attribute) {
  if (!attribute.IsValid()) return {};

  if (attribute.kMeta->objecttype == TAI_OBJECT_TYPE_NETWORKIF) {
    switch (attribute.attr.id) {
      case TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ:
        return attribute.attr.value.u64;

      case TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER:
        return attribute.attr.value.flt;

      case TAI_NETWORK_INTERFACE_ATTR_CURRENT_INPUT_POWER:
        return attribute.attr.value.flt;

      case TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT:
        return TypesConverter::ModulationToOperationalMode(
            attribute.attr.value.s32);

      default:
        break;
    }
  }

  return {};
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_WRAPPER_TAI_MANAGER_H_
