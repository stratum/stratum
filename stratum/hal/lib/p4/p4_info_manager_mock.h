/*
 * Copyright 2018 Google LLC
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


// This is a mock implementation of P4InfoManager.

#ifndef STRATUM_HAL_LIB_P4_P4_INFO_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_P4_P4_INFO_MANAGER_MOCK_H_

#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {

class P4InfoManagerMock : public P4InfoManager {
 public:
  MOCK_METHOD0(InitializeAndVerify, ::util::Status());
  MOCK_CONST_METHOD1(
      FindTableByID,
      ::util::StatusOr<const p4::config::Table>(uint32_t table_id));
  MOCK_CONST_METHOD1(
      FindTableByName,
      ::util::StatusOr<const p4::config::Table>(std::string table_name));
  MOCK_CONST_METHOD1(
      FindActionByID,
      ::util::StatusOr<const p4::config::Action>(uint32_t action_id));
  MOCK_CONST_METHOD1(
      FindActionByName,
      ::util::StatusOr<const p4::config::Action>(std::string action_name));
  MOCK_CONST_METHOD1(
      FindActionProfileByID,
      ::util::StatusOr<const p4::config::ActionProfile>(uint32_t profile_id));
  MOCK_CONST_METHOD1(
      FindActionProfileByName,
      ::util::StatusOr<const p4::config::ActionProfile>(
          std::string profile_name));
  MOCK_CONST_METHOD1(
      FindCounterByID,
      ::util::StatusOr<const p4::config::Counter>(uint32_t counter_id));
  MOCK_CONST_METHOD1(
      FindCounterByName,
      ::util::StatusOr<const p4::config::Counter>(std::string counter_name));
  MOCK_CONST_METHOD1(
      FindMeterByID,
      ::util::StatusOr<const p4::config::Meter>(uint32_t meter_id));
  MOCK_CONST_METHOD1(
      FindMeterByName,
      ::util::StatusOr<const p4::config::Meter>(std::string meter_name));
  MOCK_CONST_METHOD1(
      GetSwitchStackAnnotations,
      ::util::StatusOr<P4Annotation>(const std::string& p4_object_name));
  MOCK_CONST_METHOD0(DumpNamesToIDs, void());
  MOCK_CONST_METHOD0(p4_info, const p4::config::P4Info&());
  MOCK_METHOD0(VerifyRequiredObjects, ::util::Status());
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_INFO_MANAGER_MOCK_H_
