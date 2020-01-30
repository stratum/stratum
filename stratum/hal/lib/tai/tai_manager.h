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

#ifndef STRATUM_HAL_LIB_TAI_TAI_MANAGER_H_
#define STRATUM_HAL_LIB_TAI_TAI_MANAGER_H_

#include <memory>
#include <utility>

#include "stratum/hal/lib/tai/tai_wrapper.h"

#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"

namespace stratum {
namespace hal {
namespace tai {

/*!
 * \brief The TAIManager class provide single access point for user<->TAIAdapter
 * host interaction.
 */
class TAIManager {
 public:
  static TAIManager* Instance();
  static void Delete();

  ::util::StatusOr<DataResponse> GetValue(
      const DataRequest::Request& request,
      const std::pair<uint64, uint32>& module_netif_pair) const;

  ::util::Status SetValue(
      const SetRequest_Request& request,
      const std::pair<uint64, uint32>& module_netif_pair) const;

  bool IsObjectValid(const TAIPath& path) const;
  static bool IsRequestSupported(const SetRequest_Request& request);

  // Not copyable or movable
  TAIManager(const TAIManager&) = delete;
  TAIManager& operator=(const TAIManager&) = delete;
  TAIManager(TAIManager&&) = delete;
  TAIManager& operator=(const TAIManager&&) = delete;

 private:
  static TAIAttribute SetRequestToTAIAttribute(
      const SetRequest_Request& request,
      const std::shared_ptr<TAIObject>& kObject);
  static tai_attr_id_t SetRequestToTAIAttributeId(
      const SetRequest_Request& request);
  static tai_attr_id_t GetRequestToTAIAttributeId(
      const DataRequest::Request& request);
  static DataResponse TaiAttributeToResponse(const TAIAttribute& attr);

 protected:
  void SetTaiWrapper(std::unique_ptr<TAIWrapperInterface> wrapper);
  TAIManager() = default;

 private:
  // singleton implementation
  static TAIManager* tai_manager_;

  std::unique_ptr<TAIWrapperInterface> tai_wrapper_;
};

}  // namespace tai
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TAI_TAI_MANAGER_H_
