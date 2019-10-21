// Copyright 2019 Google LLC
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

// This file contains AnnotationMapper unit tests.

#include "stratum/p4c_backends/fpm/annotation_mapper.h"

#include <set>
#include <vector>

#include "gflags/gflags.h"
#include "google/protobuf/util/message_differencer.h"
#include "stratum/hal/lib/p4/common_flow_entry.pb.h"
#include "stratum/hal/lib/p4/p4_info_manager_mock.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/lib/macros.h"
#include "stratum/glue/status/status.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

DECLARE_string(p4c_annotation_map_files);

using ::google::protobuf::util::MessageDifferencer;
using ::testing::Return;
using ::testing::_;

namespace stratum {
namespace p4c_backends {

class AnnotationMapperTest : public testing::Test {
 protected:
  void SetUp() override {
    // By default, mock_p4_info_.GetSwitchStackAnnotations returns an
    // empty P4Annotation message.
    ON_CALL(mock_p4_info_, GetSwitchStackAnnotations(_))
        .WillByDefault(Return(default_mock_annotations_));
  }

  void SetUpAnnotationFileList() {
    const std::string kFilePath = "stratum/p4c_backends/"
        "fpm/testdata/";
    FLAGS_p4c_annotation_map_files =
        kFilePath + "annotation_string_map.pb.txt";
    FLAGS_p4c_annotation_map_files += ",";
    FLAGS_p4c_annotation_map_files +=
        kFilePath + "object_name_map.pb.txt";
  }

  // Uses a separate AnnotationMapper to get a copy of the P4AnnotationMap
  // that gets initialized from the test files.
  void GetAnnotationMap(P4AnnotationMap* test_map) {
    SetUpAnnotationFileList();
    AnnotationMapper test_file_mapper;
    CHECK(test_file_mapper.Init());
    *test_map = test_file_mapper.annotation_map();
  }

  // Sets up table descriptors and mock_p4_info_ expectations as if all pipeline
  // stages are in use, except the stages in the idle_stages input set.
  void SetUpActiveStages(std::set<P4Annotation::PipelineStage> idle_stages) {
    for (int stage = P4Annotation::PipelineStage_MIN;  // NOLINT
         stage <= P4Annotation::PipelineStage_MAX; ++stage) {
      if (P4Annotation::PipelineStage_IsValid(stage)) {
        // This loop intentionally includes a DEFAULT_STAGE annotation; the
        // stage enum name acts as the unique table name.
        auto pipeline_stage = static_cast<P4Annotation::PipelineStage>(stage);
        if (idle_stages.find(pipeline_stage) != idle_stages.end()) continue;
        const std::string kTableName =
            P4Annotation::PipelineStage_Name(pipeline_stage);
        hal::P4TableMapValue map_value;
        map_value.mutable_table_descriptor()->set_type(P4_TABLE_UNKNOWN);
        (*test_pipeline_cfg_.mutable_table_map())[kTableName] = map_value;
        test_annotations_.set_pipeline_stage(pipeline_stage);
        EXPECT_CALL(mock_p4_info_, GetSwitchStackAnnotations(kTableName))
            .WillOnce(Return(test_annotations_));
      }
    }
  }

  AnnotationMapper mapper_;
  hal::P4InfoManagerMock mock_p4_info_;
  hal::P4PipelineConfig test_pipeline_cfg_;

  // The default_mock_annotations_ member contains an empty P4Annotation
  // message that supplies the mock_p4_info_.GetSwitchStackAnnotations default
  // return value.  The test_annotations_ member is for tests to setup
  // test-dependent P4Annotation message expectations.
  const P4Annotation default_mock_annotations_;
  P4Annotation test_annotations_;
};

TEST_F(AnnotationMapperTest, TestInitNoFiles) {
  FLAGS_p4c_annotation_map_files = "";
  EXPECT_TRUE(mapper_.Init());
}

TEST_F(AnnotationMapperTest, TestInitWithFiles) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());
}

