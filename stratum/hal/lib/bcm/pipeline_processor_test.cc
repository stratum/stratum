#include "platforms/networking/hercules/hal/lib/bcm/pipeline_processor.h"

#include "platforms/networking/hercules/lib/test_utils/matchers.h"
#include "platforms/networking/hercules/lib/test_utils/p4_proto_builders.h"
#include "platforms/networking/hercules/public/lib/error.h"
#include "platforms/networking/hercules/public/proto/p4_annotation.pb.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"

namespace google {
namespace hercules {
namespace hal {
namespace bcm {
namespace {

using test_utils::EqualsProto;
using test_utils::p4_proto_builders::ApplyTable;
using test_utils::p4_proto_builders::HitBuilder;
using test_utils::p4_proto_builders::IsValidBuilder;
using test_utils::p4_proto_builders::Table;
using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

// This case tests that an empty control pipeline produces an empty pipeline.
TEST(PipelineProcessorTest, EmptyPipeline) {
  // Test a 4-table sequential pipeline.
  P4ControlBlock block;

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  EXPECT_TRUE(pipeline.empty());
}

// This is the base case. Multiple tables are applied in sequence. Only Apply
// control statements are used.
TEST(PipelineProcessorTest, SequentialApply) {
  // Test a 4-table sequential pipeline.
  P4ControlBlock block;
  *block.add_statements()->mutable_apply() = Table(1);
  *block.add_statements()->mutable_apply() = Table(2);
  *block.add_statements()->mutable_apply() = Table(3);
  *block.add_statements()->mutable_apply() = Table(4);
  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect 4 separate physical tables.
  ASSERT_EQ(pipeline.size(), 4);
  ASSERT_EQ(pipeline[0].size(), 1);
  ASSERT_EQ(pipeline[1].size(), 1);
  ASSERT_EQ(pipeline[2].size(), 1);
  ASSERT_EQ(pipeline[3].size(), 1);

  // We expect the tables to be in sequential order. The same order as the
  // input.
  EXPECT_THAT(pipeline[0][0].table, EqualsProto(Table(1)));
  EXPECT_THAT(pipeline[1][0].table, EqualsProto(Table(2)));
  EXPECT_THAT(pipeline[2][0].table, EqualsProto(Table(3)));
  EXPECT_THAT(pipeline[3][0].table, EqualsProto(Table(4)));

  // We expect the table priorities to increase in order.
  EXPECT_GT(pipeline[0][0].priority, 0);
  EXPECT_LT(pipeline[0][0].priority, pipeline[1][0].priority);
  EXPECT_LT(pipeline[1][0].priority, pipeline[2][0].priority);
  EXPECT_LT(pipeline[2][0].priority, pipeline[3][0].priority);

  // No conditions were placed on these tables.
  EXPECT_TRUE(pipeline[0][0].valid_conditions.empty());
  EXPECT_TRUE(pipeline[1][0].valid_conditions.empty());
  EXPECT_TRUE(pipeline[2][0].valid_conditions.empty());
  EXPECT_TRUE(pipeline[3][0].valid_conditions.empty());
}

// This case tests perfect nesting of tables. The resulting pipeline should be
// one physical table.
TEST(PipelineProcessorTest, NestedApply) {
  P4ControlBlock block;
  *block.add_statements()->mutable_apply() = Table(1);
  *block.add_statements() = HitBuilder()
                                .OnMiss(Table(1))
                                .Do(ApplyTable(2))
                                .Do(HitBuilder()
                                        .OnMiss(Table(2))
                                        .Do(ApplyTable(3))
                                        .Do(HitBuilder()
                                                .OnMiss(Table(3))
                                                .UseFalse()
                                                .Do(ApplyTable(4))
                                                .Build())
                                        .Build())
                                .Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect one physical table with all four tables.
  ASSERT_EQ(pipeline.size(), 1);
  ASSERT_EQ(pipeline[0].size(), 4);

  // Move the tables into a more easily accessible structure.
  absl::flat_hash_map<int, PipelineTable> pipeline_tables;
  for (const auto& pipeline_table : pipeline[0]) {
    pipeline_tables.emplace(pipeline_table.table.table_id(), pipeline_table);
  }

  // Table priorities should be in outer-to-inner order:
  // Table1 > Table2 > Table3 > Table4.
  EXPECT_LT(0, pipeline_tables.at(4).priority);
  EXPECT_LT(pipeline_tables.at(4).priority, pipeline_tables.at(3).priority);
  EXPECT_LT(pipeline_tables.at(3).priority, pipeline_tables.at(2).priority);
  EXPECT_LT(pipeline_tables.at(2).priority, pipeline_tables.at(1).priority);

  // No conditions were placed on these tables.
  EXPECT_TRUE(pipeline_tables.at(1).valid_conditions.empty());
  EXPECT_TRUE(pipeline_tables.at(2).valid_conditions.empty());
  EXPECT_TRUE(pipeline_tables.at(3).valid_conditions.empty());
  EXPECT_TRUE(pipeline_tables.at(4).valid_conditions.empty());
}

// This case tests perfect nesting of tables done at the top level. The
// resulting pipeline should be one physical table.
TEST(PipelineProcessorTest, TopLevelNestedApply) {
  P4ControlBlock block;
  *block.add_statements()->mutable_apply() = Table(1);
  *block.add_statements() =
      HitBuilder().OnMiss(Table(1)).Do(ApplyTable(2)).Build();
  *block.add_statements() =
      HitBuilder().OnMiss(Table(2)).Do(ApplyTable(3)).Build();
  *block.add_statements() =
      HitBuilder().OnMiss(Table(3)).Do(ApplyTable(4)).Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect one physical table with all four tables.
  ASSERT_EQ(pipeline.size(), 1);
  ASSERT_EQ(pipeline[0].size(), 4);

  // Move the tables into a more easily accessible structure.
  absl::flat_hash_map<int, PipelineTable> pipeline_tables;
  for (const auto& pipeline_table : pipeline[0]) {
    pipeline_tables.emplace(pipeline_table.table.table_id(), pipeline_table);
  }

  // Table priorities should be in outer-to-inner order:
  // Table1 > Table2 > Table3 > Table4.
  EXPECT_LT(0, pipeline_tables.at(4).priority);
  EXPECT_LT(pipeline_tables.at(4).priority, pipeline_tables.at(3).priority);
  EXPECT_LT(pipeline_tables.at(3).priority, pipeline_tables.at(2).priority);
  EXPECT_LT(pipeline_tables.at(2).priority, pipeline_tables.at(1).priority);

  // No conditions were placed on these tables.
  EXPECT_TRUE(pipeline_tables.at(1).valid_conditions.empty());
  EXPECT_TRUE(pipeline_tables.at(2).valid_conditions.empty());
  EXPECT_TRUE(pipeline_tables.at(3).valid_conditions.empty());
  EXPECT_TRUE(pipeline_tables.at(4).valid_conditions.empty());
}

// This case tests a IsValid conditions. The resulting tables should be tagged
// with the condition.
TEST(PipelineProcessorTest, IsValidCondition) {
  P4ControlBlock block;
  *block.add_statements() = IsValidBuilder()
                                .Header(P4HeaderType::P4_HEADER_IPV4)
                                .DoIfValid(ApplyTable(1))
                                .DoIfInvalid(ApplyTable(2))
                                .Build();
  *block.add_statements() = IsValidBuilder()
                                .Header(P4HeaderType::P4_HEADER_IPV6)
                                .UseNot()
                                .DoIfValid(ApplyTable(3))
                                .DoIfInvalid(ApplyTable(4))
                                .Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect 4 independent physical tables.
  ASSERT_EQ(pipeline.size(), 4);
  ASSERT_EQ(pipeline[0].size(), 1);
  ASSERT_EQ(pipeline[1].size(), 1);
  ASSERT_EQ(pipeline[2].size(), 1);
  ASSERT_EQ(pipeline[3].size(), 1);

  // Move the tables into a more easily accessible structure.
  absl::flat_hash_map<int, PipelineTable> pipeline_tables;
  for (const auto& physical_table : pipeline) {
    pipeline_tables.emplace(physical_table[0].table.table_id(),
                            physical_table[0]);
  }

  // Tables 1 & 2 are mutually exclusive, as are tables 3 & 4. Priority ordering
  // only matters between the two groups.
  EXPECT_LT(0, pipeline_tables.at(1).priority);
  EXPECT_LT(0, pipeline_tables.at(2).priority);
  EXPECT_LT(pipeline_tables.at(1).priority, pipeline_tables.at(3).priority);
  EXPECT_LT(pipeline_tables.at(2).priority, pipeline_tables.at(3).priority);
  EXPECT_LT(pipeline_tables.at(1).priority, pipeline_tables.at(4).priority);
  EXPECT_LT(pipeline_tables.at(2).priority, pipeline_tables.at(4).priority);

  // Check the conditions on these tables.
  EXPECT_THAT(
      pipeline_tables.at(1).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_IPV4, true)));
  EXPECT_THAT(pipeline_tables.at(2).valid_conditions,
              UnorderedElementsAre(
                  std::make_pair(P4HeaderType::P4_HEADER_IPV4, false)));
  EXPECT_THAT(
      pipeline_tables.at(3).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_IPV6, true)));
  EXPECT_THAT(pipeline_tables.at(4).valid_conditions,
              UnorderedElementsAre(
                  std::make_pair(P4HeaderType::P4_HEADER_IPV6, false)));
}

// This case tests nested IsValid conditions. The resulting tables should be
// tagged with all conditions up the nest.
TEST(PipelineProcessorTest, NestedIsValidCondition) {
  P4ControlBlock block;
  *block.add_statements() =
      IsValidBuilder()
          .Header(P4HeaderType::P4_HEADER_PACKET_IN)
          .DoIfValid(ApplyTable(1))
          .DoIfInvalid(IsValidBuilder()
                           .Header(P4HeaderType::P4_HEADER_IPV6)
                           .DoIfValid(IsValidBuilder()
                                          .Header(P4HeaderType::P4_HEADER_UDP)
                                          .DoIfValid(ApplyTable(2))
                                          .Build())
                           .DoIfInvalid(ApplyTable(3))
                           .Build())
          .Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect 3 independent physical tables.
  ASSERT_EQ(pipeline.size(), 3);
  ASSERT_EQ(pipeline[0].size(), 1);
  ASSERT_EQ(pipeline[1].size(), 1);
  ASSERT_EQ(pipeline[2].size(), 1);

  // Move the tables into a more easily accessible structure.
  absl::flat_hash_map<int, PipelineTable> pipeline_tables;
  for (const auto& physical_table : pipeline) {
    pipeline_tables.emplace(physical_table[0].table.table_id(),
                            physical_table[0]);
  }

  // All the tables are mutually exclusive, so there is no priority requirement.

  // Check the conditions on these tables.
  EXPECT_THAT(pipeline_tables.at(1).valid_conditions,
              UnorderedElementsAre(
                  std::make_pair(P4HeaderType::P4_HEADER_PACKET_IN, true)));
  EXPECT_THAT(pipeline_tables.at(2).valid_conditions,
              UnorderedElementsAre(
                  std::make_pair(P4HeaderType::P4_HEADER_PACKET_IN, false),
                  std::make_pair(P4HeaderType::P4_HEADER_IPV6, true),
                  std::make_pair(P4HeaderType::P4_HEADER_UDP, true)));
  EXPECT_THAT(pipeline_tables.at(3).valid_conditions,
              UnorderedElementsAre(
                  std::make_pair(P4HeaderType::P4_HEADER_PACKET_IN, false),
                  std::make_pair(P4HeaderType::P4_HEADER_IPV6, false)));
}

// This case tests nested conflicting IsValid conditions. When a conflict is
// found, the conflicting nest is dropped.
TEST(PipelineProcessorTest, ConflictingIsValidCondition) {
  P4ControlBlock block;
  *block.add_statements() =
      IsValidBuilder()
          .Header(P4HeaderType::P4_HEADER_IPV4)
          .DoIfValid(ApplyTable(1))
          .DoIfInvalid(IsValidBuilder()
                           .Header(P4HeaderType::P4_HEADER_IPV4)
                           .DoIfValid(ApplyTable(2))
                           .DoIfInvalid(ApplyTable(3))
                           .Build())
          .Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect 2 independent physical table: 1 and 3.
  ASSERT_EQ(pipeline.size(), 2);
  ASSERT_EQ(pipeline[0].size(), 1);
  ASSERT_EQ(pipeline[1].size(), 1);

  // Move the tables into a more easily accessible structure.
  absl::flat_hash_map<int, PipelineTable> pipeline_tables;
  for (const auto& physical_table : pipeline) {
    pipeline_tables.emplace(physical_table[0].table.table_id(),
                            physical_table[0]);
  }

  // Check the conditions on these tables.
  EXPECT_THAT(
      pipeline_tables.at(1).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_IPV4, true)));
  EXPECT_THAT(pipeline_tables.at(3).valid_conditions,
              UnorderedElementsAre(
                  std::make_pair(P4HeaderType::P4_HEADER_IPV4, false)));
}

