// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file contains TableTypeMapper unit tests.

#include "stratum/p4c_backends/fpm/table_type_mapper.h"

#include <memory>
#include <string>

#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"

namespace stratum {
namespace p4c_backends {

class ParserFieldMapperTest : public testing::Test {
 protected:
  static constexpr const char* kL2McastTableName =
      "ingress.l2_multicast.vlan_broadcast_table";
  static constexpr const char* kL2MyStationTableName =
      "ingress.l3_admit_tor.l3_admit_tor_table";
  static constexpr const char* kL2McastActionName =
      "ingress.l2_multicast.vlan_broadcast";
  static constexpr const char* kL2MyStationActionName =
      "ingress.l3_admit_tor.set_l3_admit_tor";
  static constexpr const char* kL2McastFieldName =
      "standard_metadata.mcast_grp";

  // Reads the P4Info and P4PipelineConfig from their respective text files,
  // then creates the test_p4_info_manager_ with the P4Info.
  void SetUpTestInputs(const std::string& p4_info_file,
                       const std::string& p4_pipeline_config_file) {
    const std::string kBasePath =
        "stratum/p4c_backends/fpm/testdata/";
    std::string full_path = kBasePath + p4_info_file;
    CHECK_OK(ReadProtoFromTextFile(full_path, &test_p4_info_));
    full_path = kBasePath + p4_pipeline_config_file;
    CHECK_OK(ReadProtoFromTextFile(full_path, &test_p4_pipeline_config_));
    test_p4_info_manager_ =
        absl::make_unique<hal::P4InfoManager>(test_p4_info_);
    CHECK_OK(test_p4_info_manager_->InitializeAndVerify());
  }

