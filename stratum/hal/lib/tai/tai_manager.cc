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


#include "stratum/hal/lib/tai/tai_manager.h"

#include <utility>
#include <memory>

#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/tai/types_converter.h"

namespace stratum {
namespace hal {
namespace tai {

TAIManager* TAIManager::tai_manager_ = nullptr;

TAIManager* TAIManager::Instance() {
  if (!tai_manager_) {
    tai_manager_ = new TAIManager();
    tai_manager_->SetTaiWrapper(absl::make_unique<TAIWrapper>());
  }

  return tai_manager_;
}

void TAIManager::Delete() {
  delete tai_manager_;
  tai_manager_ = nullptr;
}

/*!
 * \brief TAIManager::GetValue method make TAI get value by attribute contained
 * in \param request
 * \param request it's gNMI request value that contains what value will be
 * returned
 * \param module_netif_pair it's pair of module id and related to it networkif
 * id interface
 * \return valid DataResponse or invalid ::util::Status
 */
::util::StatusOr<DataResponse> TAIManager::GetValue(
    const DataRequest::Request& request,
    const std::pair<uint64, uint32>& module_netif_pair) const {
  LOG(INFO) << __FUNCTION__;

  const std::shared_ptr<TAIObject> kObject =
      tai_wrapper_->GetObject(TAIPathValidator::NetworkPath(module_netif_pair))
          .lock();
  if (!kObject) {
    LOG(ERROR) << "Location of module id: " << module_netif_pair.first
               << " or network interface id: " << module_netif_pair.second
               << " is not valid";

    return ::util::Status(::util::error::INTERNAL,
                          "Invalid module or/and network interface id");
  }

  tai_status_t return_code;
  const TAIAttribute kAttr = kObject->GetAttribute(
      GetRequestToTAIAttributeId(request), &return_code);
  if (return_code != TAI_STATUS_SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Can't get requested attribute. TAI lib return error code: "
           << return_code;
  }

  LOG(INFO) << __FUNCTION__ << " end";
  return ::util::StatusOr<DataResponse>(TaiAttributeToResponse(kAttr));
}

/*!
 * \brief TAIManager::SetValue method make TAI set value by attribute and value
 * contained in \param request
 * \param request it's gNMI request value that contains what value will be
 * returned
 * \param module_netif_pair it's pair of module id and related to it networkif
 * id interface
 * \return ::util::Status::OK_CODE if success or ::util::Status::error_code
 * otherwise
 */
util::Status TAIManager::SetValue(
    const SetRequest_Request& request,
    const std::pair<uint64, uint32>& module_netif_pair) const {
  LOG(INFO) << __FUNCTION__;

  const std::shared_ptr<TAIObject> kObject =
      tai_wrapper_->GetObject(TAIPathValidator::NetworkPath(module_netif_pair))
          .lock();
  if (!kObject) {
    LOG(ERROR) << "Location of module id: " << module_netif_pair.first
               << " or network interface id: " << module_netif_pair.second
               << " is not valid";

    return ::util::Status(::util::error::INTERNAL,
                          "Invalid module or/and network interface id");
  }

  TAIAttribute tai_attribute =
      SetRequestToTAIAttribute(request, kObject);
  if (!tai_attribute.IsValid()) {
    LOG(ERROR) << "Unsupported set-request";
    return ::util::Status(::util::error::INTERNAL, "Unsupported set-request");
  }

  const tai_status_t kStatusCode =
      kObject->SetAttribute(&tai_attribute.attr);
  if (kStatusCode != TAI_STATUS_SUCCESS) {
    LOG(ERROR) << "Can't set value. Returned error code: " << kStatusCode;
    return ::util::Status(::util::error::INTERNAL, "Can't set request data");
  }

  LOG(INFO) << __FUNCTION__ << " end";
  return ::util::OkStatus();
}

/*!
 * \brief TAIManager::IsObjectValid check is \param path is valid
 * \return true if valid
 */
bool TAIManager::IsObjectValid(const TAIPath& path) const {
  return tai_wrapper_->IsObjectValid(path);
}

bool TAIManager::IsRequestSupported(const SetRequest_Request& request) {
  return SetRequestToTAIAttributeId(request) != TAI_INVALID_ATTRIBUTE_ID;
}

/*!
 * \brief TAIManager::SetRequestToTAIAttribute method converts \param request
 * value to TAIAttribute value
 * \param kObject it's tai interface from what TAIAttribute will be created
 * \return valid TAIAttribute if success
 */
TAIAttribute TAIManager::SetRequestToTAIAttribute(
    const SetRequest_Request& request,
    const std::shared_ptr<TAIObject>& kObject) {
  LOG(INFO) << __FUNCTION__;
  const tai_attr_id_t kAttrId = SetRequestToTAIAttributeId(request);

  if (kObject == nullptr || kAttrId == TAI_INVALID_ATTRIBUTE_ID) {
    return TAIAttribute::InvalidAttributeObject();
  }

  TAIAttribute tai_attribute = kObject->GetAlocatedAttributeObject(kAttrId);

  if (kAttrId == TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ) {
    tai_attribute.attr.value.u64 =
        TypesConverter::MegahertzToHertz(request.port().frequency().value());
    return tai_attribute;
  }
  if (kAttrId == TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER) {
    tai_attribute.attr.value.flt = request.port().output_power().instant();
    return tai_attribute;
  }

  return TAIAttribute::InvalidAttributeObject();
}

tai_attr_id_t TAIManager::SetRequestToTAIAttributeId(
    const SetRequest_Request& request) {
  LOG(INFO) << __FUNCTION__;
  if (!request.has_port()) {
    return TAI_INVALID_ATTRIBUTE_ID;
  }

  switch (request.port().value_case()) {
    case SetRequest::Request::Port::ValueCase::kFrequency:
      return TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ;

    case SetRequest::Request::Port::ValueCase::kOutputPower: {
      return TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER;
    }
    default:
      return TAI_INVALID_ATTRIBUTE_ID;
  }

  return TAI_INVALID_ATTRIBUTE_ID;
}

/*!
 * \brief TAIManager::GetRequestToTAIAttributeId method converts \param request
 * to TAI interface method id
 * \param request indicates what TAI interface method should be used
 * \return TAI interface method if success, otherwise: TAI_INVALID_ATTRIBUTE_ID
 */
tai_attr_id_t TAIManager::GetRequestToTAIAttributeId(
    const DataRequest::Request& request) {
  LOG(INFO) << __FUNCTION__;
  switch (request.request_case()) {
    case DataRequest::Request::kFrequency:
      return TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ;

    case DataRequest::Request::kOutputPower:
      return TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER;

    case DataRequest::Request::kInputPower:
      return TAI_NETWORK_INTERFACE_ATTR_CURRENT_INPUT_POWER;

    default:
      return TAI_INVALID_ATTRIBUTE_ID;
  }

  return TAI_INVALID_ATTRIBUTE_ID;
}

/*!
 * \brief TAIManager::TaiAttributeToResponse method converts \param attribute to
 * OpenConfig value
 * \param attribute contains value that returned TAI lib
 * \return DataResponse with valid value if success, otherwise uninitialized obj
 */
DataResponse TAIManager::TaiAttributeToResponse(const TAIAttribute& attribute) {
  LOG(INFO) << __FUNCTION__;
  DataResponse resp;
  if (!attribute.IsValid()) return {};

  if (attribute.kMeta->objecttype == TAI_OBJECT_TYPE_NETWORKIF) {
    switch (attribute.attr.id) {
      case TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ:
        resp.mutable_frequency()->set_value(
            TypesConverter::HertzToMegahertz(attribute.attr.value.u64));
        break;

      case TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER:
        resp.mutable_output_power()->set_instant(attribute.attr.value.flt);
        break;

      case TAI_NETWORK_INTERFACE_ATTR_CURRENT_INPUT_POWER:
        resp.mutable_input_power()->set_instant(attribute.attr.value.flt);
        break;

      default:
        break;
    }
  }

  return resp;
}

void TAIManager::SetTaiWrapper(std::unique_ptr<TAIWrapperInterface> wrapper) {
  tai_wrapper_ = std::move(wrapper);
}

}  // namespace tai
}  // namespace hal
}  // namespace stratum
