// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "stratum/hal/lib/p4/p4_table_mapper.h"

#include <cstdarg>
#include <memory>
#include <set>
#include <string>

#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/p4/p4_static_entry_mapper_mock.h"
#include "stratum/lib/utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/integral_types.h"
#include "absl/memory/memory.h"

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

namespace stratum {
namespace hal {

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

// The unnamed namespace wraps a local function to encode the byte strings
// that P4 often uses as match field and parameter values.
namespace {

std::string EncodeByteValue(int arg_count...) {
  std::string byte_value;
  va_list args;
  va_start(args, arg_count);

  for (int arg = 0; arg < arg_count; ++arg) {
    uint8_t byte = va_arg(args, int);
    byte_value.push_back(byte);
  }

  va_end(args);
  return byte_value;
}

constexpr char kTestP4InfoFile[] =
    "stratum/hal/lib/p4/testdata/"
    "test_p4_info.pb.txt";
constexpr char kTestP4PipelineConfigFile[] =
    "stratum/hal/lib/p4/testdata/"
    "test_p4_pipeline_config.pb.txt";
constexpr char kEmptyP4PipelineConfigFile[] =
    "stratum/hal/lib/p4/testdata/"
    "empty_p4_pipeline_config.pb.txt";

}  // namespace

// This class is the P4TableMapper test fixture.
class P4TableMapperTest : public testing::Test {
 protected:
  void SetUp() override {
    p4_table_mapper_ = P4TableMapper::CreateInstance();
    static_entry_mapper_mock_ = new P4StaticEntryMapperMock();
    p4_table_mapper_->set_static_entry_mapper(static_entry_mapper_mock_);
    ASSERT_OK(ReadProtoFromTextFile(
        kTestP4InfoFile, forwarding_pipeline_config_.mutable_p4info()));
    P4PipelineConfig p4_pipeline_config;
    ASSERT_OK(
        ReadProtoFromTextFile(kTestP4PipelineConfigFile, &p4_pipeline_config));
    ASSERT_TRUE(p4_pipeline_config.SerializeToString(
        forwarding_pipeline_config_.mutable_p4_device_config()));
    // Just to make sure the P4Info is valid.
    p4_info_manager_ =
        absl::make_unique<P4InfoManager>(forwarding_pipeline_config_.p4info());
    ASSERT_OK(p4_info_manager_->InitializeAndVerify());
  }

  // Fills table_entry_ with a basic TableEntry for unit tests, leaving the
  // P4Info for table_name in table_. No match fields and actions are populated.
  void SetUpTableID(const std::string& table_name) {
    auto ret = p4_info_manager_->FindTableByName(table_name);
    ASSERT_TRUE(ret.ok()) << "Error: " << ret.status();
    table_ = ret.ValueOrDie();
    ASSERT_LE(1, table_.action_refs_size());
    ASSERT_LE(1, table_.match_fields_size());
    table_entry_.set_table_id(table_.preamble().id());
  }

  // Fills table_entry_ with a basic TableEntry to test match field mapping.
  // Upon return, table_ contains a table_id, the action ID from the first
  // action in the table's P4Info (usually NOP), and the field ID from the
  // table's first P4Info match field.  The table's P4Info goes into table_.
  void SetUpMatchFieldTest(const std::string& table_name) {
    ASSERT_NO_FATAL_FAILURE(SetUpTableID(table_name));
    table_entry_.mutable_action()->mutable_action()->set_action_id(
        table_.action_refs(0).id());
    auto match_field = table_entry_.add_match();
    match_field->set_field_id(table_.match_fields(0).id());
  }

  // SetUpMultiMatchFieldTest uses SetUpMatchFieldTest for basic setup, then
  // updates table_entry_ according to the match fields defined by the input
  // table_name.
  void SetUpMultiMatchFieldTest(const std::string& table_name) {
    SetUpMatchFieldTest(table_name);
    table_entry_.mutable_match()->Clear();
    for (const auto& match_field : table_.match_fields()) {
      auto new_match = table_entry_.add_match();
      new_match->set_field_id(match_field.id());
      // The multi-match tables in the test file use LPM, EXACT, and TERNARY
      // fields.  EXACT fields need a specific value below.  Other types
      // have empty values indicating a default/don't care match.  Tests
      // can adjust as needed.  EXACT field bit widths for by multi-field
      // tests are expected to be 64 bits.
      if (match_field.match_type() == ::p4::config::v1::MatchField::EXACT) {
        ASSERT_EQ(64, match_field.bitwidth());
        new_match->mutable_exact()->set_value(
            EncodeByteValue(8, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88));
      }
    }
  }

  // Fills table_entry_ with a basic TableEntry to test table action mapping.
  // Tests for table actions always use the "action-test-table" from the test
  // data input file.  The match field is encoded according to the table's
  // first entry and given a dummy byte value.  The calling test must set all
  // of the action data.
  void SetUpTableActionTest() {
    ASSERT_NO_FATAL_FAILURE(SetUpTableID("action-test-table"));
    auto match_field = table_entry_.add_match();
    match_field->set_field_id(table_.match_fields(0).id());
    std::string byte_value = "128bit-match-key";
    match_field->mutable_exact()->set_value(byte_value);
  }

  // Prepares for an action profile test using "action-profile-test-table"
  // from the test data input file.  The table's P4Info will be left in
  // table_. The action profile ID for "test-action-profile-1" is inserted into
  // action_profile_member_ and action_profile_group_. Tests will later encode
  // member data in action_profile_member_ or group data in
  // action_profile_group_.
  void SetUpActionProfileTest() {
    ASSERT_NO_FATAL_FAILURE(SetUpTableID("action-profile-test-table"));
    auto status =
        p4_info_manager_->FindActionProfileByName("test-action-profile-1");
    ASSERT_TRUE(status.ok());
    auto profile_info = status.ValueOrDie();
    action_profile_member_.set_action_profile_id(profile_info.preamble().id());
    action_profile_group_.set_action_profile_id(profile_info.preamble().id());
  }

  // P4TableMapper for tests.
  std::unique_ptr<P4TableMapper> p4_table_mapper_;

  // P4InfoManager used for verifying P4Info and accessing some info from
  // the given P4Info.
  std::unique_ptr<P4InfoManager> p4_info_manager_;

  // The SetUp function creates the mock and transfers ownership to the tested
  // P4TableMapper.
  P4StaticEntryMapperMock* static_entry_mapper_mock_;

  // Test ForwardingPipelineConfig proto for testing.
  ::p4::v1::ForwardingPipelineConfig forwarding_pipeline_config_;

  // Table proto in P4Info for table in table_entry_.
  ::p4::config::v1::Table table_;

  // For tests to setup table mapping.
  ::p4::v1::TableEntry table_entry_;