TEST_F(AnnotationMapperTest, TestInitMissingFile) {
  FLAGS_p4c_annotation_map_files = "this-file-does-not-exist";
  EXPECT_FALSE(mapper_.Init());
}

TEST_F(AnnotationMapperTest, TestInitTwice) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());
  EXPECT_FALSE(mapper_.Init());
}

TEST_F(AnnotationMapperTest, TestInitFromMap) {
  P4AnnotationMap test_map;
  const std::string kActionAddendaName = "test-action-addenda";
  P4ActionAnnotationValue map_value;
  map_value.add_addenda_names(kActionAddendaName);
  (*test_map.mutable_action_addenda_map())["action-annotation"] = map_value;
  auto action_addendum = test_map.add_action_addenda();
  action_addendum->set_name(kActionAddendaName);
  EXPECT_TRUE(mapper_.InitFromP4AnnotationMap(test_map));
}

TEST_F(AnnotationMapperTest, TestInitFromMapTwice) {
  P4AnnotationMap empty_map;  // An empty map is OK for this test.
  EXPECT_TRUE(mapper_.InitFromP4AnnotationMap(empty_map));
  EXPECT_FALSE(mapper_.InitFromP4AnnotationMap(empty_map));
}

TEST_F(AnnotationMapperTest, TestInitThenInitFromMap) {
  SetUpAnnotationFileList();
  P4AnnotationMap empty_map;  // An empty map is OK for this test.
  EXPECT_TRUE(mapper_.Init());
  EXPECT_FALSE(mapper_.InitFromP4AnnotationMap(empty_map));
}

TEST_F(AnnotationMapperTest, TestInitFromMapThenInit) {
  SetUpAnnotationFileList();
  P4AnnotationMap empty_map;  // An empty map is OK for this test.
  EXPECT_TRUE(mapper_.InitFromP4AnnotationMap(empty_map));
  EXPECT_FALSE(mapper_.Init());
}

TEST_F(AnnotationMapperTest, TestInitMissingAddendaName) {
  P4AnnotationMap test_map;
  const std::string kActionAddendaName = "test-action-addenda";
  P4ActionAnnotationValue map_value;
  map_value.add_addenda_names(kActionAddendaName);
  (*test_map.mutable_action_addenda_map())["action-annotation"] = map_value;
  auto action_addendum = test_map.add_action_addenda();
  action_addendum->add_device_data();  // No action_addendum->set_name is done.
  EXPECT_FALSE(mapper_.InitFromP4AnnotationMap(test_map));
}

TEST_F(AnnotationMapperTest, TestInitDuplicateAddendaName) {
  P4AnnotationMap test_map;
  const std::string kActionAddendaName = "test-action-addenda";
  P4ActionAnnotationValue map_value;
  map_value.add_addenda_names(kActionAddendaName);
  (*test_map.mutable_action_addenda_map())["action-annotation"] = map_value;
  auto action_addendum = test_map.add_action_addenda();
  action_addendum->set_name(kActionAddendaName);
  auto action_addendum2 = test_map.add_action_addenda();
  action_addendum2->set_name(kActionAddendaName);
  EXPECT_FALSE(mapper_.InitFromP4AnnotationMap(test_map));
}

// In this test, test_pipeline_cfg_ is empty.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsNop) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());
  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
}

// In this test, test_pipeline_cfg_ has a field descriptor that gets its
// type set according to the field name annotation.  The tested field is
// in the test annotation files.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsMapFieldType) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());

  const std::string kFieldName = "match-field-name-with-type";
  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_field_descriptor()->set_type(P4_FIELD_TYPE_ANNOTATED);
  (*test_pipeline_cfg_.mutable_table_map())[kFieldName] = table_map_value;
  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  const auto iter = test_pipeline_cfg_.table_map().find(kFieldName);
  ASSERT_TRUE(iter != test_pipeline_cfg_.table_map().end());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_SRC, iter->second.field_descriptor().type());
}

