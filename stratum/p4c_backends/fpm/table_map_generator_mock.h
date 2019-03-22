/*
 * Copyright 2019 Google LLC
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

// This is a mock implementation of TableMapGenerator.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TABLE_MAP_GENERATOR_MOCK_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TABLE_MAP_GENERATOR_MOCK_H_

#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "gmock/gmock.h"

namespace stratum {
namespace p4c_backends {

class TableMapGeneratorMock : public TableMapGenerator {
 public:
  MOCK_METHOD1(AddField, void(const std::string& field_name));
  MOCK_METHOD2(SetFieldType, void(
      const std::string& field_name, P4FieldType type));
  MOCK_METHOD5(SetFieldAttributes, void(
      const std::string& field_name, P4FieldType field_type,
      P4HeaderType header_type, uint32_t bit_offset, uint32_t bit_width));
  MOCK_METHOD1(SetFieldLocalMetadataFlag, void(const std::string& field_name));
  MOCK_METHOD3(SetFieldValueSet, void(
      const std::string& field_name, const std::string& value_set_name,
      P4HeaderType header_type));
  MOCK_METHOD3(AddFieldMatch, void(
      const std::string& field_name, const std::string& match_type,
      int bit_width));
  MOCK_METHOD2(ReplaceFieldDescriptor,
               void(const std::string& field_name,
                    const hal::P4FieldDescriptor& new_descriptor));
  MOCK_METHOD1(AddAction, void(const std::string& action_name));
  MOCK_METHOD3(AssignActionSourceValueToField, void(
      const std::string& action_name,
      const P4AssignSourceValue& source_value,
      const std::string& field_name));
  MOCK_METHOD3(AssignActionParameterToField, void(
      const std::string& action_name, const std::string& param_name,
      const std::string& field_name));
  MOCK_METHOD3(AssignHeaderToHeader, void(
      const std::string& action_name, const P4AssignSourceValue& source_header,
      const std::string& destination_header));
  MOCK_METHOD1(AddDropPrimitive, void(const std::string& action_name));
  MOCK_METHOD1(AddNopPrimitive, void(const std::string& action_name));
  MOCK_METHOD2(AddMeterColorAction, void(
      const std::string& action_name,
      const hal::P4ActionDescriptor::P4MeterColorAction& color_action));
  MOCK_METHOD2(AddMeterColorActionsFromString, void(
      const std::string& action_name, const std::string& color_actions));
  MOCK_METHOD2(AddTunnelAction, void(
      const std::string& action_name,
      const hal::P4ActionDescriptor::P4TunnelAction& tunnel_action));
  MOCK_METHOD2(ReplaceActionDescriptor, void(
      const std::string& action_name,
      const hal::P4ActionDescriptor& new_descriptor));
  MOCK_METHOD1(AddTable, void(const std::string& table_name));
  MOCK_METHOD2(SetTableType, void(
      const std::string& table_name, P4TableType type));
  MOCK_METHOD1(SetTableStaticEntriesFlag, void(const std::string& table_name));
  MOCK_METHOD2(SetTableValidHeaders, void(
      const std::string& table_name,
      const std::set<std::string>& header_names));
  MOCK_METHOD1(AddHeader, void(const std::string& header_name));
  MOCK_METHOD3(SetHeaderAttributes, void(
      const std::string& header_name, P4HeaderType type, int32 depth));
  MOCK_METHOD2(AddInternalAction, void(
      const std::string& action_name,
      const hal::P4ActionDescriptor& internal_descriptor));
  MOCK_CONST_METHOD0(generated_map, const hal::P4PipelineConfig&());
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TABLE_MAP_GENERATOR_MOCK_H_