  // For action profile mapping tests.
  ::p4::v1::ActionProfileMember action_profile_member_;
  ::p4::v1::ActionProfileGroup action_profile_group_;
};

// Pushes a normal valid forwarding pipeline spec.
TEST_F(P4TableMapperTest, PushForwardingPipelineConfigSuccess) {
  EXPECT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
}

// Verifies a normal valid forwarding pipeline spec.
TEST_F(P4TableMapperTest, VerifyForwardingPipelineConfigSuccess) {
  EXPECT_OK(p4_table_mapper_->VerifyForwardingPipelineConfig(
      forwarding_pipeline_config_));
}

// Verifies a forwarding pipeline spec with a missing table map data file.
TEST_F(P4TableMapperTest,
       VerifyForwardingPipelineConfigFailureInvalidTableMapData) {
  // Override p4_device_config with an invalid data.
  forwarding_pipeline_config_.set_p4_device_config("invalid data!");

  ::util::Status status = p4_table_mapper_->VerifyForwardingPipelineConfig(
      forwarding_pipeline_config_);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Failed to parse p4_device_config"));
}

// Verifies a forwarding pipeline spec with empty table map data.
TEST_F(P4TableMapperTest, VerifyForwardingPipelineConfigFailureEmptyTableMap) {
  // Override p4_device_config with an invalid data.
  P4PipelineConfig p4_pipeline_config;
  ASSERT_OK(
      ReadProtoFromTextFile(kEmptyP4PipelineConfigFile, &p4_pipeline_config));
  ASSERT_TRUE(p4_pipeline_config.SerializeToString(
      forwarding_pipeline_config_.mutable_p4_device_config()));

  ::util::Status status = p4_table_mapper_->VerifyForwardingPipelineConfig(
      forwarding_pipeline_config_);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("missing object mapping descriptors"));
}

// Verifies a forwarding pipeline spec change requiring reboot.
TEST_F(P4TableMapperTest, PushForwardingPipelineConfigReboot) {
  // This test first pushes a modified version of pipeline config. When the
  // original pipeline config is subsequently verified ERR_REBOOT_REQUIRED
  // status is returned.
  ::p4::v1::ForwardingPipelineConfig modified_pipeline_config =
      forwarding_pipeline_config_;
  {
    // Mutate the config. Add an extra static entry.
    P4PipelineConfig p4_pipeline_config;
    p4_pipeline_config.ParseFromString(
        forwarding_pipeline_config_.p4_device_config());
    ASSERT_LE(1, forwarding_pipeline_config_.p4info().tables_size());
    ::p4::v1::TableEntry static_table_entry;
    static_table_entry.set_table_id(
        forwarding_pipeline_config_.p4info().tables(0).preamble().id());
    ::p4::v1::WriteRequest* test_write_request =
        p4_pipeline_config.mutable_static_table_entries();
    ::p4::v1::Update* update = test_write_request->add_updates();
    update->set_type(::p4::v1::Update::INSERT);
    *(update->mutable_entity()->mutable_table_entry()) = static_table_entry;
    ASSERT_TRUE(p4_pipeline_config.SerializeToString(
        modified_pipeline_config.mutable_p4_device_config()));
  }
  ASSERT_OK(
      p4_table_mapper_->PushForwardingPipelineConfig(modified_pipeline_config));
  ::util::Status status = p4_table_mapper_->VerifyForwardingPipelineConfig(
      forwarding_pipeline_config_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_REBOOT_REQUIRED, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("require a reboot"));
}

// TODO: Many of the tests below that expect ERR_OPER_NOT_SUPPORTED
// need to expect status.ok() once P4TableMapper is complete.

// Tests mapping of an exact field with U64 value conversion when table type is
// not given and we expect pipeline stage to be populated.
TEST_F(P4TableMapperTest, TestPipelneStageIsPopulated) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("fallback-to-stage-test-table");
  auto match_field = table_entry_.mutable_match(0);
  match_field->mutable_exact()->set_value(EncodeByteValue(4, 10, 2, 255, 4));

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_TRUE(flow_entry.has_action());
  ASSERT_EQ(1, flow_entry.fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_IPV4_DST, flow_entry.fields(0).type());
  EXPECT_EQ(128, flow_entry.fields(0).bit_offset());
  EXPECT_EQ(32, flow_entry.fields(0).bit_width());
  EXPECT_EQ(P4_HEADER_IPV4, flow_entry.fields(0).header_type());
  const uint32 kExpectedU32 = 0x0a02ff04;
  EXPECT_EQ(kExpectedU32, flow_entry.fields(0).value().u32());
  EXPECT_TRUE(flow_entry.has_table_info());
  EXPECT_EQ(table_.preamble().id(), flow_entry.table_info().id());
  EXPECT_EQ("fallback-to-stage-test-table", flow_entry.table_info().name());
  EXPECT_EQ(P4_TABLE_UNKNOWN, flow_entry.table_info().type());
  EXPECT_EQ(P4Annotation::L2, flow_entry.table_info().pipeline_stage());
  EXPECT_EQ(table_.preamble().annotations_size(),
            flow_entry.table_info().annotations_size());
}

// Tests mapping of an exact field with U32 value conversion.
TEST_F(P4TableMapperTest, TestU32ExactField) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("exact-match-32-table");
  auto match_field = table_entry_.mutable_match(0);
  match_field->mutable_exact()->set_value(EncodeByteValue(4, 10, 2, 255, 4));

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_TRUE(flow_entry.has_action());
  ASSERT_EQ(1, flow_entry.fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_IPV4_DST, flow_entry.fields(0).type());
  EXPECT_EQ(128, flow_entry.fields(0).bit_offset());
  EXPECT_EQ(32, flow_entry.fields(0).bit_width());
  EXPECT_EQ(P4_HEADER_IPV4, flow_entry.fields(0).header_type());
  const uint32 kExpectedU32 = 0x0a02ff04;
  EXPECT_EQ(kExpectedU32, flow_entry.fields(0).value().u32());
  EXPECT_TRUE(flow_entry.has_table_info());
  EXPECT_EQ(table_.preamble().id(), flow_entry.table_info().id());
  EXPECT_EQ("exact-match-32-table", flow_entry.table_info().name());
  EXPECT_EQ(P4_TABLE_L3_IP, flow_entry.table_info().type());
  EXPECT_EQ(table_.preamble().annotations_size(),
            flow_entry.table_info().annotations_size());
}

// Tests mapping of an LPM field with U32 value and mask conversion.
TEST_F(P4TableMapperTest, TestU32LPMField) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("lpm-match-32-table");
  auto match_field = table_entry_.mutable_match(0);
  match_field->mutable_lpm()->set_value(EncodeByteValue(4, 192, 168, 1, 0));
  match_field->mutable_lpm()->set_prefix_len(24);

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_TRUE(flow_entry.has_action());
  ASSERT_EQ(1, flow_entry.fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_IPV4_DST, flow_entry.fields(0).type());
  EXPECT_EQ(128, flow_entry.fields(0).bit_offset());
  EXPECT_EQ(32, flow_entry.fields(0).bit_width());
  EXPECT_EQ(P4_HEADER_IPV4, flow_entry.fields(0).header_type());
  const uint32 kExpectedU32 = 0xc0a80100;
  EXPECT_EQ(kExpectedU32, flow_entry.fields(0).value().u32());
  const uint32 kExpectedMask = 0xffffff00;
  EXPECT_EQ(kExpectedMask, flow_entry.fields(0).mask().u32());
  EXPECT_EQ(table_.preamble().id(), flow_entry.table_info().id());
  EXPECT_EQ("lpm-match-32-table", flow_entry.table_info().name());
  EXPECT_EQ(P4_TABLE_L3_IP, flow_entry.table_info().type());
  EXPECT_EQ(table_.preamble().annotations_size(),
            flow_entry.table_info().annotations_size());
}

// Tests mapping of an exact field with U64 value conversion.
TEST_F(P4TableMapperTest, TestU64ExactField) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("exact-match-64-table");
  auto match_field = table_entry_.mutable_match(0);
  match_field->mutable_exact()->set_value(
      EncodeByteValue(8, 0x00, 0x00, 0xab, 0xcd, 0xef, 0x11, 0x22, 0x33));

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_TRUE(flow_entry.has_action());
  ASSERT_EQ(1, flow_entry.fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_DST, flow_entry.fields(0).type());
  EXPECT_EQ(0, flow_entry.fields(0).bit_offset());
  EXPECT_EQ(64, flow_entry.fields(0).bit_width());
  EXPECT_EQ(P4_HEADER_ETHERNET, flow_entry.fields(0).header_type());
  const uint64 kExpectedU64 = 0xabcdef112233ULL;
  EXPECT_EQ(kExpectedU64, flow_entry.fields(0).value().u64());
  EXPECT_EQ(table_.preamble().id(), flow_entry.table_info().id());
  EXPECT_EQ("exact-match-64-table", flow_entry.table_info().name());
  EXPECT_EQ(P4_TABLE_L2_MY_STATION, flow_entry.table_info().type());
  EXPECT_EQ(table_.preamble().annotations_size(),
            flow_entry.table_info().annotations_size());
}