// In this test, test_pipeline_cfg_ has a field descriptor that has no
// annotation mapping.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsUnmappedField) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());
  const std::string kFieldName = "match-field-unmapped";

  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_field_descriptor()->set_type(P4_FIELD_TYPE_IPV4_DST);
  (*test_pipeline_cfg_.mutable_table_map())[kFieldName] = table_map_value;
  hal::P4PipelineConfig orig_pipeline_cfg = test_pipeline_cfg_;
  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  test_pipeline_cfg_.clear_idle_pipeline_stages();
  EXPECT_TRUE(MessageDifferencer::Equals(
      orig_pipeline_cfg, test_pipeline_cfg_));
}

// In this test, the field name has an annotation map entry, but an improperly
// formed annotation map is missing the field addenda.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsUndefinedFieldAddenda) {
  P4AnnotationMap test_map;
  GetAnnotationMap(&test_map);
  P4FieldAnnotationValue field_map_value;
  field_map_value.add_addenda_names("missing-field-addenda");
  const std::string kFieldWithMissingAddenda = "new-test-field";
  (*test_map.mutable_field_addenda_map())[kFieldWithMissingAddenda] =
      field_map_value;
  EXPECT_TRUE(mapper_.InitFromP4AnnotationMap(test_map));

  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_field_descriptor()->set_type(P4_FIELD_TYPE_ANNOTATED);
  (*test_pipeline_cfg_.mutable_table_map())[kFieldWithMissingAddenda] =
      table_map_value;
  EXPECT_FALSE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
}

// This test makes sure that AnnotationMapper doesn't gag on an unimplemented
// action descriptor mapping.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsActionDescriptor) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());

  const std::string kActionName = "action-annotation-1";
  const std::string kActionParameterName = "action-parameter-name-1";
  const std::string kActionDestinationFieldName = "action-destination-name-1";
  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_action_descriptor()->set_type(
      P4_ACTION_TYPE_FUNCTION);
  auto assignment = table_map_value.mutable_action_descriptor()->add_assignments();
  assignment->mutable_assigned_value()->set_parameter_name(kActionParameterName);
  assignment->set_destination_field_name(kActionDestinationFieldName);
  (*test_pipeline_cfg_.mutable_table_map())[kActionName] = table_map_value;

  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  const auto iter = test_pipeline_cfg_.table_map().find(kActionName);
  ASSERT_TRUE(iter != test_pipeline_cfg_.table_map().end());
  EXPECT_EQ(P4_ACTION_TYPE_PROFILE_GROUP_ID, iter->second.action_descriptor().type());

  // Check that the original assignment has been replaced
  EXPECT_EQ(iter->second.action_descriptor().assignments().size(), 1);
  auto as = iter->second.action_descriptor().assignments(0);
  EXPECT_EQ(as.destination_field_name(), "fake-destination-field-name");
  EXPECT_EQ(as.assigned_value().constant_param(), 1);
}

// In this test, test_pipeline_cfg_ has a table descriptor that gets its
// type set according to the table name annotation. The tested table is
// in the test annotation files. The mock_p4_info_ returns an empty
// P4Annotation message.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsMapTableType) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());

  const std::string kTableName = "table-name-with-type";
  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_table_descriptor()->set_type(P4_TABLE_UNKNOWN);
  (*test_pipeline_cfg_.mutable_table_map())[kTableName] = table_map_value;
  test_annotations_.set_pipeline_stage(P4Annotation::L3_LPM);
  EXPECT_CALL(mock_p4_info_, GetSwitchStackAnnotations(kTableName)).Times(1);

  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  const auto iter = test_pipeline_cfg_.table_map().find(kTableName);
  ASSERT_TRUE(iter != test_pipeline_cfg_.table_map().end());
  EXPECT_EQ(P4_TABLE_L3_IP, iter->second.table_descriptor().type());
  EXPECT_EQ(P4Annotation::DEFAULT_STAGE,
            iter->second.table_descriptor().pipeline_stage());
}