// This case tests conflicting IsValid conditions between a child table and its
// parent. When a conflict is found, the child table is dropped.
TEST(PipelineProcessorTest, ConflictingParentalIsValidCondition) {
  P4ControlBlock block;
  *block.add_statements() = IsValidBuilder()
                                .Header(P4HeaderType::P4_HEADER_IPV4)
                                .DoIfValid(ApplyTable(1))
                                .Build();
  *block.add_statements() =
      IsValidBuilder()
          .Header(P4HeaderType::P4_HEADER_IPV4)
          .DoIfInvalid(HitBuilder().OnMiss(Table(1)).Do(ApplyTable(2)).Build())
          .Build();
  *block.add_statements() = HitBuilder()
                                .OnMiss(Table(1))
                                .Do(IsValidBuilder()
                                        .Header(P4_HEADER_IPV4)
                                        .DoIfInvalid(ApplyTable(3))
                                        .Build())
                                .Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect only Table1.
  ASSERT_EQ(pipeline.size(), 1);
  ASSERT_EQ(pipeline[0].size(), 1);
  EXPECT_EQ(pipeline[0][0].table.table_id(), 1);
}

// This case tests collapsing nested IsValid conditions. The resulting tables
// should be tagged with all conditions up the nest but without redundant
// conditions.
TEST(PipelineProcessorTest, CollapsingIsValidCondition) {
  P4ControlBlock block;
  *block.add_statements() =
      IsValidBuilder()
          .Header(P4HeaderType::P4_HEADER_IPV4)
          .DoIfValid(ApplyTable(1))
          .DoIfInvalid(
              IsValidBuilder()
                  .Header(P4HeaderType::P4_HEADER_IPV6)
                  .DoIfValid(
                      IsValidBuilder()
                          .Header(P4HeaderType::P4_HEADER_UDP)
                          .DoIfValid(ApplyTable(2))
                          .DoIfInvalid(IsValidBuilder()
                                           .Header(P4HeaderType::P4_HEADER_TCP)
                                           .DoIfValid(ApplyTable(3))
                                           .Build())
                          .Build())
                  .DoIfInvalid(ApplyTable(4))
                  .Build())
          .Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect 4 independent physical tables.
  ASSERT_EQ(pipeline.size(), 4);
  ASSERT_EQ(pipeline[0].size(), 1);
  ASSERT_EQ(pipeline[1].size(), 1);
  ASSERT_EQ(pipeline[2].size(), 1);
  ASSERT_EQ(pipeline[3].size(), 1);

  // Move the tables into a more easily accessible structure.
  absl::flat_hash_map<int, PipelineTable> pipeline_tables;
  for (const auto& physical_table : pipeline) {
    pipeline_tables.emplace(physical_table[0].table.table_id(),
                            physical_table[0]);
  }

  // All the tables are mutually exclusive, so there is no priority requirement.

  // Check the conditions on these tables.
  EXPECT_THAT(
      pipeline_tables.at(1).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_IPV4, true)));
  EXPECT_THAT(
      pipeline_tables.at(2).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_IPV6, true),
                           std::make_pair(P4HeaderType::P4_HEADER_UDP, true)));
  EXPECT_THAT(
      pipeline_tables.at(3).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_IPV6, true),
                           std::make_pair(P4HeaderType::P4_HEADER_TCP, true)));
  EXPECT_THAT(pipeline_tables.at(4).valid_conditions,
              UnorderedElementsAre(
                  std::make_pair(P4HeaderType::P4_HEADER_IPV4, false),
                  std::make_pair(P4HeaderType::P4_HEADER_IPV6, false)));
}