// Tests mapping of an exact field with 128-bit value.
TEST_F(P4TableMapperTest, TestU128ExactField) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("exact-match-bytes-table");
  std::string byte_value;
  for (int i = 0; i < 16; ++i) byte_value.push_back(i * 4);
  auto match_field = table_entry_.mutable_match(0);
  match_field->mutable_exact()->set_value(byte_value);

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_TRUE(flow_entry.has_action());
  ASSERT_EQ(1, flow_entry.fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_DST, flow_entry.fields(0).type());
  EXPECT_EQ(0, flow_entry.fields(0).bit_offset());
  EXPECT_EQ(128, flow_entry.fields(0).bit_width());
  EXPECT_EQ(P4_HEADER_ETHERNET, flow_entry.fields(0).header_type());
  EXPECT_EQ(byte_value, flow_entry.fields(0).value().b());
  EXPECT_EQ(table_.preamble().id(), flow_entry.table_info().id());
  EXPECT_EQ("exact-match-bytes-table", flow_entry.table_info().name());
  EXPECT_EQ(P4_TABLE_L2_MY_STATION, flow_entry.table_info().type());
  EXPECT_EQ(table_.preamble().annotations_size(),
            flow_entry.table_info().annotations_size());
}

// Tests mapping of an LPM field with 128-bit value and mask conversion.
TEST_F(P4TableMapperTest, TestU128LPMField) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("lpm-match-bytes-table");
  std::string byte_value;
  for (int i = 0; i < 16; ++i) byte_value.push_back(i * 8);
  auto match_field = table_entry_.mutable_match(0);
  match_field->mutable_lpm()->set_value(byte_value);
  match_field->mutable_lpm()->set_prefix_len(125);

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_TRUE(flow_entry.has_action());
  ASSERT_EQ(1, flow_entry.fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_DST, flow_entry.fields(0).type());
  EXPECT_EQ(0, flow_entry.fields(0).bit_offset());
  EXPECT_EQ(128, flow_entry.fields(0).bit_width());
  EXPECT_EQ(P4_HEADER_ETHERNET, flow_entry.fields(0).header_type());
  EXPECT_EQ(byte_value, flow_entry.fields(0).value().b());
  std::string expected_mask;
  for (int i = 0; i < 15; ++i) expected_mask.push_back(0xff);
  expected_mask.push_back(0xf8);
  EXPECT_EQ(expected_mask, flow_entry.fields(0).mask().b());
  EXPECT_EQ(table_.preamble().id(), flow_entry.table_info().id());
  EXPECT_EQ("lpm-match-bytes-table", flow_entry.table_info().name());
  EXPECT_EQ(P4_TABLE_L2_MY_STATION, flow_entry.table_info().type());
  EXPECT_EQ(table_.preamble().annotations_size(),
            flow_entry.table_info().annotations_size());
}

// Tests mapping of an action with an action profile member ID.
TEST_F(P4TableMapperTest, TestTableActionProfileMemberID) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpTableActionTest();
  auto action = table_entry_.mutable_action();
  action->set_action_profile_member_id(2);

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_LT(0, flow_entry.fields_size());
  EXPECT_TRUE(flow_entry.has_action());
  EXPECT_EQ(P4_ACTION_TYPE_PROFILE_MEMBER_ID, flow_entry.action().type());
  EXPECT_EQ(action->action_profile_member_id(),
            flow_entry.action().profile_member_id());
}

// Tests mapping of an action with an action profile group ID.
TEST_F(P4TableMapperTest, TestTableActionProfileGroupID) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpTableActionTest();
  auto action = table_entry_.mutable_action();
  action->set_action_profile_group_id(678);

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_LT(0, flow_entry.fields_size());
  EXPECT_TRUE(flow_entry.has_action());
  EXPECT_EQ(P4_ACTION_TYPE_PROFILE_GROUP_ID, flow_entry.action().type());
  EXPECT_EQ(action->action_profile_group_id(),
            flow_entry.action().profile_group_id());
}

// Tests mapping of a primitive NOP action.
TEST_F(P4TableMapperTest, TestTableActionNOP) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpTableActionTest();
  auto status = p4_info_manager_->FindActionByName("nop");
  ASSERT_TRUE(status.ok());
  const auto action_info = status.ValueOrDie();
  auto action = table_entry_.mutable_action()->mutable_action();
  action->set_action_id(action_info.preamble().id());

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_LT(0, flow_entry.fields_size());
  EXPECT_TRUE(flow_entry.has_action());
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, flow_entry.action().type());
  EXPECT_TRUE(flow_entry.action().has_function());
  const auto& action_function = flow_entry.action().function();
  ASSERT_EQ(1, action_function.primitives_size());
  EXPECT_EQ(P4_ACTION_OP_NOP, action_function.primitives(0).op_code());
}

// Tests mapping of an action with 32-bit parameter.
TEST_F(P4TableMapperTest, TestTableActionU32Param) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpTableActionTest();
  auto status = p4_info_manager_->FindActionByName("set-32");
  ASSERT_TRUE(status.ok());
  const auto action_info = status.ValueOrDie();
  auto action = table_entry_.mutable_action()->mutable_action();
  action->set_action_id(action_info.preamble().id());
  auto param = action->add_params();
  ASSERT_LE(1, action_info.params_size());
  param->set_param_id(action_info.params(0).id());
  param->set_value(EncodeByteValue(4, 192, 168, 1, 1));

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_LT(0, flow_entry.fields_size());
  EXPECT_TRUE(flow_entry.has_action());
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, flow_entry.action().type());
  EXPECT_TRUE(flow_entry.action().has_function());
  const auto& action_function = flow_entry.action().function();
  EXPECT_EQ(0, action_function.primitives_size());
  ASSERT_EQ(1, action_function.modify_fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_IPV4_DST, action_function.modify_fields(0).type());
  const uint32 kExpectedValue = 0xc0a80101;
  EXPECT_EQ(kExpectedValue, action_function.modify_fields(0).u32());
}

// Tests mapping of an action with 64-bit parameter.
TEST_F(P4TableMapperTest, TestTableActionU64Param) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpTableActionTest();
  auto status = p4_info_manager_->FindActionByName("set-64");
  ASSERT_TRUE(status.ok());
  const auto action_info = status.ValueOrDie();
  auto action = table_entry_.mutable_action()->mutable_action();
  action->set_action_id(action_info.preamble().id());
  auto param = action->add_params();
  ASSERT_LE(1, action_info.params_size());
  param->set_param_id(action_info.params(0).id());
  param->set_value(EncodeByteValue(8, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0, 0, 0));

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_LT(0, flow_entry.fields_size());
  EXPECT_TRUE(flow_entry.has_action());
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, flow_entry.action().type());
  EXPECT_TRUE(flow_entry.action().has_function());
  const auto& action_function = flow_entry.action().function();
  EXPECT_EQ(0, action_function.primitives_size());
  ASSERT_EQ(1, action_function.modify_fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_DST, action_function.modify_fields(0).type());
  const uint64 kExpectedValue = 0xffeeddccbb000000ULL;
  EXPECT_EQ(kExpectedValue, action_function.modify_fields(0).u64());
}

// Tests mapping of an action with byte value parameter.
TEST_F(P4TableMapperTest, TestTableActionByteParam) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpTableActionTest();
  auto status = p4_info_manager_->FindActionByName("set-bytes");
  ASSERT_TRUE(status.ok());
  const auto action_info = status.ValueOrDie();
  auto action = table_entry_.mutable_action()->mutable_action();
  action->set_action_id(action_info.preamble().id());
  auto param = action->add_params();
  ASSERT_LE(1, action_info.params_size());
  param->set_param_id(action_info.params(0).id());
  const std::string test_bytes_value("12345678");
  param->set_value(test_bytes_value);

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_LT(0, flow_entry.fields_size());
  EXPECT_TRUE(flow_entry.has_action());
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, flow_entry.action().type());
  EXPECT_TRUE(flow_entry.action().has_function());
  const auto& action_function = flow_entry.action().function();
  EXPECT_EQ(0, action_function.primitives_size());
  ASSERT_EQ(1, action_function.modify_fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_DST, action_function.modify_fields(0).type());
  EXPECT_EQ(test_bytes_value, action_function.modify_fields(0).b());
}