// In this test, the table's mock_p4_info_ returns a P4Annotation message
// that specifies the pipeline stage.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsTablePipelineStage) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());

  const std::string kTableName = "table-name-with-type";
  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_table_descriptor()->set_type(P4_TABLE_UNKNOWN);
  (*test_pipeline_cfg_.mutable_table_map())[kTableName] = table_map_value;
  test_annotations_.set_pipeline_stage(P4Annotation::L3_LPM);
  EXPECT_CALL(mock_p4_info_, GetSwitchStackAnnotations(kTableName))
      .WillOnce(Return(test_annotations_));

  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  const auto iter = test_pipeline_cfg_.table_map().find(kTableName);
  ASSERT_TRUE(iter != test_pipeline_cfg_.table_map().end());
  EXPECT_EQ(P4_TABLE_L3_IP, iter->second.table_descriptor().type());
  EXPECT_EQ(test_annotations_.pipeline_stage(),
            iter->second.table_descriptor().pipeline_stage());
}

// In this test, the table's mock_p4_info_ returns a GetSwitchStackAnnotations
// status error.
TEST_F(AnnotationMapperTest, TestProcessSwitchStackAnnotationsError) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());

  const std::string kTableName = "table-name-with-type";
  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_table_descriptor()->set_type(P4_TABLE_UNKNOWN);
  (*test_pipeline_cfg_.mutable_table_map())[kTableName] = table_map_value;
  test_annotations_.set_pipeline_stage(P4Annotation::L3_LPM);
  ::util::Status mock_status = MAKE_ERROR(ERR_INVALID_P4_INFO) << "Mock error";
  EXPECT_CALL(mock_p4_info_, GetSwitchStackAnnotations(kTableName))
      .WillOnce(Return(mock_status));

  EXPECT_FALSE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
}

// In this test, test_pipeline_cfg_ has a table descriptor that gets custom
// device_data from the table name annotation.  The tested table is
// in the test annotation files.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsMapTableDeviceData) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());

  const std::string kTableName = "table-name-with-addenda";
  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_table_descriptor()->set_type(P4_TABLE_UNKNOWN);
  (*test_pipeline_cfg_.mutable_table_map())[kTableName] = table_map_value;
  EXPECT_CALL(mock_p4_info_, GetSwitchStackAnnotations(kTableName)).Times(1);

  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  const auto iter = test_pipeline_cfg_.table_map().find(kTableName);
  ASSERT_TRUE(iter != test_pipeline_cfg_.table_map().end());
  const auto& annotated_descriptor = iter->second.table_descriptor();
  EXPECT_EQ(P4_TABLE_UNKNOWN, annotated_descriptor.type());
  ASSERT_EQ(1, annotated_descriptor.device_data_size());
  EXPECT_FALSE(annotated_descriptor.device_data(0).name().empty());
  EXPECT_FALSE(annotated_descriptor.device_data(0).data().empty());
}

// In this test, test_pipeline_cfg_ has a table descriptor that gets multiple
// sets of device_data from the table name annotation.  The tested table is
// in the test annotation files.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsMapTableMultiDeviceData) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());

  const std::string kTableName = "table-name-with-type-and-multiple-addenda";
  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_table_descriptor()->set_type(P4_TABLE_UNKNOWN);
  (*test_pipeline_cfg_.mutable_table_map())[kTableName] = table_map_value;
  EXPECT_CALL(mock_p4_info_, GetSwitchStackAnnotations(kTableName)).Times(1);

  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  const auto iter = test_pipeline_cfg_.table_map().find(kTableName);
  ASSERT_TRUE(iter != test_pipeline_cfg_.table_map().end());
  const auto& annotated_descriptor = iter->second.table_descriptor();
  EXPECT_EQ(P4_TABLE_L3_IP, annotated_descriptor.type());
  ASSERT_EQ(2, annotated_descriptor.device_data_size());
  EXPECT_FALSE(annotated_descriptor.device_data(0).name().empty());
  EXPECT_FALSE(annotated_descriptor.device_data(0).data().empty());
  EXPECT_FALSE(annotated_descriptor.device_data(1).name().empty());
  EXPECT_FALSE(annotated_descriptor.device_data(1).data().empty());
  EXPECT_NE(annotated_descriptor.device_data(0).name(),
            annotated_descriptor.device_data(1).name());
  EXPECT_NE(annotated_descriptor.device_data(0).data(),
            annotated_descriptor.device_data(1).data());
}