// This case tests conflicts between L3 IsValid conditions. Tables with
// conflicting L3 IsValid conditions should be dropped.
TEST(PipelineProcessorTest, ConflictingL3IsValidCondition) {
  P4ControlBlock block;
  *block.add_statements() =
      IsValidBuilder()
          .Header(P4HeaderType::P4_HEADER_IPV4)
          .DoIfValid(IsValidBuilder()
                         .Header(P4HeaderType::P4_HEADER_IPV6)
                         .DoIfValid(ApplyTable(1))
                         .DoIfInvalid(ApplyTable(2))
                         .Build())
          .DoIfValid(IsValidBuilder()
                         .Header(P4HeaderType::P4_HEADER_ARP)
                         .DoIfValid(ApplyTable(3))
                         .DoIfInvalid(ApplyTable(4))
                         .Build())
          .Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect 2 independent physical tables.
  ASSERT_EQ(pipeline.size(), 2);
  ASSERT_EQ(pipeline[0].size(), 1);
  ASSERT_EQ(pipeline[1].size(), 1);

  // Move the tables into a more easily accessible structure.
  absl::flat_hash_map<int, PipelineTable> pipeline_tables;
  for (const auto& physical_table : pipeline) {
    pipeline_tables.emplace(physical_table[0].table.table_id(),
                            physical_table[0]);
  }

  // All the tables are mutually exclusive, so there is no priority requirement.

  // Check the conditions on these tables.
  EXPECT_THAT(
      pipeline_tables.at(2).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_IPV4, true)));
  EXPECT_THAT(
      pipeline_tables.at(4).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_IPV4, true)));
}