// Tests mapping of an action with multiple parameters.
TEST_F(P4TableMapperTest, TestTableActionMultiParam) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpTableActionTest();
  auto status = p4_info_manager_->FindActionByName("set-multi-params");
  ASSERT_TRUE(status.ok());
  const auto action_info = status.ValueOrDie();
  auto action = table_entry_.mutable_action()->mutable_action();
  action->set_action_id(action_info.preamble().id());

  // First parameter has a 16-bit value.
  auto param = action->add_params();
  ASSERT_LE(3, action_info.params_size());
  param->set_param_id(action_info.params(0).id());
  param->set_value(EncodeByteValue(2, 0xab, 0xcd));

  // Second parameter has a 48-bit value.
  param = action->add_params();
  param->set_param_id(action_info.params(1).id());
  param->set_value(EncodeByteValue(6, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10));

  // Third parameter has an arbitrary byte value that doesn't get assigned
  // anywhere by the action descriptor.
  param = action->add_params();
  param->set_param_id(action_info.params(2).id());
  const std::string test_bytes_value("really-long-value");
  param->set_value(test_bytes_value);

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_LT(0, flow_entry.fields_size());
  EXPECT_TRUE(flow_entry.has_action());
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, flow_entry.action().type());
  EXPECT_TRUE(flow_entry.action().has_function());
  const auto& action_function = flow_entry.action().function();
  ASSERT_EQ(2, action_function.primitives_size());
  EXPECT_EQ(P4_FIELD_TYPE_IPV4_DST, action_function.modify_fields(0).type());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_DST, action_function.modify_fields(1).type());
  const uint32 kField0Value = 0xabcd;
  EXPECT_EQ(kField0Value, action_function.modify_fields(0).u32());
  const uint64 kField1Value = 0x605040302010ULL;
  EXPECT_EQ(kField1Value, action_function.modify_fields(1).u64());
  ASSERT_EQ(2, action_function.modify_fields_size());
  EXPECT_EQ(P4_ACTION_OP_CLONE, action_function.primitives(0).op_code());
  EXPECT_EQ(P4_ACTION_OP_DROP, action_function.primitives(1).op_code());
}

// Tests mapping of an action with constant value assignments of various widths.
TEST_F(P4TableMapperTest, TestTableActionConstantAssignment) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpTableActionTest();
  auto status = p4_info_manager_->FindActionByName("set-constant-value");
  ASSERT_TRUE(status.ok());
  const auto action_info = status.ValueOrDie();
  auto action = table_entry_.mutable_action()->mutable_action();
  action->set_action_id(action_info.preamble().id());

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_LT(0, flow_entry.fields_size());
  EXPECT_TRUE(flow_entry.has_action());
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, flow_entry.action().type());
  EXPECT_TRUE(flow_entry.action().has_function());
  const auto& action_function = flow_entry.action().function();
  EXPECT_EQ(0, action_function.primitives_size());
  ASSERT_EQ(3, action_function.modify_fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_IPV4_DST, action_function.modify_fields(0).type());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_DST, action_function.modify_fields(1).type());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_DST, action_function.modify_fields(2).type());
  const uint32 kExpectedValue32 = 0x1f002f00;
  EXPECT_EQ(kExpectedValue32, action_function.modify_fields(0).u32());
  EXPECT_EQ(kExpectedValue32, action_function.modify_fields(1).u32());
  const uint64 kExpectedValue64 = 0xba9876543210;
  EXPECT_EQ(kExpectedValue64, action_function.modify_fields(2).u64());
}

// Tests mapping of an action with a color-based drop decision.
TEST_F(P4TableMapperTest, TestTableActionDropNotGreen) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpTableActionTest();
  auto status = p4_info_manager_->FindActionByName("meter-not-green");
  ASSERT_TRUE(status.ok());
  const auto action_info = status.ValueOrDie();
  auto action = table_entry_.mutable_action()->mutable_action();
  action->set_action_id(action_info.preamble().id());

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_LT(0, flow_entry.fields_size());
  EXPECT_TRUE(flow_entry.has_action());
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, flow_entry.action().type());
  EXPECT_TRUE(flow_entry.action().has_function());
  const auto& action_function = flow_entry.action().function();
  EXPECT_EQ(0, action_function.modify_fields_size());
  EXPECT_EQ(2, action_function.primitives_size());
  std::set<P4ActionOp> meter_ops;

  for (const auto& primitive : action_function.primitives()) {
    meter_ops.insert(primitive.op_code());
    ASSERT_EQ(2, primitive.meter_colors_size());
    std::set<P4MeterColor> meter_colors;
    meter_colors.insert(primitive.meter_colors(0));
    meter_colors.insert(primitive.meter_colors(1));
    EXPECT_TRUE(meter_colors.find(P4_METER_YELLOW) != meter_colors.end());
    EXPECT_TRUE(meter_colors.find(P4_METER_RED) != meter_colors.end());
  }
  EXPECT_TRUE(meter_ops.find(P4_ACTION_OP_DROP) != meter_ops.end());
  EXPECT_TRUE(meter_ops.find(P4_ACTION_OP_CLONE) != meter_ops.end());
}

// Tests mapping of an action with a color-based header field assignment.
// TODO: Update this test when the mapping operation is supported.
TEST_F(P4TableMapperTest, TestTableActionAssignWhenGreen) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpTableActionTest();
  auto status = p4_info_manager_->FindActionByName("assign-when-green");
  ASSERT_TRUE(status.ok());
  const auto action_info = status.ValueOrDie();
  auto action = table_entry_.mutable_action()->mutable_action();
  action->set_action_id(action_info.preamble().id());

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_LT(0, flow_entry.fields_size());
  EXPECT_TRUE(flow_entry.has_action());
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, flow_entry.action().type());
}

// Tests mapping of an action profile with group type.
TEST_F(P4TableMapperTest, TestActionProfileGroup) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpActionProfileTest();

  // Group ID and members don't matter to the P4TableMapper.
  action_profile_group_.set_group_id(1);
  action_profile_group_.add_members()->set_member_id(1);
  action_profile_group_.add_members()->set_member_id(2);

  MappedAction mapped_action;
  auto map_status = p4_table_mapper_->MapActionProfileGroup(
      action_profile_group_, &mapped_action);
  EXPECT_OK(map_status);
  EXPECT_EQ(P4_ACTION_TYPE_PROFILE_GROUP_ID, mapped_action.type());
  EXPECT_EQ(MappedAction::ACTION_VALUE_NOT_SET,
            mapped_action.action_value_case());
}

// Tests mapping of an action profile with member type.
TEST_F(P4TableMapperTest, TestActionProfileMember) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpActionProfileTest();

  // Member ID doesn't matter to the P4TableMapper.
  // The test encodes "set-32" as the member's action.
  action_profile_member_.set_member_id(3);

  auto status = p4_info_manager_->FindActionByName("set-32");
  ASSERT_TRUE(status.ok());
  const auto action_info = status.ValueOrDie();
  auto action = action_profile_member_.mutable_action();
  action->set_action_id(action_info.preamble().id());
  auto param = action->add_params();
  ASSERT_LE(1, action_info.params_size());
  param->set_param_id(action_info.params(0).id());
  param->set_value(EncodeByteValue(4, 192, 168, 1, 1));

  MappedAction mapped_action;
  auto map_status = p4_table_mapper_->MapActionProfileMember(
      action_profile_member_, &mapped_action);
  EXPECT_OK(map_status);
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, mapped_action.type());
  EXPECT_EQ(MappedAction::kFunction, mapped_action.action_value_case());
}

// Tests mapping of internal match fields supplied by p4c in the table map.
TEST_F(P4TableMapperTest, TestInternalMatchField) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("test-internal-match-table");
  auto match_field = table_entry_.mutable_match(0);
  match_field->mutable_exact()->set_value(EncodeByteValue(4, 10, 2, 255, 4));

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);

  // The flow_entry should have 3 fields, 2 internal fields from the pipeline
  // spec plus the match_field from the table_entry_ request.  The internal
  // fields come first, followed by the input request match.
  ASSERT_EQ(3, flow_entry.fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_IPV6_SRC, flow_entry.fields(0).type());
  EXPECT_EQ("byte-value", flow_entry.fields(0).value().b());
  EXPECT_EQ("byte-mask", flow_entry.fields(0).mask().b());
  EXPECT_EQ(P4_FIELD_TYPE_VRF, flow_entry.fields(1).type());
  const uint32 kExpectedVrfU32 = 0xfffe;
  EXPECT_EQ(kExpectedVrfU32, flow_entry.fields(1).value().u32());
  EXPECT_EQ(P4_FIELD_TYPE_IPV4_DST, flow_entry.fields(2).type());
  const uint32 kExpectedU32 = 0x0a02ff04;
  EXPECT_EQ(kExpectedU32, flow_entry.fields(2).value().u32());
}

