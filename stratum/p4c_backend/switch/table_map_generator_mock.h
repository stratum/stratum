// This is a mock implementation of TableMapGenerator.

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TABLE_MAP_GENERATOR_MOCK_H_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TABLE_MAP_GENERATOR_MOCK_H_

#include "platforms/networking/hercules/p4c_backend/switch/table_map_generator.h"
#include "testing/base/public/gmock.h"

namespace google {
namespace hercules {
namespace p4c_backend {

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

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TABLE_MAP_GENERATOR_MOCK_H_