// This case tests conflicts between TCP/UDP IsValid conditions. Tables with
// conflicting TCP/UDP IsValid conditions should be dropped.
TEST(PipelineProcessorTest, ConflictingTcpUdpIsValidCondition) {
  P4ControlBlock block;
  *block.add_statements() =
      IsValidBuilder()
          .Header(P4HeaderType::P4_HEADER_TCP)
          .DoIfValid(IsValidBuilder()
                         .Header(P4HeaderType::P4_HEADER_UDP)
                         .DoIfValid(ApplyTable(1))
                         .DoIfInvalid(ApplyTable(2))
                         .Build())
          .DoIfValid(IsValidBuilder()
                         .Header(P4HeaderType::P4_HEADER_UDP_PAYLOAD)
                         .DoIfValid(ApplyTable(3))
                         .DoIfInvalid(ApplyTable(4))
                         .Build())
          .Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect 2 independent physical tables.
  ASSERT_EQ(pipeline.size(), 2);
  ASSERT_EQ(pipeline[0].size(), 1);
  ASSERT_EQ(pipeline[1].size(), 1);

  // Move the tables into a more easily accessible structure.
  absl::flat_hash_map<int, PipelineTable> pipeline_tables;
  for (const auto& physical_table : pipeline) {
    pipeline_tables.emplace(physical_table[0].table.table_id(),
                            physical_table[0]);
  }

  // All the tables are mutually exclusive, so there is no priority requirement.

  // Check the conditions on these tables.
  EXPECT_THAT(
      pipeline_tables.at(2).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_TCP, true)));
  EXPECT_THAT(
      pipeline_tables.at(4).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_TCP, true)));
}