// Tests mapping of priority and metadata fields.
TEST_F(P4TableMapperTest, TestPriorityAndMetadataMapping) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("lpm-match-bytes-table");
  std::string byte_value("128bit-match-key");
  auto match_field = table_entry_.mutable_match(0);
  match_field->mutable_lpm()->set_value(byte_value);
  match_field->mutable_lpm()->set_prefix_len(5);
  table_entry_.set_priority(100);
  table_entry_.set_controller_metadata(0x0102030405060708ULL);

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_EQ(table_entry_.priority(), flow_entry.priority());
  EXPECT_EQ(table_entry_.controller_metadata(),
            flow_entry.controller_metadata());
}

// Tests table entry mapping with no previous config push.
TEST_F(P4TableMapperTest, TestTableMapNoConfig) {
  SetUpMatchFieldTest("lpm-match-bytes-table");
  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INTERNAL, map_status.error_code());
  EXPECT_THAT(map_status.error_message(),
              HasSubstr("TableEntry without valid P4 configuration"));
}

// Tests action profile mapping with no previous config push.
TEST_F(P4TableMapperTest, TestActionProfileMapNoConfig) {
  SetUpActionProfileTest();
  action_profile_group_.set_group_id(1);
  MappedAction mapped_action;
  auto map_status = p4_table_mapper_->MapActionProfileGroup(
      action_profile_group_, &mapped_action);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INTERNAL, map_status.error_code());
  EXPECT_THAT(map_status.error_message(),
              HasSubstr("ActionProfileGroup without valid P4 configuration"));
}

// Tests mapping of an invalid table ID.
TEST_F(P4TableMapperTest, TestTableMapBadTableID) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("exact-match-bytes-table");
  table_entry_.set_table_id(0xe0000);  // Bogus table ID.

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, map_status.error_code());
  EXPECT_THAT(map_status.error_message(), HasSubstr("not found"));
}

// Tests mapping of a request to change a table's default action.
TEST_F(P4TableMapperTest, TestTableMapNewDefaultAction) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("exact-match-bytes-table");
  table_entry_.clear_match();  // Missing match fields means new default action.

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_TRUE(flow_entry.has_action());
  EXPECT_EQ(0, flow_entry.fields_size());  // No fields = new default action.
  EXPECT_TRUE(flow_entry.has_table_info());
}

// Tests mapping of a request to change default action of a table with a const
// default action.
TEST_F(P4TableMapperTest, TestTableMapNewConstDefaultAction) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("test-const-action-table");
  table_entry_.clear_match();  // Missing match fields means new default action.

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(), HasSubstr("change default action"));
}

// Tests mapping of missing action for INSERT update.
TEST_F(P4TableMapperTest, TestTableMapNoActionInsert) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("exact-match-bytes-table");
  table_entry_.clear_action();

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(), HasSubstr("no action"));
}

// Tests mapping of MODIFY update without action.
TEST_F(P4TableMapperTest, TestTableMapNoActionModify) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("exact-match-bytes-table");
  table_entry_.clear_action();
  table_entry_.mutable_match(0)->mutable_exact()->set_value(EncodeByteValue(
      16, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::MODIFY, &flow_entry);
  EXPECT_TRUE(map_status.ok());
  EXPECT_LT(0, flow_entry.fields_size());
  EXPECT_FALSE(flow_entry.has_action());
}

// Tests mapping of DELETE update without action.
TEST_F(P4TableMapperTest, TestTableMapNoActionDelete) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("exact-match-bytes-table");
  table_entry_.clear_action();
  table_entry_.mutable_match(0)->mutable_exact()->set_value(EncodeByteValue(
      16, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::DELETE, &flow_entry);
  EXPECT_TRUE(map_status.ok());
  EXPECT_LT(0, flow_entry.fields_size());
  EXPECT_FALSE(flow_entry.has_action());
}

// Tests mapping of missing field ID.
TEST_F(P4TableMapperTest, TestTableMapMissingFieldID) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("exact-match-bytes-table");
  table_entry_.mutable_match(0)->set_field_id(0);

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(), HasSubstr("no field_id"));
}

// Tests mapping of field ID that doesn't belong to table.
TEST_F(P4TableMapperTest, TestTableMapFieldIDNotInTable) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("exact-match-bytes-table");
  table_entry_.mutable_match(0)->set_field_id(0xf0001);

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_OPER_NOT_SUPPORTED, map_status.error_code());
  EXPECT_THAT(map_status.error_message(), HasSubstr("not recognized in table"));
}

// Tests mapping of LPM field with missing value to default match.
TEST_F(P4TableMapperTest, TestTableMapLPMFieldMissingValue) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("lpm-match-bytes-table");

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  ASSERT_EQ(1, flow_entry.fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_DST, flow_entry.fields(0).type());
  EXPECT_FALSE(flow_entry.fields(0).has_value());
}

// Tests mapping of EXACT field with missing value.
TEST_F(P4TableMapperTest, TestTableMapExactFieldMissingValue) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("exact-match-32-table");

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(), HasSubstr(table_.preamble().name()));
  ASSERT_EQ(1, flow_entry.fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_UNKNOWN, flow_entry.fields(0).type());
  EXPECT_TRUE(ProtoEqual(table_entry_.match(0),
                         flow_entry.fields(0).value().raw_pi_match()));
}

// Tests mapping of field with wrong value encoding.
TEST_F(P4TableMapperTest, TestTableMapFieldEncodeError) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("lpm-match-bytes-table");
  auto match_field = table_entry_.mutable_match(0);
  match_field->mutable_exact()->set_value(  // Exact instead of LPM.
      EncodeByteValue(1, 127));
  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(), HasSubstr(table_.preamble().name()));
  ASSERT_EQ(1, flow_entry.fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_UNKNOWN, flow_entry.fields(0).type());
  EXPECT_TRUE(ProtoEqual(table_entry_.match(0),
                         flow_entry.fields(0).value().raw_pi_match()));
}

// Tests mapping of field with bad value.
TEST_F(P4TableMapperTest, TestTableMapFieldBadValue) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("lpm-match-32-table");
  auto match_field = table_entry_.mutable_match(0);
  match_field->mutable_lpm()->set_value(EncodeByteValue(1, 127));
  match_field->mutable_lpm()->set_prefix_len(125);  // Prefix is too big.

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(), HasSubstr(table_.preamble().name()));
  ASSERT_EQ(1, flow_entry.fields_size());
  EXPECT_EQ(P4_FIELD_TYPE_UNKNOWN, flow_entry.fields(0).type());
  EXPECT_TRUE(ProtoEqual(table_entry_.match(0),
                         flow_entry.fields(0).value().raw_pi_match()));
}

// Tests mapping of multiple field IDs in a match request.
TEST_F(P4TableMapperTest, TestTableMapMultipleFields) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMultiMatchFieldTest("test-multi-match-table");
  ASSERT_EQ(3, table_.match_fields_size());

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_EQ(3, flow_entry.fields_size());
}

// Tests mapping of duplicate field IDs in a request.
TEST_F(P4TableMapperTest, TestTableMapDuplicateFieldID) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMultiMatchFieldTest("test-multi-match-table");
  ASSERT_EQ(3, table_.match_fields_size());
  table_entry_.mutable_match(2)->set_field_id(table_entry_.match(0).field_id());

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(), HasSubstr("multiple match field"));
  EXPECT_THAT(map_status.error_message(), HasSubstr(table_.preamble().name()));
}

// Tests mapping of multiple field IDs with a don't-care LPM field.
TEST_F(P4TableMapperTest, TestTableMapMultipleFieldsDontCareLPM) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMultiMatchFieldTest("test-multi-match-table");
  ASSERT_EQ(3, table_.match_fields_size());

  // This test removes the LPM field from the tested table_entry_.
  const ::p4::v1::FieldMatch exact_field = table_entry_.match(1);
  const ::p4::v1::FieldMatch ternary_field = table_entry_.match(2);
  table_entry_.mutable_match()->Clear();
  *table_entry_.add_match() = exact_field;
  *table_entry_.add_match() = ternary_field;

  // All three fields should be present in the output flow entry.
  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_EQ(3, flow_entry.fields_size());
}