// In this test, test_pipeline_cfg_ has a table descriptor that has no
// annotation mapping.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsUnmappedTable) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());
  const std::string kTableName = "table-unmapped";

  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_table_descriptor()->set_type(P4_TABLE_UNKNOWN);
  (*test_pipeline_cfg_.mutable_table_map())[kTableName] = table_map_value;
  hal::P4PipelineConfig orig_pipeline_cfg = test_pipeline_cfg_;
  EXPECT_CALL(mock_p4_info_, GetSwitchStackAnnotations(kTableName)).Times(1);

  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  test_pipeline_cfg_.clear_idle_pipeline_stages();
  EXPECT_TRUE(MessageDifferencer::Equals(
      orig_pipeline_cfg, test_pipeline_cfg_));
}

// In this test, the table name has an annotation map entry, but an improperly
// formed annotation map is missing the table addenda.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsUndefinedTableAddenda) {
  P4AnnotationMap test_map;
  GetAnnotationMap(&test_map);
  P4TableAnnotationValue table_annotation_value;
  table_annotation_value.add_addenda_names("missing-table-addenda");
  const std::string kTableWithMissingAddenda = "new-test-table";
  (*test_map.mutable_table_addenda_map())[kTableWithMissingAddenda] =
      table_annotation_value;
  EXPECT_TRUE(mapper_.InitFromP4AnnotationMap(test_map));

  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_table_descriptor()->set_type(P4_TABLE_UNKNOWN);
  (*test_pipeline_cfg_.mutable_table_map())[kTableWithMissingAddenda] =
      table_map_value;
  EXPECT_CALL(mock_p4_info_,
              GetSwitchStackAnnotations(kTableWithMissingAddenda))
      .Times(1);
  EXPECT_FALSE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
}

// In this test, the table name has an annotation map entry with addenda that
// provide some internal match fields.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsTableInternalMatch) {
  // This test creates its own custom annotation map data for a table
  // with two internal match fields.
  P4AnnotationMap test_map;
  GetAnnotationMap(&test_map);
  P4TableAnnotationValue table_annotation_value;
  table_annotation_value.set_type(P4_TABLE_L3_IP);
  const std::string kMatchFieldAddendaName = "match-addenda";
  table_annotation_value.add_addenda_names(kMatchFieldAddendaName);
  const std::string kTestTableName = "table-with-match-fields";
  (*test_map.mutable_table_addenda_map())[kTestTableName] =
      table_annotation_value;
  hal::MappedField match1;
  match1.set_type(P4_FIELD_TYPE_VRF);
  match1.mutable_value()->set_u32(0xfffe);
  match1.mutable_mask()->set_u32(0xffff);
  hal::MappedField match2;
  match2.set_type(P4_FIELD_TYPE_IPV6_SRC);
  match2.mutable_value()->set_b("test-ipv6-src");
  P4TableAddenda table_match_addenda;
  table_match_addenda.set_name(kMatchFieldAddendaName);
  *table_match_addenda.add_internal_match_fields() = match1;
  *table_match_addenda.add_internal_match_fields() = match2;
  *test_map.add_table_addenda() = table_match_addenda;
  EXPECT_TRUE(mapper_.InitFromP4AnnotationMap(test_map));

  // Annotations are ready, with a descriptor for the new table in
  // test_pipeline_cfg_, ProcessAnnotations should handle it.
  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_table_descriptor()->set_type(P4_TABLE_UNKNOWN);
  (*test_pipeline_cfg_.mutable_table_map())[kTestTableName] = table_map_value;
  EXPECT_CALL(mock_p4_info_, GetSwitchStackAnnotations(kTestTableName))
      .Times(1);

  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  const auto iter = test_pipeline_cfg_.table_map().find(kTestTableName);
  ASSERT_TRUE(iter != test_pipeline_cfg_.table_map().end());
  const auto& annotated_descriptor = iter->second.table_descriptor();
  EXPECT_EQ(P4_TABLE_L3_IP, annotated_descriptor.type());
  ASSERT_EQ(2, annotated_descriptor.internal_match_fields_size());
  EXPECT_TRUE(MessageDifferencer::Equals(
      match1, annotated_descriptor.internal_match_fields(0)));
  EXPECT_TRUE(MessageDifferencer::Equals(
      match2, annotated_descriptor.internal_match_fields(1)));
}