// This case tests conflicts between UDP/TCP IsValid conditions. Tables with
// conflicting UDP/TCP IsValid conditions should be dropped.
TEST(PipelineProcessorTest, ConflictingUdpTcpIsValidCondition) {
  P4ControlBlock block;
  *block.add_statements() =
      IsValidBuilder()
          .Header(P4HeaderType::P4_HEADER_UDP)
          .DoIfValid(IsValidBuilder()
                         .Header(P4HeaderType::P4_HEADER_TCP)
                         .DoIfValid(ApplyTable(1))
                         .DoIfInvalid(ApplyTable(2))
                         .Build())
          .Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect 2 independent physical tables.
  ASSERT_EQ(pipeline.size(), 1);
  ASSERT_EQ(pipeline[0].size(), 1);

  // Move the tables into a more easily accessible structure.
  absl::flat_hash_map<int, PipelineTable> pipeline_tables;
  for (const auto& physical_table : pipeline) {
    pipeline_tables.emplace(physical_table[0].table.table_id(),
                            physical_table[0]);
  }

  // All the tables are mutually exclusive, so there is no priority requirement.

  // Check the conditions on these tables.
  EXPECT_THAT(
      pipeline_tables.at(2).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_UDP, true)));
}

// This case tests multiple subtables from a single parent table. Each subtable
// may have differing priorities, but they must all be lower than the parent
// table.
TEST(PipelineProcessorTest, BranchedPriorities) {
  P4ControlBlock block;
  *block.add_statements()->mutable_apply() = Table(1);
  *block.add_statements() =
      HitBuilder().OnMiss(Table(1)).Do(ApplyTable(2)).Build();
  *block.add_statements() = HitBuilder()
                                .OnMiss(Table(2))
                                .Do(IsValidBuilder()
                                        .Header(P4HeaderType::P4_HEADER_GRE)
                                        .DoIfValid(ApplyTable(3))
                                        .DoIfInvalid(ApplyTable(4))
                                        .Build())
                                .Build();
  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect one physical table with 4 logical tables.
  ASSERT_EQ(pipeline.size(), 1);
  ASSERT_EQ(pipeline[0].size(), 4);

  // Move the tables into a more easily accessible structure.
  absl::flat_hash_map<int, PipelineTable> pipeline_tables;
  for (const auto& table : pipeline[0]) {
    pipeline_tables.emplace(table.table.table_id(), table);
  }

  // Expect Priority order: 1 > 2 > (3,4).
  EXPECT_LT(0, pipeline_tables.at(4).priority);
  EXPECT_LT(0, pipeline_tables.at(3).priority);
  EXPECT_LT(pipeline_tables.at(3).priority, pipeline_tables.at(2).priority);
  EXPECT_LT(pipeline_tables.at(4).priority, pipeline_tables.at(2).priority);
  EXPECT_LT(pipeline_tables.at(2).priority, pipeline_tables.at(1).priority);

  // Check the IsValid conditions on the tables.
  EXPECT_THAT(
      pipeline_tables.at(3).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_GRE, true)));
  EXPECT_THAT(
      pipeline_tables.at(4).valid_conditions,
      UnorderedElementsAre(std::make_pair(P4HeaderType::P4_HEADER_GRE, false)));
}