// Tests mapping of multiple field IDs with a don't-care ternary field.
TEST_F(P4TableMapperTest, TestTableMapMultipleFieldsDontCareTernary) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMultiMatchFieldTest("test-multi-match-table");
  ASSERT_EQ(3, table_.match_fields_size());

  // This test removes the ternary field from the tested table_entry_.
  const ::p4::v1::FieldMatch lpm_field = table_entry_.match(0);
  const ::p4::v1::FieldMatch exact_field = table_entry_.match(1);
  table_entry_.mutable_match()->Clear();
  *table_entry_.add_match() = lpm_field;
  *table_entry_.add_match() = exact_field;

  // All three fields should be present in the output flow entry.
  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_OK(map_status);
  EXPECT_EQ(3, flow_entry.fields_size());
}

// Tests mapping of multiple field IDs with a don't-care exact field.
TEST_F(P4TableMapperTest, TestTableMapMultipleFieldsDontCareExact) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMultiMatchFieldTest("test-multi-match-table");
  ASSERT_EQ(3, table_.match_fields_size());

  // This test removes the exact field from the tested table_entry_.
  const ::p4::v1::FieldMatch lpm_field = table_entry_.match(0);
  const ::p4::v1::FieldMatch ternary_field = table_entry_.match(2);
  table_entry_.mutable_match()->Clear();
  *table_entry_.add_match() = lpm_field;
  *table_entry_.add_match() = ternary_field;

  // This mapping should fail because exact match fields don't have defaults.
  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(),
              HasSubstr("P4 MatchType EXACT has no default value"));
}

// Tests mapping of an action with no encoded action function or profile IDs.
TEST_F(P4TableMapperTest, TestTableMissingActionData) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpTableActionTest();
  table_entry_.mutable_action();  // Marks the action present.

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(),
              HasSubstr("Unrecognized P4 TableEntry action type"));
}

// Tests mapping of missing action ID.
TEST_F(P4TableMapperTest, TestTableMapMissingActionID) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpTableActionTest();
  table_entry_.mutable_action()->mutable_action()->set_action_id(0);

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(), HasSubstr("no action_id"));
}

// Tests action profile mapping with an invalid profile ID.
TEST_F(P4TableMapperTest, TestActionProfileMapBadProfileID) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpActionProfileTest();
  action_profile_group_.set_group_id(1);
  action_profile_group_.set_action_profile_id(0xe0000);  // Bogus ID.

  MappedAction mapped_action;
  auto map_status = p4_table_mapper_->MapActionProfileGroup(
      action_profile_group_, &mapped_action);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, map_status.error_code());
  EXPECT_THAT(map_status.error_message(), HasSubstr("not found"));
}

// Tests action profile mapping with missing member action ID.
TEST_F(P4TableMapperTest, TestActionProfileMissingMemberActionID) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpActionProfileTest();
  auto action = action_profile_member_.mutable_action();
  action->set_action_id(0);

  MappedAction mapped_action;
  auto map_status = p4_table_mapper_->MapActionProfileMember(
      action_profile_member_, &mapped_action);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(), HasSubstr("action has no action_id"));
}

// Tests mapping of an action profile with member action not common to all
// tables sharing the profile..
TEST_F(P4TableMapperTest, TestActionProfileSharedTableError) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpActionProfileTest();
  action_profile_member_.set_member_id(3);

  // The "set-multi-params" action is not common to all tables sharing the
  // tested profile.
  auto status = p4_info_manager_->FindActionByName("set-multi-params");
  ASSERT_TRUE(status.ok());
  const auto action_info = status.ValueOrDie();
  auto action = action_profile_member_.mutable_action();
  action->set_action_id(action_info.preamble().id());

  MappedAction mapped_action;
  auto map_status = p4_table_mapper_->MapActionProfileMember(
      action_profile_member_, &mapped_action);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_OPER_NOT_SUPPORTED, map_status.error_code());
  EXPECT_THAT(map_status.error_message(),
              HasSubstr("not a recognized action for table"));
}

TEST_F(P4TableMapperTest, DeparsePacketInMetadataSuccess) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  MappedPacketMetadata mapped_packet_metadata;
  ::p4::v1::PacketMetadata p4_packet_metadata;

  // Deparse ingress port for a packet to be sent to the controller.
  mapped_packet_metadata.set_type(P4_FIELD_TYPE_INGRESS_PORT);
  mapped_packet_metadata.set_u32(4097);
  ASSERT_OK(p4_table_mapper_->DeparsePacketInMetadata(mapped_packet_metadata,
                                                      &p4_packet_metadata));
  EXPECT_EQ(1, p4_packet_metadata.metadata_id());
  EXPECT_EQ(std::string("\x0\x0\x10\x01", 4), p4_packet_metadata.value());

  // Deparse ingress trunk for a packet to be sent to the controller.
  mapped_packet_metadata.set_type(P4_FIELD_TYPE_INGRESS_TRUNK);
  mapped_packet_metadata.set_u32(4098);
  ASSERT_OK(p4_table_mapper_->DeparsePacketInMetadata(mapped_packet_metadata,
                                                      &p4_packet_metadata));
  EXPECT_EQ(2, p4_packet_metadata.metadata_id());
  EXPECT_EQ(std::string("\x0\x0\x10\x02", 4), p4_packet_metadata.value());

  // Deparse egress port for a packet to be sent to the controller.
  mapped_packet_metadata.set_type(P4_FIELD_TYPE_EGRESS_PORT);
  mapped_packet_metadata.set_u32(4099);
  ASSERT_OK(p4_table_mapper_->DeparsePacketInMetadata(mapped_packet_metadata,
                                                      &p4_packet_metadata));
  EXPECT_EQ(3, p4_packet_metadata.metadata_id());
  EXPECT_EQ(std::string("\x0\x0\x10\x03", 4), p4_packet_metadata.value());

  // Note that there is no way to deparse the unknown metadata in the set of
  // ingress metadata.
}

TEST_F(P4TableMapperTest, DeparsePacketInMetadataFailure) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  MappedPacketMetadata mapped_packet_metadata;
  ::p4::v1::PacketMetadata p4_packet_metadata;

  // Deparse an unknown metadata.
  mapped_packet_metadata.set_type(P4_FIELD_TYPE_VLAN_VID);
  mapped_packet_metadata.set_u32(4097);
  ::util::Status status = p4_table_mapper_->DeparsePacketInMetadata(
      mapped_packet_metadata, &p4_packet_metadata);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Don't know how to deparse"));

  // Invalid bitwidth.
  mapped_packet_metadata.set_type(P4_FIELD_TYPE_EGRESS_PORT);
  mapped_packet_metadata.set_u64(4097);
  status = p4_table_mapper_->DeparsePacketInMetadata(mapped_packet_metadata,
                                                     &p4_packet_metadata);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Incorrect bitwidth for a u64"));
}

TEST_F(P4TableMapperTest, DeparsePacketInMetadataDuplicateType) {
  // This test replaces the original pipeline config field type for one
  // metadata field to make it a duplicate of another field.
  P4PipelineConfig p4_pipeline_config;
  p4_pipeline_config.ParseFromString(
      forwarding_pipeline_config_.p4_device_config());
  auto iter = p4_pipeline_config.mutable_table_map()->find(
      "packet_in.unknown-type-metadata");
  ASSERT_TRUE(iter != p4_pipeline_config.mutable_table_map()->end());
  iter->second.mutable_field_descriptor()->set_type(P4_FIELD_TYPE_EGRESS_PORT);
  ASSERT_TRUE(p4_pipeline_config.SerializeToString(
      forwarding_pipeline_config_.mutable_p4_device_config()));
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));

  // The egress port type should deparse as the original field (id 3),
  // not the field with the duplicate type.
  MappedPacketMetadata mapped_packet_metadata;
  ::p4::v1::PacketMetadata p4_packet_metadata;
  mapped_packet_metadata.set_type(P4_FIELD_TYPE_EGRESS_PORT);
  mapped_packet_metadata.set_u32(0x1234);
  ASSERT_OK(p4_table_mapper_->DeparsePacketInMetadata(mapped_packet_metadata,
                                                      &p4_packet_metadata));
  EXPECT_EQ(3, p4_packet_metadata.metadata_id());
  EXPECT_EQ(std::string("\x0\x0\x12\x34", 4), p4_packet_metadata.value());
}