  // These members are created and/or populated by SetUpTestInputs.
  hal::P4PipelineConfig test_p4_pipeline_config_;
  ::p4::config::v1::P4Info test_p4_info_;
  std::unique_ptr<hal::P4InfoManager> test_p4_info_manager_;
};

// Tests normal expected output for the tables in the test files.
TEST_F(ParserFieldMapperTest, TestL2TableTypes) {
  SetUpTestInputs("table_type_mapper_p4info.pb.txt",
                  "table_type_mapper_p4pipeline.pb.txt");
  TableTypeMapper test_table_type_mapper;
  test_table_type_mapper.ProcessTables(
      *test_p4_info_manager_, &test_p4_pipeline_config_);

  const hal::P4TableDescriptor& table1_descriptor = FindTableDescriptorOrDie(
      kL2McastTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_L2_MULTICAST, table1_descriptor.type());
  const hal::P4TableDescriptor& table2_descriptor = FindTableDescriptorOrDie(
      kL2MyStationTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_L2_MY_STATION, table2_descriptor.type());
}

// This test makes sure TableTypeMapper does not overwrite an already known
// table type in its input.
TEST_F(ParserFieldMapperTest, TestTableTypeAlreadyKnown) {
  SetUpTestInputs("table_type_mapper_p4info.pb.txt",
                  "table_type_mapper_p4pipeline.pb.txt");
  hal::P4TableDescriptor* table1_descriptor = FindMutableTableDescriptorOrDie(
      kL2McastTableName, &test_p4_pipeline_config_);
  table1_descriptor->set_type(P4_TABLE_L3_IP);
  TableTypeMapper test_table_type_mapper;
  test_table_type_mapper.ProcessTables(
      *test_p4_info_manager_, &test_p4_pipeline_config_);

  EXPECT_EQ(P4_TABLE_L3_IP, table1_descriptor->type());
  const hal::P4TableDescriptor& table2_descriptor = FindTableDescriptorOrDie(
      kL2MyStationTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_L2_MY_STATION, table2_descriptor.type());
}

// This test erases the field descriptor for the L2-multicast group so it is
// not found when the TableTypeMapper runs.
TEST_F(ParserFieldMapperTest, TestTableTypeMissingFieldDescriptor) {
  SetUpTestInputs("table_type_mapper_p4info.pb.txt",
                  "table_type_mapper_p4pipeline.pb.txt");
  test_p4_pipeline_config_.mutable_table_map()->erase(kL2McastFieldName);
  TableTypeMapper test_table_type_mapper;
  test_table_type_mapper.ProcessTables(
      *test_p4_info_manager_, &test_p4_pipeline_config_);

  const hal::P4TableDescriptor& table1_descriptor = FindTableDescriptorOrDie(
      kL2McastTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_UNKNOWN, table1_descriptor.type());
  const hal::P4TableDescriptor& table2_descriptor = FindTableDescriptorOrDie(
      kL2MyStationTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_L2_MY_STATION, table2_descriptor.type());
}

// This test changes the multicast group field type so it is not the type that
// leads to the L2 multicast table type.
TEST_F(ParserFieldMapperTest, TestTableTypeNonMcastFieldType) {
  SetUpTestInputs("table_type_mapper_p4info.pb.txt",
                  "table_type_mapper_p4pipeline.pb.txt");
  hal::P4FieldDescriptor* field_descriptor = FindMutableFieldDescriptorOrNull(
      kL2McastFieldName, &test_p4_pipeline_config_);
  field_descriptor->set_type(P4_FIELD_TYPE_VRF);
  TableTypeMapper test_table_type_mapper;
  test_table_type_mapper.ProcessTables(
      *test_p4_info_manager_, &test_p4_pipeline_config_);

  const hal::P4TableDescriptor& table1_descriptor = FindTableDescriptorOrDie(
      kL2McastTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_UNKNOWN, table1_descriptor.type());
  const hal::P4TableDescriptor& table2_descriptor = FindTableDescriptorOrDie(
      kL2MyStationTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_L2_MY_STATION, table2_descriptor.type());
}

// This test adds a conflicting action assignment to cause the TableTypeMapper
// to fail to determine the table type.
TEST_F(ParserFieldMapperTest, TestTableTypeConflictInSameAction) {
  // This test copies the assignment from the my-station table into the action
  // for the L2-mcast table, creating a single action that tries to reference
  // both tables.
  SetUpTestInputs("table_type_mapper_p4info.pb.txt",
                  "table_type_mapper_p4pipeline.pb.txt");
  hal::P4ActionDescriptor* action1 = FindMutableActionDescriptorOrDie(
      kL2McastActionName, &test_p4_pipeline_config_);
  const hal::P4ActionDescriptor& action2 = FindActionDescriptorOrDie(
      kL2MyStationActionName, test_p4_pipeline_config_);
  ASSERT_LE(1, action2.assignments_size());
  *action1->add_assignments() = action2.assignments(0);
  TableTypeMapper test_table_type_mapper;
  test_table_type_mapper.ProcessTables(
      *test_p4_info_manager_, &test_p4_pipeline_config_);

  const hal::P4TableDescriptor& table1_descriptor = FindTableDescriptorOrDie(
      kL2McastTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_UNKNOWN, table1_descriptor.type());
  const hal::P4TableDescriptor& table2_descriptor = FindTableDescriptorOrDie(
      kL2MyStationTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_L2_MY_STATION, table2_descriptor.type());
}

// This test also creates conflicting action assignments.  It sets up the
// same conflict as the previous test, but then it copies the original
// assignment into a third assignment to make sure the original conflict
// is not lost.
TEST_F(ParserFieldMapperTest, TestTableTypeConflictInSameAction2) {
  SetUpTestInputs("table_type_mapper_p4info.pb.txt",
                  "table_type_mapper_p4pipeline.pb.txt");
  hal::P4ActionDescriptor* action1 = FindMutableActionDescriptorOrDie(
      kL2McastActionName, &test_p4_pipeline_config_);
  ASSERT_LE(1, action1->assignments_size());
  const hal::P4ActionDescriptor& action2 = FindActionDescriptorOrDie(
      kL2MyStationActionName, test_p4_pipeline_config_);
  ASSERT_LE(1, action2.assignments_size());
  *action1->add_assignments() = action2.assignments(0);
  *action1->add_assignments() = action1->assignments(0);
  TableTypeMapper test_table_type_mapper;
  test_table_type_mapper.ProcessTables(
      *test_p4_info_manager_, &test_p4_pipeline_config_);

  const hal::P4TableDescriptor& table1_descriptor = FindTableDescriptorOrDie(
      kL2McastTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_UNKNOWN, table1_descriptor.type());
  const hal::P4TableDescriptor& table2_descriptor = FindTableDescriptorOrDie(
      kL2MyStationTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_L2_MY_STATION, table2_descriptor.type());
}

// This test also creates conflicting action assignments, but the conflicting
// assignments are spread over multiple actions for the same table.  The test
// copies the assignment from the my-station table into the previously empty
// "TestAction", which both tables reference.
TEST_F(ParserFieldMapperTest, TestTableTypeConflictInDifferentActions) {
  SetUpTestInputs("table_type_mapper_p4info.pb.txt",
                  "table_type_mapper_p4pipeline.pb.txt");
  hal::P4ActionDescriptor* action1 = FindMutableActionDescriptorOrDie(
      "TestAction", &test_p4_pipeline_config_);
  const hal::P4ActionDescriptor& action2 = FindActionDescriptorOrDie(
      kL2MyStationActionName, test_p4_pipeline_config_);
  ASSERT_LE(1, action2.assignments_size());
  *action1->add_assignments() = action2.assignments(0);
  TableTypeMapper test_table_type_mapper;
  test_table_type_mapper.ProcessTables(
      *test_p4_info_manager_, &test_p4_pipeline_config_);

  const hal::P4TableDescriptor& table1_descriptor = FindTableDescriptorOrDie(
      kL2McastTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_UNKNOWN, table1_descriptor.type());
  const hal::P4TableDescriptor& table2_descriptor = FindTableDescriptorOrDie(
      kL2MyStationTableName, test_p4_pipeline_config_);
  EXPECT_EQ(P4_TABLE_L2_MY_STATION, table2_descriptor.type());
}

}  // namespace p4c_backends
}  // namespace stratum