// This case tests that unknown IsValid conditions are skipped during the
// processing.
TEST(PipelineProcessorTest, UnknownIsValidConditions) {
  P4ControlBlock block;
  *block.add_statements()->mutable_apply() = Table(1);
  *block.add_statements() = IsValidBuilder()
                                .Header(P4HeaderType::P4_HEADER_UNKNOWN)
                                .DoIfValid(ApplyTable(2))
                                .DoIfInvalid(ApplyTable(3))
                                .Build();
  *block.add_statements()->mutable_apply() = Table(4);
  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect logical tables 1 and 4 as separate physical tables.
  ASSERT_EQ(pipeline.size(), 2);
  ASSERT_EQ(pipeline[0].size(), 1);
  ASSERT_EQ(pipeline[1].size(), 1);
  EXPECT_EQ(pipeline[0][0].table.table_id(), 1);
  EXPECT_EQ(pipeline[1][0].table.table_id(), 4);
}

// This case tests that conflicting IsValid UDP and UDP payload conditions are
// skipped during the processing.
TEST(PipelineProcessorTest, ConflictingUdpIsValidConditions) {
  P4ControlBlock block;
  *block.add_statements()->mutable_apply() = Table(1);
  *block.add_statements() =
      IsValidBuilder()
          .Header(P4HeaderType::P4_HEADER_UDP)
          .DoIfValid(ApplyTable(2))
          .DoIfInvalid(IsValidBuilder()
                           .Header(P4HeaderType::P4_HEADER_UDP_PAYLOAD)
                           .DoIfValid(ApplyTable(3))
                           .Build())
          .Build();
  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline. Table 3 should not be present.
  // We expect logical tables 1 and 2 as separate physical tables.
  ASSERT_EQ(pipeline.size(), 2);
  ASSERT_EQ(pipeline[0].size(), 1);
  ASSERT_EQ(pipeline[1].size(), 1);
  EXPECT_EQ(pipeline[0][0].table.table_id(), 1);
  EXPECT_EQ(pipeline[1][0].table.table_id(), 2);
}