TEST_F(P4TableMapperTest, ParsePacketOutMetadataSuccess) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  MappedPacketMetadata mapped_packet_metadata;
  ::p4::v1::PacketMetadata p4_packet_metadata;

  // Parse egress port from a packet received from controller.
  p4_packet_metadata.set_metadata_id(1);
  p4_packet_metadata.set_value("\x10\x03");
  ASSERT_OK(p4_table_mapper_->ParsePacketOutMetadata(p4_packet_metadata,
                                                     &mapped_packet_metadata));
  EXPECT_EQ(P4_FIELD_TYPE_EGRESS_PORT, mapped_packet_metadata.type());
  EXPECT_EQ(4099, mapped_packet_metadata.u64());

  // Parse a metadata with unknown type from a packet received from controller.
  p4_packet_metadata.set_metadata_id(2);
  p4_packet_metadata.set_value("\x10\x04");
  ASSERT_OK(p4_table_mapper_->ParsePacketOutMetadata(p4_packet_metadata,
                                                     &mapped_packet_metadata));
  EXPECT_EQ(P4_FIELD_TYPE_ANNOTATED, mapped_packet_metadata.type());
  EXPECT_EQ(4100, mapped_packet_metadata.u32());
}

TEST_F(P4TableMapperTest, ParsePacketOutMetadataFailure) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  MappedPacketMetadata mapped_packet_metadata;
  ::p4::v1::PacketMetadata p4_packet_metadata;

  // Unknown metadata ID
  p4_packet_metadata.set_metadata_id(100);
  p4_packet_metadata.set_value("\x10\x03");
  ::util::Status status = p4_table_mapper_->ParsePacketOutMetadata(
      p4_packet_metadata, &mapped_packet_metadata);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Don't know how to parse"));
}

TEST_F(P4TableMapperTest, DeparsePacketOutMetadataSuccess) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  MappedPacketMetadata mapped_packet_metadata;
  ::p4::v1::PacketMetadata p4_packet_metadata;

  // Deparse egress port for a packet to be sent to the switch.
  mapped_packet_metadata.set_type(P4_FIELD_TYPE_EGRESS_PORT);
  mapped_packet_metadata.set_u64(4099);
  ASSERT_OK(p4_table_mapper_->DeparsePacketOutMetadata(mapped_packet_metadata,
                                                       &p4_packet_metadata));
  EXPECT_EQ(1, p4_packet_metadata.metadata_id());
  EXPECT_EQ(std::string("\x0\x0\x0\x0\x0\x0\x10\x03", 8),
            p4_packet_metadata.value());

  // Note that there is no way to deparse the unknown metadata in the set of
  // egress metadata.
}

TEST_F(P4TableMapperTest, DeparsePacketOutMetadataFailure) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  MappedPacketMetadata mapped_packet_metadata;
  ::p4::v1::PacketMetadata p4_packet_metadata;

  // Deparse an unknown metadata.
  mapped_packet_metadata.set_type(P4_FIELD_TYPE_VLAN_VID);
  mapped_packet_metadata.set_u32(4097);
  ::util::Status status = p4_table_mapper_->DeparsePacketOutMetadata(
      mapped_packet_metadata, &p4_packet_metadata);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Don't know how to deparse"));

  // Invalid bitwidth.
  mapped_packet_metadata.set_type(P4_FIELD_TYPE_EGRESS_PORT);
  mapped_packet_metadata.set_u32(4097);
  status = p4_table_mapper_->DeparsePacketOutMetadata(mapped_packet_metadata,
                                                      &p4_packet_metadata);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Incorrect bitwidth for a u32"));
}

TEST_F(P4TableMapperTest, ParsePacketInMetadataSuccess) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  MappedPacketMetadata mapped_packet_metadata;
  ::p4::v1::PacketMetadata p4_packet_metadata;

  // Parse ingress port from a packet received from switch.
  p4_packet_metadata.set_metadata_id(1);
  p4_packet_metadata.set_value("\x10\x01");
  ASSERT_OK(p4_table_mapper_->ParsePacketInMetadata(p4_packet_metadata,
                                                    &mapped_packet_metadata));
  EXPECT_EQ(P4_FIELD_TYPE_INGRESS_PORT, mapped_packet_metadata.type());
  EXPECT_EQ(4097, mapped_packet_metadata.u32());

  // Parse ingress trunk from a packet received from switch.
  p4_packet_metadata.set_metadata_id(2);
  p4_packet_metadata.set_value("\x10\x02");
  ASSERT_OK(p4_table_mapper_->ParsePacketInMetadata(p4_packet_metadata,
                                                    &mapped_packet_metadata));
  EXPECT_EQ(P4_FIELD_TYPE_INGRESS_TRUNK, mapped_packet_metadata.type());
  EXPECT_EQ(4098, mapped_packet_metadata.u32());

  // Parse egress port from a packet received from switch.
  p4_packet_metadata.set_metadata_id(3);
  p4_packet_metadata.set_value("\x10\x03");
  ASSERT_OK(p4_table_mapper_->ParsePacketInMetadata(p4_packet_metadata,
                                                    &mapped_packet_metadata));
  EXPECT_EQ(P4_FIELD_TYPE_EGRESS_PORT, mapped_packet_metadata.type());
  EXPECT_EQ(4099, mapped_packet_metadata.u32());

  // Parse a metadata with unknown type from a packet received from switch.
  p4_packet_metadata.set_metadata_id(4);
  p4_packet_metadata.set_value("\x10\x04");
  ASSERT_OK(p4_table_mapper_->ParsePacketInMetadata(p4_packet_metadata,
                                                    &mapped_packet_metadata));
  EXPECT_EQ(P4_FIELD_TYPE_ANNOTATED, mapped_packet_metadata.type());
  EXPECT_EQ(4100, mapped_packet_metadata.u32());
}

TEST_F(P4TableMapperTest, ParsePacketInMetadataFailure) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  MappedPacketMetadata mapped_packet_metadata;
  ::p4::v1::PacketMetadata p4_packet_metadata;

  // Unknown metadata ID
  p4_packet_metadata.set_metadata_id(100);
  p4_packet_metadata.set_value("\x10\x03");
  ::util::Status status = p4_table_mapper_->ParsePacketInMetadata(
      p4_packet_metadata, &mapped_packet_metadata);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Don't know how to parse"));
}

// Tests mapping of a table & field id to a match field type.
TEST_F(P4TableMapperTest, TestMapMatchField) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("exact-match-32-table");

  MappedField expected_mapped_field;
  expected_mapped_field.set_type(P4_FIELD_TYPE_IPV4_DST);
  expected_mapped_field.set_bit_width(32);
  expected_mapped_field.set_bit_offset(128);
  expected_mapped_field.set_header_type(P4_HEADER_IPV4);

  const ::p4::v1::FieldMatch& field_match = table_entry_.match(0);
  MappedField mapped_field;
  EXPECT_OK(p4_table_mapper_->MapMatchField(
      table_entry_.table_id(), field_match.field_id(), &mapped_field));
  EXPECT_THAT(mapped_field, EqualsProto(expected_mapped_field));
}

// Tests mapping of a table & field id to a missing match field type results in
// a graceful error.
TEST_F(P4TableMapperTest, TestMapMatchFieldFailure) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  MappedField mapped_field;
  ::util::Status status = p4_table_mapper_->MapMatchField(0, 0, &mapped_field);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Unrecognized field id"));
}

// Tests lookup for a table by ID.
TEST_F(P4TableMapperTest, TestLookupTable) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  ASSERT_NO_FATAL_FAILURE(SetUpTableID("exact-match-32-table"));
  ::p4::config::v1::Table table;
  ASSERT_OK(p4_table_mapper_->LookupTable(table_.preamble().id(), &table));
  EXPECT_THAT(table, EqualsProto(table_));
}