// This test makes sure that AnnotationMapper doesn't gag on a malformed
// table map entry.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsNoDescriptor) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());

  const std::string kTableName = "table-name-1";
  hal::P4TableMapValue empty_table_map_value;
  (*test_pipeline_cfg_.mutable_table_map())[kTableName] = empty_table_map_value;
  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
}

TEST_F(AnnotationMapperTest, TestProcessAnnotationsNoInit) {
  EXPECT_FALSE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
}

// In this test, test_pipeline_cfg_ has a field descriptor whose annotation
// refers to field addenda that aren't implemented.
TEST_F(AnnotationMapperTest, TestProcessAnnotationsUnimplementedFieldAddenda) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());

  const std::string kFieldName = "match-field-name-with-multiple-addenda";
  hal::P4TableMapValue table_map_value;
  table_map_value.mutable_field_descriptor()->set_type(P4_FIELD_TYPE_ANNOTATED);
  (*test_pipeline_cfg_.mutable_table_map())[kFieldName] = table_map_value;
  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  const auto iter = test_pipeline_cfg_.table_map().find(kFieldName);
  ASSERT_TRUE(iter != test_pipeline_cfg_.table_map().end());
  EXPECT_TRUE(MessageDifferencer::Equals(
      table_map_value.field_descriptor(), iter->second.field_descriptor()));
}

// In this test, the pipeline config and mock_p4_info_ are set up as if every
// pipeline stage is in use.
TEST_F(AnnotationMapperTest, TestAllPipelineStagesInUse) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());
  SetUpActiveStages({});
  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  EXPECT_EQ(0, test_pipeline_cfg_.idle_pipeline_stages_size());
}

// In this test, the pipeline config and mock_p4_info_ are set up as if the
// L2 pipeline stage is unused.
TEST_F(AnnotationMapperTest, TestIdlePipelineStage) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());
  SetUpActiveStages({P4Annotation::L2});
  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  ASSERT_EQ(1, test_pipeline_cfg_.idle_pipeline_stages_size());
  EXPECT_EQ(P4Annotation::L2, test_pipeline_cfg_.idle_pipeline_stages(0));
}

// In this test, the pipeline config and mock_p4_info_ are set up to treat the
// DEFAULT_STAGE as unused, but it should never appear in idle_pipeline_stages.
TEST_F(AnnotationMapperTest, TestNoIdleDefaultPipelineStage) {
  SetUpAnnotationFileList();
  EXPECT_TRUE(mapper_.Init());
  SetUpActiveStages({P4Annotation::DEFAULT_STAGE});
  EXPECT_TRUE(mapper_.ProcessAnnotations(mock_p4_info_, &test_pipeline_cfg_));
  EXPECT_EQ(0, test_pipeline_cfg_.idle_pipeline_stages_size());
}

}  // namespace p4c_backends
}  // namespace stratum