// This case tests that consistent IsValid UDP and UDP payload conditions are
// combined during the processing.
TEST(PipelineProcessorTest, ConsistentUdpIsValidConditions) {
  P4ControlBlock block;
  *block.add_statements()->mutable_apply() = Table(1);
  *block.add_statements() =
      IsValidBuilder()
          .Header(P4HeaderType::P4_HEADER_UDP)
          .DoIfValid(IsValidBuilder()
                         .Header(P4HeaderType::P4_HEADER_UDP_PAYLOAD)
                         .DoIfValid(ApplyTable(2))
                         .Build())
          .Build();
  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  ASSERT_OK_AND_ASSIGN(auto pipeline_processor,
                       PipelineProcessor::CreateInstance(block));
  const std::vector<PipelineProcessor::PhysicalTableAsVector>& pipeline =
      pipeline_processor->PhysicalPipeline();

  SCOPED_TRACE(absl::StrCat("PhysicalPipeline\n",
                            pipeline_processor->PhysicalPipelineAsString()));

  // Evaluate the resulting pipeline.
  // We expect logical tables 1 and 2 as separate physical tables.
  ASSERT_EQ(pipeline.size(), 2);
  ASSERT_EQ(pipeline[0].size(), 1);
  ASSERT_EQ(pipeline[1].size(), 1);
  EXPECT_EQ(pipeline[0][0].table.table_id(), 1);
  EXPECT_EQ(pipeline[1][0].table.table_id(), 2);
  // We should have collapsed the UDP payload condition into one UDP condition.
  ASSERT_EQ(pipeline[1][0].valid_conditions.size(), 1);
  ASSERT_EQ(pipeline[1][0].valid_conditions.count(P4_HEADER_UDP) +
                pipeline[1][0].valid_conditions.count(P4_HEADER_UDP_PAYLOAD),
            1);
  ASSERT_EQ(pipeline[1][0].valid_conditions.begin()->second, true);
}

//*****************************************************************************
//  Error Conditions
//*****************************************************************************

// This case tests pipeline rejection when a table is applied multiple times.
TEST(PipelineProcessorTest, ReApplyTable) {
  P4ControlBlock block;
  *block.add_statements()->mutable_apply() = Table(1);
  *block.add_statements()->mutable_apply() = Table(1);

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));
  // Create the pipeline.
  EXPECT_THAT(PipelineProcessor::CreateInstance(block).status(),
              StatusIs(HerculesErrorSpace(), ERR_INVALID_PARAM,
                       HasSubstr("Cannot apply a table more than once.")));
}