// Tests lookup for a table by ID fails for an unknown table.
TEST_F(P4TableMapperTest, TestLookupTableFailure) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  ::p4::config::v1::Table table;
  ::util::Status lookup_status = p4_table_mapper_->LookupTable(0x999, &table);
  EXPECT_FALSE(lookup_status.ok());
  EXPECT_THAT(lookup_status.error_message(), HasSubstr("0x999"));
}

// Tests mapping of hidden static table update for expected failure.
TEST_F(P4TableMapperTest, TestHiddenStaticTableUpdateFails) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("test-hidden-static-table");

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(),
              HasSubstr("test-hidden-static-table with static entries"));
}

// Tests mapping of hidden static table update succeeds after
// EnableStaticTableUpdates.
TEST_F(P4TableMapperTest, TestHiddenTableUpdateFailsStaticEnabled) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("test-hidden-static-table");
  p4_table_mapper_->EnableStaticTableUpdates();

  CommonFlowEntry flow_entry;
  EXPECT_OK(p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry));
}

// Tests expected failure of static table update.
TEST_F(P4TableMapperTest, TestStaticTableUpdateFails) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("test-static-table");

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(),
              HasSubstr("P4 table test-static-table with static entries"));
}

// Tests expected success of static table update after EnableStaticTableUpdates.
TEST_F(P4TableMapperTest, TestStaticTableUpdateOKAfterEnable) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("test-static-table");
  p4_table_mapper_->EnableStaticTableUpdates();

  CommonFlowEntry flow_entry;
  EXPECT_OK(p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry));
}

// Tests expected failure of static table update after Enable/Disable sequence.
TEST_F(P4TableMapperTest, TestStaticTableUpdateFailsEnableDisable) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("test-static-table");
  p4_table_mapper_->EnableStaticTableUpdates();
  p4_table_mapper_->DisableStaticTableUpdates();

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
}

// Tests normal HandlePrePushStaticEntryChanges behavior.
TEST_F(P4TableMapperTest, TestPrePushStaticEntryChanges) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  EXPECT_CALL(*static_entry_mapper_mock_, HandlePrePushChanges(_, _))
      .WillOnce(Return(::util::OkStatus()));
  ::p4::v1::WriteRequest dummy_static_config;
  ::p4::v1::WriteRequest dummy_out;
  EXPECT_OK(p4_table_mapper_->HandlePrePushStaticEntryChanges(
      dummy_static_config, &dummy_out));
}

// Tests HandlePrePushStaticEntryChanges behavior when called before any
// pipeline config push.  This needs to work before the initial push.
TEST_F(P4TableMapperTest, TestPrePushStaticEntryChangesNoPipeline) {
  EXPECT_CALL(*static_entry_mapper_mock_, HandlePrePushChanges(_, _))
      .WillOnce(Return(::util::OkStatus()));
  ::p4::v1::WriteRequest dummy_static_config;
  ::p4::v1::WriteRequest dummy_out;
  EXPECT_OK(p4_table_mapper_->HandlePrePushStaticEntryChanges(
      dummy_static_config, &dummy_out));
}

// Tests HandlePrePushStaticEntryChanges behavior when the P4StaticEntryMapper
// encounters an error.
TEST_F(P4TableMapperTest, TestPrePushStaticEntryChangesError) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  const std::string kErrorMsg = "static-entry-error";
  EXPECT_CALL(*static_entry_mapper_mock_, HandlePrePushChanges(_, _))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));
  ::p4::v1::WriteRequest dummy_static_config;
  ::p4::v1::WriteRequest dummy_out;
  ::util::Status status = p4_table_mapper_->HandlePrePushStaticEntryChanges(
      dummy_static_config, &dummy_out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kErrorMsg));
}

// Tests normal HandlePostPushStaticEntryChanges behavior.
TEST_F(P4TableMapperTest, TestPostPushStaticEntryChanges) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  EXPECT_CALL(*static_entry_mapper_mock_, HandlePostPushChanges(_, _))
      .WillOnce(Return(::util::OkStatus()));
  ::p4::v1::WriteRequest dummy_static_config;
  ::p4::v1::WriteRequest dummy_out;
  EXPECT_OK(p4_table_mapper_->HandlePostPushStaticEntryChanges(
      dummy_static_config, &dummy_out));
}

// Tests HandlePostPushStaticEntryChanges behavior when called before any
// pipeline config push.  As the name implies, this should fail.
TEST_F(P4TableMapperTest, TestPostPushStaticEntryChangesNoPipeline) {
  EXPECT_CALL(*static_entry_mapper_mock_, HandlePostPushChanges(_, _)).Times(0);
  ::p4::v1::WriteRequest dummy_static_config;
  ::p4::v1::WriteRequest dummy_out;
  ::util::Status status = p4_table_mapper_->HandlePostPushStaticEntryChanges(
      dummy_static_config, &dummy_out);
  EXPECT_FALSE(status.ok());
}

// Tests HandlePostPushStaticEntryChanges behavior when the P4StaticEntryMapper
// encounters an error.
TEST_F(P4TableMapperTest, TestPostPushStaticEntryChangesError) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  const std::string kErrorMsg = "static-entry-error";
  EXPECT_CALL(*static_entry_mapper_mock_, HandlePostPushChanges(_, _))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg)));
  ::p4::v1::WriteRequest dummy_static_config;
  ::p4::v1::WriteRequest dummy_out;
  ::util::Status status = p4_table_mapper_->HandlePostPushStaticEntryChanges(
      dummy_static_config, &dummy_out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr(kErrorMsg));
}

// Tests expected failures of hidden non-static table update.
TEST_F(P4TableMapperTest, TestHiddenNonStaticTableUpdateFails) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  SetUpMatchFieldTest("test-hidden-non-static-table");

  CommonFlowEntry flow_entry;
  auto map_status = p4_table_mapper_->MapFlowEntry(
      table_entry_, ::p4::v1::Update::INSERT, &flow_entry);
  EXPECT_FALSE(map_status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, map_status.error_code());
  EXPECT_THAT(map_status.error_message(),
              HasSubstr("Updates to hidden P4 table test-hidden-non-static"));

  // Failure is also expected after EnableStaticTableUpdates.
  p4_table_mapper_->EnableStaticTableUpdates();
  EXPECT_FALSE(
      p4_table_mapper_
          ->MapFlowEntry(table_entry_, ::p4::v1::Update::INSERT, &flow_entry)
          .ok());
}

// Tests hidden stage status of normal P4 table.
TEST_F(P4TableMapperTest, TestNormalTableNotHidden) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  ASSERT_NO_FATAL_FAILURE(SetUpTableID("exact-match-32-table"));
  auto hidden_state =
      p4_table_mapper_->IsTableStageHidden(table_.preamble().id());
  EXPECT_EQ(TRI_STATE_FALSE, hidden_state);
}

// Tests hidden status of P4 table in the HIDDEN pipeline stage.
TEST_F(P4TableMapperTest, TestHiddenTable) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  ASSERT_NO_FATAL_FAILURE(SetUpTableID("test-hidden-static-table"));
  auto hidden_state =
      p4_table_mapper_->IsTableStageHidden(table_.preamble().id());
  EXPECT_EQ(TRI_STATE_TRUE, hidden_state);
}

// Tests hidden status of an unknown P4 table ID.
TEST_F(P4TableMapperTest, TestHiddenTableUnknownID) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  auto hidden_state = p4_table_mapper_->IsTableStageHidden(0x13579bdf);
  EXPECT_EQ(TRI_STATE_UNKNOWN, hidden_state);
}

// Tests hidden table status with an action ID input instead of table ID.
TEST_F(P4TableMapperTest, TestHiddenTableActionID) {
  ASSERT_OK(p4_table_mapper_->PushForwardingPipelineConfig(
      forwarding_pipeline_config_));
  auto status = p4_info_manager_->FindActionByName("nop");
  ASSERT_TRUE(status.ok());
  const auto& action_info = status.ValueOrDie();
  auto hidden_state =
      p4_table_mapper_->IsTableStageHidden(action_info.preamble().id());
  EXPECT_EQ(TRI_STATE_UNKNOWN, hidden_state);
}

}  // namespace hal
}  // namespace stratum