// This case tests pipeline rejection when the dependencies of tables changes
// in the control block.
TEST(PipelineProcessorTest, MixedDependencies) {
  // Create the following dependencies:
  // Table1 (No dependency).
  // Table2 (No dependency).
  // Table3 from Table1.
  // Table4 from Table3 from Table2 (inconsistent Table3 dependency).
  P4ControlBlock block;
  *block.add_statements()->mutable_apply() = Table(1);
  *block.add_statements()->mutable_apply() = Table(2);
  *block.add_statements() =
      HitBuilder().OnMiss(Table(1)).Do(ApplyTable(3)).Build();
  *block.add_statements() =
      HitBuilder()
          .OnMiss(Table(2))
          .Do(HitBuilder().OnMiss(Table(3)).Do(ApplyTable(4)).Build())
          .Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));
  // Create the pipeline.
  EXPECT_THAT(PipelineProcessor::CreateInstance(block).status(),
              StatusIs(HerculesErrorSpace(), ERR_INVALID_PARAM,
                       HasSubstr("Inconsistent dependency")));
}

// This case tests pipeline rejection when a table is branched on before it is
// applied.
TEST(PipelineProcessorTest, BranchBeforeApply) {
  P4ControlBlock block;
  *block.add_statements() =
      HitBuilder().OnMiss(Table(1)).Do(ApplyTable(2)).Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));
  // Create the pipeline.
  EXPECT_THAT(
      PipelineProcessor::CreateInstance(block).status(),
      StatusIs(HerculesErrorSpace(), ERR_INVALID_PARAM,
               HasSubstr("Cannot branch on a table before it is applied.")));
}

// This case tests pipeline rejection when a table is applied after another
// table is hit.
TEST(PipelineProcessorTest, OnHitDependency) {
  P4ControlBlock block;
  *block.add_statements()->mutable_apply() = Table(1);
  *block.add_statements() =
      HitBuilder().OnHit(Table(1)).Do(ApplyTable(2)).Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));
  // Create the pipeline.
  EXPECT_THAT(PipelineProcessor::CreateInstance(block).status(),
              StatusIs(HerculesErrorSpace(), ERR_INVALID_PARAM,
                       HasSubstr("On-hit actions")));
}

// This case tests pipeline rejection when a table is applied after another
// table is hit. This test used !Miss as the hit condition.
TEST(PipelineProcessorTest, OnNotMissDependency) {
  P4ControlBlock block;
  *block.add_statements()->mutable_apply() = Table(1);
  *block.add_statements() =
      HitBuilder().OnHit(Table(1)).UseFalse().Do(ApplyTable(2)).Build();

  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));
  // Create the pipeline.
  EXPECT_THAT(PipelineProcessor::CreateInstance(block).status(),
              StatusIs(HerculesErrorSpace(), ERR_INVALID_PARAM,
                       HasSubstr("On-hit actions")));
}

// This case tests pipeline rejection when a table depends on another table in a
// different pipeline stage.
TEST(PipelineProcessorTest, ConflictingStageDependency) {
  P4ControlBlock block;
  *block.add_statements()->mutable_apply() = Table(1, P4Annotation::VLAN_ACL);
  *block.add_statements() = HitBuilder()
                                .OnMiss(Table(1, P4Annotation::VLAN_ACL))
                                .Do(ApplyTable(2, P4Annotation::INGRESS_ACL))
                                .Build();
  SCOPED_TRACE(absl::StrCat("P4ControlBlock:\n", block.DebugString()));

  // Create the pipeline.
  EXPECT_THAT(PipelineProcessor::CreateInstance(block).status(),
              StatusIs(HerculesErrorSpace(), ERR_INVALID_PARAM,
                       HasSubstr("Pipeline stage mismatch")));
}

}  // namespace
}  // namespace bcm
}  // namespace hal
}  // namespace hercules
}  // namespace google
