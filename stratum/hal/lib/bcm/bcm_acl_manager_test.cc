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


#include "stratum/hal/lib/bcm/bcm_acl_manager.h"

#include <vector>

#include "stratum/lib/test_utils/p4_proto_builders.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/substitute.h"
#include "stratum/glue/status/canonical_errors.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/bcm/bcm_chassis_ro_mock.h"
#include "stratum/hal/lib/bcm/bcm_sdk_mock.h"
#include "stratum/hal/lib/bcm/bcm_table_manager_mock.h"
#include "stratum/hal/lib/p4/p4_table_mapper_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/p4_annotation.pb.h"
#include "stratum/glue/gtl/flat_map.h"
#include "stratum/glue/gtl/flat_set.h"
#include "stratum/glue/gtl/map_util.h"
#include "util/task/canonical_errors.h"
#include "util/task/status.h"
#include "util/task/statusor.h"

DECLARE_string(bcm_hardware_specs_file);
DECLARE_string(test_tmpdir);

namespace stratum {
namespace hal {
namespace bcm {
namespace {

using test_utils::EqualsProto;
using test_utils::PartiallyUnorderedEqualsProto;
using test_utils::p4_proto_builders::ApplyTable;
using test_utils::p4_proto_builders::IsValidBuilder;
using test_utils::p4_proto_builders::P4ControlTableRefBuilder;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::UnorderedElementsAreArray;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

using StageToTablesMap =
    absl::flat_hash_map<P4Annotation::PipelineStage,
                        std::vector<::p4::config::v1::Table>>;
using StageToControlBlockMap =
    absl::flat_hash_map<P4Annotation::PipelineStage, P4ControlBlock>;

constexpr char kDefaultBcmHardwareSpecsText[] = R"PROTO(
  chip_specs {
    chip_type: UNKNOWN
    acl {
      field_processors {
        stage: VLAN
        slices { count: 4 width: 200 size: 256 } }
      field_processors {
        stage: INGRESS
        slices { count: 8 width: 200 size: 256 }
      }
      field_processors {
        stage: EGRESS
        slices { count: 4 width: 200 size: 256 }
      }
    }
    udf { chunk_bits: 16 chunks_per_set: 8 set_count: 2 }
  }
)PROTO";

// *****************************************************************************
// Matchers (and supporting functions)
// *****************************************************************************
// Reports the difference between two sets. Fills in sets of left-side-only and
// right-side-only values. Returns true if the sets are different.
template <typename T>
bool SetDifference(const T& left, const T& right, T* left_only, T* right_only) {
  left_only->clear();
  right_only->clear();
  for (const auto& left_val : left) {
    if (!right.count(left_val)) left_only->insert(left_val);
  }
  for (const auto& right_val : right) {
    if (!left.count(right_val)) right_only->insert(right_val);
  }
  return !right_only->empty() || !left_only->empty();
}

// Reports the difference between two maps. Fills in sets of left-side-only and
// right-side-only values. Returns true if the maps are different.
template <typename T, typename U>
bool MapDifference(const T& left, const T& right, U* left_only, U* right_only) {
  for (const auto& left_pair : left) {
    auto right_value = right.find(left_pair.first);
    if (right_value == right.end() || right_value->second != left_pair.second) {
      left_only->push_back(left_pair);
    }
  }
  for (const auto& right_pair : right) {
    auto left_value = left.find(right_pair.first);
    if (left_value == left.end() || left_value->second != right_pair.second) {
      right_only->push_back(right_pair);
    }
  }
  return !right_only->empty() || !left_only->empty();
}

// Returns a string representing a P4FieldType. If the type is valid, returns
// the enum name. If not, returns the integer value as a string.
std::string P4FieldTypeToString(P4FieldType type) {
  std::string s = P4FieldType_Name(type);
  if (s.empty()) {
    return absl::StrCat(type);
  }
  return s;
}

MATCHER_P(DerivedFromStatus, status, "") {
  if (arg.error_code() != status.error_code()) {
    return false;
  }
  if (arg.error_message().find(status.error_message()) == std::string::npos) {
    *result_listener << "\nOriginal error string: \"" << status.error_message()
                     << "\" is missing from the actual status.";
    return false;
  }
  return true;
}

// *****************************************************************************
// Helper Classes
// *****************************************************************************

// Fills a vector with all the tables applied in a control block.
void GetTablesFromControlBlock(const P4ControlBlock& control_block,
                               std::vector<::p4::config::v1::Table>* tables) {
  auto add_table = [tables](const P4ControlTableRef& ref) {
    ::p4::config::v1::Table table;
    table.mutable_preamble()->set_id(ref.table_id());
    table.mutable_preamble()->set_name(ref.table_name());
    tables->push_back(table);
  };
  for (const P4ControlStatement& statement : control_block.statements()) {
    switch (statement.statement_case()) {
      case P4ControlStatement::kApply:
        add_table(statement.apply());
        break;
      case P4ControlStatement::kBranch:
        GetTablesFromControlBlock(statement.branch().true_block(), tables);
        GetTablesFromControlBlock(statement.branch().false_block(), tables);
        break;
      case P4ControlStatement::kFixedPipeline:
        for (const auto& table_ref : statement.fixed_pipeline().tables()) {
          add_table(table_ref);
        }
        break;
      default:
        break;
    }
  }
}

// This class provides convenient methods for building P4ControlBlock pipelines.
class ControlBlockHelper {
 public:
  ControlBlockHelper()
      : control_block_(), stage_(P4Annotation::INGRESS_ACL), num_tables_(0) {}

  // Returns the number of tables currently in the control block.
  int GetNumTables() { return num_tables_; }

  // Sets the stage that will be applied to subsequent tables. If stage() has
  // not been called, the default stage is INGRESS_ACL.
  ControlBlockHelper& stage(P4Annotation::PipelineStage stage) {
    stage_ = stage;
    return *this;
  }

  // Appends a nested set of tables to the top-level of the control block
  // matching the flow:
  // reference[1].apply();
  // if (!reference[1].hit()) {
  //   reference[2].apply();
  //   if (!reference[2].hit()) {
  //     reference[3].apply();
  //   }
  // }
  ControlBlockHelper& append_nested(
      const std::vector<P4ControlTableRef>& tables) {
    control_block_.mutable_statements()->MergeFrom(
        MakeBranchStatement(tables.begin(), tables.end()).statements());
    num_tables_ += tables.size();
    return *this;
  }

  // Same as above, but crafts the references from tables. Applies the latest
  // stage provided by stage() or INGRESS_ACL if stage() has not been called.
  ControlBlockHelper& append_nested(
      const std::vector<::p4::config::v1::Table>& tables) {
    std::vector<P4ControlTableRef> references;
    for (const ::p4::config::v1::Table& table : tables) {
      references.push_back(
          P4ControlTableRefBuilder(table.preamble()).Stage(stage_).Build());
    }
    return append_nested(references);
  }

  // Appends a new table apply the top-level of the control block.
  ControlBlockHelper& append(const P4ControlTableRef& reference) {
    *control_block_.add_statements()->mutable_apply() = reference;
    ++num_tables_;
    return *this;
  }

  // Same as above, but crafts the reference from a table and a stage. Applies
  // the latest stage provided by stage() or INGRESS_ACL if stage() has not been
  // called.
  ControlBlockHelper& append(const ::p4::config::v1::Table& table) {
    return append(
        P4ControlTableRefBuilder(table.preamble()).Stage(stage_).Build());
  }

  // Returns the control block.
  const P4ControlBlock& operator()() const { return control_block_; }

 private:
  // Helper function to recursively create a P4ControlBlock for a nested table
  // set. Each step returns:
  //
  // statements {
  //   apply { <*begin> }
  // }
  // statements {  // This statement is skipped at the lowest level.
  //   branch {
  //     condition {
  //       hit { <*begin> }
  //     }
  //     false_block {
  //       <P4ControlBlock from recursion>
  //     }
  //   }
  // }
  P4ControlBlock MakeBranchStatement(
      std::vector<P4ControlTableRef>::const_iterator begin,
      std::vector<P4ControlTableRef>::const_iterator end) {
    P4ControlBlock control_block;
    // Return an empty P4ControlBlock if there are no tables.
    if (begin == end) return control_block;

    // Add an apply statement for the first table.
    const P4ControlTableRef& base_table = *begin;
    *control_block.add_statements()->mutable_apply() = base_table;

    // Add a branch statement to apply subsequent tables if the first table
    // misses.
    if (++begin != end) {
      P4IfStatement* branch_statement =
          control_block.add_statements()->mutable_branch();
      *branch_statement->mutable_condition()->mutable_hit() = base_table;
      *branch_statement->mutable_false_block() =
          MakeBranchStatement(begin, end);
    }
    return control_block;
  }

  P4ControlBlock control_block_;       // Current control block.
  P4Annotation::PipelineStage stage_;  // Stage to apply to new tables.
  int num_tables_;  // The number of tables in the current control block.
};

// *****************************************************************************
// Constants for P4::config::Table
// *****************************************************************************
// These constants & constant functions create the default set of Tables & maps
// that can be used by the tests. Control information is not provided here.

// Number of tables to create for each pipeline stage.
constexpr int kTablesPerStage = 21;
// Default unit id.
constexpr int kUnit = 1;
// Default node/device id.
constexpr uint64 kNodeId = 123456;
// Default table size.
constexpr int kTableSize = 10;

// Map of default P4 tables indexed by table id.
const gtl::flat_map<int, ::p4::config::v1::Table>& DefaultP4Tables() {
  static const auto* tables = []() {
    auto* tables = new gtl::flat_map<int, ::p4::config::v1::Table>();
    ::p4::config::v1::Table table;
    CHECK_OK(ParseProtoFromString(R"PROTO(
      preamble { id: 1 name: "table_1" }
      match_fields { id: 1 name: "P4_FIELD_TYPE_ETH_SRC" match_type: TERNARY }
      size: 10
    )PROTO", &table));
    tables->insert(std::make_pair(1, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      preamble { id: 2 name: "table_2" }
      match_fields { id: 2 name: "P4_FIELD_TYPE_ETH_DST" match_type: TERNARY }
      size: 10
    )PROTO", &table));
    tables->insert(std::make_pair(2, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      preamble { id: 3 name: "table_3" }
      match_fields { id: 3 name: "P4_FIELD_TYPE_ETH_TYPE" match_type: TERNARY }
      size: 10
    )PROTO", &table));
    tables->insert(std::make_pair(3, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      preamble { id: 4 name: "table_4" }
      match_fields { id: 4 name: "P4_FIELD_TYPE_IPV4_SRC" match_type: TERNARY }
      size: 10
    )PROTO", &table));
    tables->insert(std::make_pair(4, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      preamble { id: 5 name: "table_5" }
      match_fields { id: 5 name: "P4_FIELD_TYPE_IPV4_DST" match_type: TERNARY }
      size: 10
    )PROTO", &table));
    tables->insert(std::make_pair(5, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      preamble { id: 6 name: "table_6" }
      match_fields { id: 6 name: "P4_FIELD_TYPE_IPV6_SRC" match_type: TERNARY }
      size: 10
    )PROTO", &table));
    tables->insert(std::make_pair(6, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      preamble { id: 7 name: "table_7" }
      match_fields { id: 7 name: "P4_FIELD_TYPE_IPV6_DST" match_type: TERNARY }
      size: 10
    )PROTO", &table));
    tables->insert(std::make_pair(7, table));

    CHECK_OK(ParseProtoFromString(R"PROTO(
      preamble { id: 8 name: "table_8" }
      match_fields { id: 1 name: "P4_FIELD_TYPE_ETH_SRC" match_type: TERNARY }
      match_fields { id: 2 name: "P4_FIELD_TYPE_ETH_DST" match_type: TERNARY }
      size: 10
    )PROTO", &table));
    tables->insert(std::make_pair(8, table));

    CHECK_OK(ParseProtoFromString(R"PROTO(
      preamble { id: 9 name: "table_9" }
      match_fields { id: 4 name: "P4_FIELD_TYPE_IPV4_SRC" match_type: TERNARY }
      match_fields { id: 5 name: "P4_FIELD_TYPE_IPV4_DST" match_type: TERNARY }
      size: 10
    )PROTO", &table));
    tables->insert(std::make_pair(9, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      preamble { id: 10 name: "table_10" }
      match_fields { id: 6 name: "P4_FIELD_TYPE_IPV6_SRC" match_type: TERNARY }
      match_fields { id: 7 name: "P4_FIELD_TYPE_IPV6_DST" match_type: TERNARY }
      size: 10
    )PROTO", &table));
    tables->insert(std::make_pair(10, table));
    return tables;
  }();
  return *tables;
}

// Returns a vector of just the tables in DefaultP4Tables().
const std::vector<::p4::config::v1::Table> DefaultP4TablesVector() {
  static const auto* tables = []() {
    auto* tables = new std::vector<::p4::config::v1::Table>();
    for (const auto& pair : DefaultP4Tables()) {
      tables->push_back(pair.second);
    }
    return tables;
  }();
  return *tables;
}

// Map of BcmAClTables corresponding to the default P4 tables, indexed by table
// id. These tables do not have a stage since that is not specified by the P4
// tables.
const gtl::flat_map<int, BcmAclTable>& DefaultBcmAclTables() {
  static const auto* tables = []() {
    auto tables = new gtl::flat_map<int, BcmAclTable>();
    BcmAclTable table;
    CHECK_OK(ParseProtoFromString(R"PROTO(
      fields { type: ETH_SRC })PROTO", &table));
    tables->insert(std::make_pair(1, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      fields { type: ETH_DST })PROTO", &table));
    tables->insert(std::make_pair(2, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      fields { type: ETH_TYPE })PROTO", &table));
    tables->insert(std::make_pair(3, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      fields { type: IPV4_SRC })PROTO", &table));
    tables->insert(std::make_pair(4, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      fields { type: IPV4_DST })PROTO", &table));
    tables->insert(std::make_pair(5, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      fields { type: IPV6_SRC_UPPER_64 })PROTO", &table));
    tables->insert(std::make_pair(6, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      fields { type: IPV6_DST_UPPER_64 })PROTO", &table));
    tables->insert(std::make_pair(7, table));

    CHECK_OK(ParseProtoFromString(R"PROTO(
      fields { type: ETH_SRC }
      fields { type: ETH_DST }
    )PROTO", &table));
    tables->insert(std::make_pair(8, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      fields { type: IPV4_SRC }
      fields { type: IPV4_DST }
    )PROTO", &table));
    tables->insert(std::make_pair(9, table));
    CHECK_OK(ParseProtoFromString(R"PROTO(
      fields { type: IPV6_SRC_UPPER_64 }
      fields { type: IPV6_DST_UPPER_64 }
    )PROTO", &table));
    tables->insert(std::make_pair(10, table));
    return tables;
  }();
  return *tables;
}

// Default control block using the tables in DefaultP4Tables.
const P4ControlBlock& DefaultControlBlock() {
  static const P4ControlBlock* control_block = []() {
    std::vector<::p4::config::v1::Table> tables;
    for (const auto& pair : DefaultP4Tables()) {
      tables.push_back(pair.second);
    }
    ControlBlockHelper control_block_helper;
    control_block_helper.stage(P4Annotation::INGRESS_ACL)
        .append(tables[0])
        .append_nested({tables[1], tables[2], tables[3]})
        .stage(P4Annotation::VLAN_ACL)
        .append(tables[4])
        .append(tables[5])
        .stage(P4Annotation::EGRESS_ACL)
        .append(tables[6])
        .append_nested({tables[7], tables[8], tables[9]});
    CHECK_EQ(control_block_helper.GetNumTables(), DefaultP4Tables().size())
        << "The default control block does not contain all the default ACL "
           "tables.";
    P4ControlBlock* control_block = new P4ControlBlock();
    *control_block = control_block_helper();
    return control_block;
  }();
  return *control_block;
}

// Sets the BcmAclStage for all tables within a map.
const gtl::flat_map<int, BcmAclTable> SetStage(
    const gtl::flat_map<int, BcmAclTable>& original_tables, BcmAclStage stage) {
  auto tables = original_tables;
  for (auto& pair : tables) {
    pair.second.set_stage(stage);
  }
  return tables;
}

// Build a simple entry that uses the first match fields in a table.
::p4::v1::TableEntry BuildSimpleEntry(const ::p4::config::v1::Table& table,
                                      int match_value) {
  ::p4::v1::TableEntry entry;
  entry.set_table_id(table.preamble().id());
  entry.add_match()->set_field_id(table.match_fields(0).id());
  entry.mutable_match(0)->mutable_ternary()->set_value(
      absl::StrCat(match_value));
  return entry;
}

// Build a P4 ForwardingPipelineConfig that holds a given control block.
::p4::v1::ForwardingPipelineConfig BuildForwardingPipelineConfig(
    const P4ControlBlock& control_block) {
  P4PipelineConfig p4_pipeline_config;
  *p4_pipeline_config.add_p4_controls()->mutable_main() = control_block;
  p4_pipeline_config.mutable_p4_controls(0)->set_name("test_control");
  ::p4::v1::ForwardingPipelineConfig forwarding_pipeline_config;
  p4_pipeline_config.SerializeToString(
      forwarding_pipeline_config.mutable_p4_device_config());
  return forwarding_pipeline_config;
}

// *****************************************************************************
// Testfixture
// *****************************************************************************
// BcmAclManagerTest is used to test the BcmAclManager class.
class BcmAclManagerTest : public ::testing::Test {
 protected:
  BcmAclManagerTest() {
    FLAGS_bcm_hardware_specs_file =
        FLAGS_test_tmpdir + "/bcm_hardware_specs.pb.txt";
    CHECK_OK(WriteStringToFile(kDefaultBcmHardwareSpecsText,
                               FLAGS_bcm_hardware_specs_file));
    bcm_chassis_ro_mock_ = absl::make_unique<BcmChassisRoMock>();
    bcm_table_manager_mock_ = absl::make_unique<BcmTableManagerMock>();
    bcm_sdk_mock_ = absl::make_unique<testing::NiceMock<BcmSdkMock>>();
    p4_table_mapper_mock_ = absl::make_unique<P4TableMapperMock>();
    bcm_acl_manager_ = BcmAclManager::CreateInstance(
        bcm_chassis_ro_mock_.get(), bcm_table_manager_mock_.get(),
        bcm_sdk_mock_.get(), p4_table_mapper_mock_.get(), kUnit);
    bcm_table_manager_ = BcmTableManager::CreateInstance(
        bcm_chassis_ro_mock_.get(), p4_table_mapper_mock_.get(), kUnit);
  }

  ::util::Status DefaultError() {
    return ::util::Status(StratumErrorSpace(), ERR_UNKNOWN, "Some error");
  }

  void SetUp() override;
  void SetUpP4InfoManagerMock();
  void SetUpP4TableMapperMock();
  void SetUpBcmTableManagerMock();
  void SetUpBcmSdkMock();

  // Installs the provided set of tables.
  ::util::Status SetUpTables(const std::vector<::p4::config::v1::Table>& tables,
                             const P4ControlBlock& control_block);

  // Installs the default tables.
  ::util::Status SetUpDefaultTables() {
    return SetUpTables(DefaultP4TablesVector(), DefaultControlBlock());
  }

  // Looks up tables from mock_tables_ by table id. This should be used in lieu
  // of P4TableMapper::LookupTable.
  ::util::Status LookupTable(int id, ::p4::config::v1::Table* table);

  // Fill a mapped_field field_type based on a table match_field. Fail if
  // table_id/match_id pair is unknown. This should be used in lieu of
  // P4TableMapper::MapMatchField.
  ::util::Status MapMatchField(int table_id, int match_id,
                               MappedField* mapped_field);

  // Mock config state. Map of table ID to table.
  gtl::flat_map<int, ::p4::config::v1::Table> mock_tables_;

  // Class instances used for testing (real and mocked). Note that in addition
  // to a mocked version of BcmTableManager passed to BcmAclManager, we use a
  // real BcmTableManager as well to test logical table creation/insertion.
  std::unique_ptr<BcmAclManager> bcm_acl_manager_;
  std::unique_ptr<BcmChassisRoMock> bcm_chassis_ro_mock_;
  std::unique_ptr<BcmTableManagerMock> bcm_table_manager_mock_;
  std::unique_ptr<testing::NiceMock<BcmSdkMock>> bcm_sdk_mock_;
  std::unique_ptr<BcmTableManager> bcm_table_manager_;
  std::unique_ptr<P4TableMapperMock> p4_table_mapper_mock_;
};

void BcmAclManagerTest::SetUp() {
  SetUpP4TableMapperMock();
  SetUpBcmTableManagerMock();
  SetUpBcmSdkMock();
}

void BcmAclManagerTest::SetUpP4TableMapperMock() {
  ON_CALL(*p4_table_mapper_mock_, LookupTable(_, _))
      .WillByDefault(Invoke(this, &BcmAclManagerTest::LookupTable));
  EXPECT_CALL(*p4_table_mapper_mock_, LookupTable(_, _)).Times(AnyNumber());

  ON_CALL(*p4_table_mapper_mock_, MapMatchField(_, _, _))
      .WillByDefault(Invoke(this, &BcmAclManagerTest::MapMatchField));
  EXPECT_CALL(*p4_table_mapper_mock_, MapMatchField(_, _, _))
      .Times(AnyNumber());
}

void BcmAclManagerTest::SetUpBcmTableManagerMock() {
  // Push calls to our real table manager that will really store the tables.
  ON_CALL(*bcm_table_manager_mock_, P4FieldTypeToBcmFieldType(_))
      .WillByDefault(Invoke(bcm_table_manager_.get(),
                            &BcmTableManager::P4FieldTypeToBcmFieldType));
  EXPECT_CALL(*bcm_table_manager_mock_, P4FieldTypeToBcmFieldType(_))
      .Times(AnyNumber());
  ON_CALL(*bcm_table_manager_mock_, ReadTableEntries(_, _, _))
      .WillByDefault(
          Invoke(bcm_table_manager_.get(), &BcmTableManager::ReadTableEntries));
  EXPECT_CALL(*bcm_table_manager_mock_, ReadTableEntries(_, _, _))
      .Times(AnyNumber());
  ON_CALL(*bcm_table_manager_mock_, DeleteTable(_))
      .WillByDefault(
          Invoke(bcm_table_manager_.get(), &BcmTableManager::DeleteTable));
  ON_CALL(*bcm_table_manager_mock_, AddAclTableEntry(_, _))
      .WillByDefault(
          Invoke(bcm_table_manager_.get(), &BcmTableManager::AddAclTableEntry));
  ON_CALL(*bcm_table_manager_mock_, UpdateTableEntry(_))
      .WillByDefault(
          Invoke(bcm_table_manager_.get(), &BcmTableManager::UpdateTableEntry));
  ON_CALL(*bcm_table_manager_mock_, DeleteTableEntry(_))
      .WillByDefault(
          Invoke(bcm_table_manager_.get(), &BcmTableManager::DeleteTableEntry));
  ON_CALL(*bcm_table_manager_mock_, GetReadOnlyAclTable(_))
      .WillByDefault(Invoke(bcm_table_manager_.get(),
                            &BcmTableManager::GetReadOnlyAclTable));
  EXPECT_CALL(*bcm_table_manager_mock_, GetReadOnlyAclTable(_))
      .Times(AnyNumber());
  ON_CALL(*bcm_table_manager_mock_, GetAllAclTableIDs())
      .WillByDefault(Invoke(bcm_table_manager_.get(),
                            &BcmTableManager::GetAllAclTableIDs));
  EXPECT_CALL(*bcm_table_manager_mock_, GetAllAclTableIDs()).Times(AnyNumber());
  ON_CALL(*bcm_table_manager_mock_, AddAclTable(_))
      .WillByDefault(
          Invoke(bcm_table_manager_.get(), &BcmTableManager::AddAclTable));
  ON_CALL(*bcm_table_manager_mock_, UpdateTableEntryMeter(_))
      .WillByDefault(Invoke(bcm_table_manager_.get(),
                            &BcmTableManager::UpdateTableEntryMeter));
}

void BcmAclManagerTest::SetUpBcmSdkMock() {
  // By default, CreateAclTable will report a unique table id for each table.
  ON_CALL(*bcm_sdk_mock_, CreateAclTable(_, _))
      .WillByDefault(Invoke([](int /*unit*/, const BcmAclTable& table) {
        static int table_id = 1;
        return ++table_id;
      }));
  ON_CALL(*bcm_sdk_mock_, DestroyAclTable(_, _))
      .WillByDefault(Return(::util::OkStatus()));
}

::util::Status BcmAclManagerTest::LookupTable(int id,
                                              ::p4::config::v1::Table* table) {
  auto result = gtl::FindOrNull(mock_tables_, id);
  if (result == nullptr) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND) << "Table " << id << " not found.";
  }
  *table = *result;
  return ::util::OkStatus();
}

::util::Status BcmAclManagerTest::MapMatchField(int table_id, int match_id,
                                                MappedField* mapped_field) {
  ::p4::config::v1::Table table;
  RETURN_IF_ERROR(LookupTable(table_id, &table));
  for (const auto& match : table.match_fields()) {
    if (match.id() == match_id) {
      P4FieldType field_type = P4_FIELD_TYPE_UNKNOWN;
      CHECK(P4FieldType_Parse(match.name(), &field_type))
          << "Failed to parse P4FieldType: " << match.name();
      mapped_field->set_type(field_type);
      return ::util::OkStatus();
    }
  }
  return MAKE_ERROR(ERR_ENTRY_NOT_FOUND).SetNoLogging()
         << "No field_type found for table " << table_id << ", match "
         << match_id << ".";
}

::util::Status BcmAclManagerTest::SetUpTables(
    const std::vector<::p4::config::v1::Table>& tables,
    const P4ControlBlock& control_block) {
  for (const auto& table : tables) {
    mock_tables_[table.preamble().id()] = table;
  }

  EXPECT_CALL(*bcm_table_manager_mock_, GetAllAclTableIDs()).Times(AtLeast(1));
  EXPECT_CALL(*bcm_table_manager_mock_, P4FieldTypeToBcmFieldType(_))
      .Times(AtLeast(1));
  EXPECT_CALL(*bcm_table_manager_mock_, AddAclTable(_))
      .Times(AtLeast(tables.size()));

  return bcm_acl_manager_->PushForwardingPipelineConfig(
      BuildForwardingPipelineConfig(control_block));
}

// *****************************************************************************
// Tests
// *****************************************************************************

TEST_F(BcmAclManagerTest, PushChassisConfig_Success) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);

  EXPECT_CALL(*bcm_sdk_mock_, InitAclHardware(kUnit))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetAclControl(kUnit, _))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_acl_manager_->PushChassisConfig(config, kNodeId));
}

TEST_F(BcmAclManagerTest, PushChassisConfig_NoBcmHardwareSpecs) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  FLAGS_bcm_hardware_specs_file = "tmp/nothing_to_see_here";

  // Push config and evaluate the error return.
  EXPECT_FALSE(bcm_acl_manager_->PushChassisConfig(config, kNodeId).ok());
}

TEST_F(BcmAclManagerTest, PushChassisConfig_BadBcmHardwareSpecs) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  ASSERT_OK(WriteStringToFile("random_text", FLAGS_bcm_hardware_specs_file));

  // Push config and evaluate the error return.
  EXPECT_FALSE(bcm_acl_manager_->PushChassisConfig(config, kNodeId).ok());
}

TEST_F(BcmAclManagerTest, PushChassisConfig_UnknownChip) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  config.mutable_chassis()->set_platform(PLT_GENERIC_TRIDENT2);
  ASSERT_OK(WriteStringToFile("chip_specs { chip_type: TOMAHAWK }",
                              FLAGS_bcm_hardware_specs_file));

  // Push config and evaluate the error return.
  EXPECT_THAT(bcm_acl_manager_->PushChassisConfig(config, kNodeId),
              StatusIs(HerculesErrorSpace(), ERR_INTERNAL,
                       HasSubstr("Unable to find ChipModelSpec")));
}

TEST_F(BcmAclManagerTest, PushChassisConfig_AclControlFailure) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);

  // Expect failure when the SDK can't initialize the ACL control.
  EXPECT_CALL(*bcm_sdk_mock_, InitAclHardware(kUnit))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetAclControl(kUnit, _))
      .WillOnce(Return(DefaultError()));
  // Push config and evaluate the error return.
  EXPECT_THAT(bcm_acl_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
}

TEST_F(BcmAclManagerTest, PushChassisConfig_InitAclHardwareFailure) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);

  // Expect failure when the SDK can't initialize the ACL hardware.
  EXPECT_CALL(*bcm_sdk_mock_, InitAclHardware(kUnit))
      .WillOnce(Return(DefaultError()));
  // Push config and evaluate the error return.
  EXPECT_THAT(
      bcm_acl_manager_->PushChassisConfig(config, kNodeId),
      StatusIs(DefaultError().error_space(), DefaultError().error_code(),
               HasSubstr(DefaultError().error_message())));
}

TEST_F(BcmAclManagerTest, TestVerifyChassisConfigSuccess) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);

  EXPECT_CALL(*bcm_sdk_mock_, InitAclHardware(kUnit))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetAclControl(kUnit, _))
      .WillOnce(Return(::util::OkStatus()));

  // Call verify before and after config push.
  EXPECT_OK(bcm_acl_manager_->VerifyChassisConfig(config, kNodeId));
  EXPECT_OK(bcm_acl_manager_->PushChassisConfig(config, kNodeId));
  EXPECT_OK(bcm_acl_manager_->VerifyChassisConfig(config, kNodeId));
}

TEST_F(BcmAclManagerTest, TestVerifyChassisConfigFailure) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);

  // Verify failure for invalid node
  ::util::Status status = bcm_acl_manager_->VerifyChassisConfig(config, 0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid node ID"));

  // Change in the node_id after config push is reboot required.
  EXPECT_CALL(*bcm_sdk_mock_, InitAclHardware(kUnit))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetAclControl(kUnit, _))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_acl_manager_->PushChassisConfig(config, kNodeId));
  status = bcm_acl_manager_->VerifyChassisConfig(config, kNodeId + 1);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_REBOOT_REQUIRED, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Detected a change in the node_id"));
}

// Test setup for a pipeline with a one-to-one mapping of logical-to-physical
// tables.
TEST_F(BcmAclManagerTest, TestPushForwardingPipelineConfig_OneLinearStage) {
  // Generate the control block using the default table set.
  mock_tables_ = DefaultP4Tables();

  // Create a single ordered list for the tables.
  std::vector<::p4::config::v1::Table> tables;
  for (const auto& pair : DefaultP4Tables()) {
    tables.push_back(pair.second);
  }

  // Set up the control block by making all the tables apply sequentially.
  P4ControlBlock control_block;
  for (const auto& table : tables) {
    *control_block.add_statements()->mutable_apply() =
        P4ControlTableRefBuilder(table.preamble())
            .Stage(P4Annotation::INGRESS_ACL)
            .Build();
  }
  SCOPED_TRACE(
      absl::StrCat("Input control block:\n", control_block.DebugString()));

  // Expect to install each table into the BcmTableManager.
  EXPECT_CALL(*bcm_table_manager_mock_, AddAclTable(_)).Times(tables.size());

  // Expect to install each table separately into the SDK. We will mock the Bcm
  // table IDs as 100 + the P4 table ID (e.g. P4 ID 1 --> BCM ID 101).
  const auto ifp_bcm_tables =
      SetStage(DefaultBcmAclTables(), BCM_ACL_STAGE_IFP);
  for (const auto& table : tables) {
    int id = table.preamble().id();
    EXPECT_CALL(*bcm_sdk_mock_,
                CreateAclTable(kUnit, PartiallyUnorderedEqualsProto(
                                          ifp_bcm_tables.at(id))))
        .WillOnce(Return(100 + id));
  }

  // Set up the P4 ForwardingPipelineConfig object.
  P4PipelineConfig p4_pipeline_config;
  *p4_pipeline_config.add_p4_controls()->mutable_main() = control_block;
  p4_pipeline_config.mutable_p4_controls(0)->set_name("test_control");
  ::p4::v1::ForwardingPipelineConfig forwarding_pipeline_config;
  p4_pipeline_config.SerializeToString(
      forwarding_pipeline_config.mutable_p4_device_config());

  // Push the forwarding pipeline config (invoke the unit under test).
  ASSERT_OK(bcm_acl_manager_->PushForwardingPipelineConfig(
      forwarding_pipeline_config));

  // Check the software tables.
  int previous_priority = 0;
  for (const auto& table : tables) {
    SCOPED_TRACE(absl::StrCat("Failed verification for table: ",
                              table.preamble().ShortDebugString()));
    const AclTable* acl_table;
    ASSERT_OK_AND_ASSIGN(acl_table, bcm_table_manager_->GetReadOnlyAclTable(
                                        table.preamble().id()));
    // The first table should have the lowest priority. Subsequent tables should
    // have progressively higher priorities.
    EXPECT_GT(acl_table->Priority(), previous_priority);
    previous_priority = acl_table->Priority();
    // Check only the parameters that we pass in (stage & priority).
    EXPECT_EQ(acl_table->PhysicalTableId(), table.preamble().id() + 100);
    EXPECT_EQ(acl_table->Stage(), BCM_ACL_STAGE_IFP);
  }
}

// Test setup for a pipeline with a complex mapping of logical-to-physical
// tables.
TEST_F(BcmAclManagerTest, TestPushForwardingPipelineConfig_OneComplexStage) {
  // Generate the control block using the default table set.
  mock_tables_ = DefaultP4Tables();

  // Create a single ordered list for the tables we will use.
  // We will only use the first 8 tables.
  ASSERT_GE(DefaultP4Tables().size(), 8)
      << "There are not enough default tables for this test.";
  std::vector<::p4::config::v1::Table> tables;
  for (const auto& pair : DefaultP4Tables()) {
    tables.push_back(pair.second);
  }
  tables.resize(8);
  P4ControlBlock control_block =
      ControlBlockHelper()
          .stage(P4Annotation::INGRESS_ACL)
          .append(tables[0])
          .append_nested({tables[1], tables[2], tables[3]})
          .append(tables[4])
          .append_nested({tables[5], tables[6], tables[7]})();
  SCOPED_TRACE(
      absl::StrCat("Input control block:\n", control_block.DebugString()));

  // Expect to install each table into the BcmTableManager.
  EXPECT_CALL(*bcm_table_manager_mock_, AddAclTable(_)).Times(tables.size());

  // Expect to install 4 physical tables, one for each nest. We will capture and
  // sort the tables by priority to match the control block order.
  struct PriorityCompare {
    bool operator()(const BcmAclTable& lhs, const BcmAclTable& rhs) const {
      return lhs.priority() < rhs.priority();
    }
  };
  gtl::flat_set<BcmAclTable, PriorityCompare> bcm_tables;
  EXPECT_CALL(*bcm_sdk_mock_, CreateAclTable(kUnit, _))
      .Times(4)
      .WillRepeatedly(
          Invoke([&bcm_tables](int /*unit*/, const BcmAclTable& table) {
            static int table_id = 1;
            bcm_tables.insert(table);
            return ++table_id;
          }));

  // Set up the P4 ForwardingPipelineConfig object.
  P4PipelineConfig p4_pipeline_config;
  *p4_pipeline_config.add_p4_controls()->mutable_main() = control_block;
  p4_pipeline_config.mutable_p4_controls(0)->set_name("test_control");
  ::p4::v1::ForwardingPipelineConfig forwarding_pipeline_config;
  p4_pipeline_config.SerializeToString(
      forwarding_pipeline_config.mutable_p4_device_config());

  // Push the forwarding pipeline config (invoke the unit under test).
  ASSERT_OK(bcm_acl_manager_->PushForwardingPipelineConfig(
      forwarding_pipeline_config));

  // Check the software tables.
  std::vector<AclTable> acl_tables;
  for (const auto& table : tables) {
    const AclTable* acl_table;
    ASSERT_OK_AND_ASSIGN(acl_table, bcm_table_manager_->GetReadOnlyAclTable(
                                        table.preamble().id()));
    acl_tables.push_back(*acl_table);
  }

  // Expected priority order by table index:
  // [0], [3, 2, 1 (nested)], [4], [7, 6, 5] (nested)
  EXPECT_LT(acl_tables[0].Priority(), acl_tables[3].Priority());
  EXPECT_LT(acl_tables[3].Priority(), acl_tables[2].Priority());
  EXPECT_LT(acl_tables[2].Priority(), acl_tables[1].Priority());
  EXPECT_LT(acl_tables[1].Priority(), acl_tables[4].Priority());
  EXPECT_LT(acl_tables[4].Priority(), acl_tables[7].Priority());
  EXPECT_LT(acl_tables[7].Priority(), acl_tables[6].Priority());
  EXPECT_LT(acl_tables[6].Priority(), acl_tables[5].Priority());

  // Check the stages.
  for (const auto& acl_table : acl_tables) {
    EXPECT_EQ(acl_table.Stage(), BCM_ACL_STAGE_IFP);
  }

  // There should be 4 unique ids: Table[0], Table[1-3], Table[4], Table[5-7]
  absl::flat_hash_set<int> physical_table_ids = {
      acl_tables[0].PhysicalTableId()};
  EXPECT_TRUE(physical_table_ids.insert(acl_tables[1].PhysicalTableId()).second)
      << "Duplicate physical table ID found for table index 1.";
  EXPECT_TRUE(physical_table_ids.insert(acl_tables[4].PhysicalTableId()).second)
      << "Duplicate physical table ID found for table index 4.";
  EXPECT_TRUE(physical_table_ids.insert(acl_tables[7].PhysicalTableId()).second)
      << "Duplicate physical table ID found for table index 7.";
  // Check common physical table ids.
  EXPECT_EQ(acl_tables[1].PhysicalTableId(), acl_tables[2].PhysicalTableId());
  EXPECT_EQ(acl_tables[2].PhysicalTableId(), acl_tables[3].PhysicalTableId());
  // Second set of common physical table ids.
  EXPECT_EQ(acl_tables[5].PhysicalTableId(), acl_tables[6].PhysicalTableId());
  EXPECT_EQ(acl_tables[6].PhysicalTableId(), acl_tables[7].PhysicalTableId());

  // Verify Hardware Tables
  const auto ifp_bcm_tables =
      SetStage(DefaultBcmAclTables(), BCM_ACL_STAGE_IFP);
  ASSERT_EQ(bcm_tables.size(), 4);

  auto bcm_table = bcm_tables.begin();
  // P4 Table 0 is inserted alone.
  BcmAclTable expected_table = ifp_bcm_tables.at(tables[0].preamble().id());
  EXPECT_THAT(*bcm_table, PartiallyUnorderedEqualsProto(expected_table));
  EXPECT_EQ(bcm_table->fields_size(), expected_table.fields_size());

  // The next physical table is derived from P4 Tables 1-3, which have mutually
  // exclusive fields. The table should have one of each unique field from the
  // input tables.
  ++bcm_table;
  absl::flat_hash_set<BcmField::Type, EnumHash<BcmField::Type>> expected_fields;
  for (int i = 1; i < 4; ++i) {
    SCOPED_TRACE(absl::StrCat("Failed verification against table index ", i));
    expected_table = ifp_bcm_tables.at(tables[i].preamble().id());
    EXPECT_THAT(*bcm_table, PartiallyUnorderedEqualsProto(expected_table));
    for (const BcmField& field : expected_table.fields()) {
      expected_fields.insert(field.type());
    }
  }
  // Compare the fields.
  absl::flat_hash_set<BcmField::Type, EnumHash<BcmField::Type>> bcm_fields;
  for (const BcmField& field : bcm_table->fields()) {
    bcm_fields.insert(field.type());
  }
  EXPECT_THAT(bcm_fields, UnorderedElementsAreArray(expected_fields));

  // P4 Table 4 is inserted alone.
  ++bcm_table;
  expected_table = ifp_bcm_tables.at(tables[4].preamble().id());
  EXPECT_THAT(*bcm_table, PartiallyUnorderedEqualsProto(expected_table));
  EXPECT_EQ(bcm_table->fields_size(), expected_table.fields_size());

  // The next physical table is derived from P4 Tables 5-7, which have mutually
  // exclusive fields.
  ++bcm_table;
  expected_fields.clear();
  for (int i = 5; i < 8; ++i) {
    SCOPED_TRACE(absl::StrCat("Failed verification against table index ", i));
    expected_table = ifp_bcm_tables.at(tables[i].preamble().id());
    EXPECT_THAT(*bcm_table, PartiallyUnorderedEqualsProto(expected_table));
    for (const BcmField& field : expected_table.fields()) {
      expected_fields.insert(field.type());
    }
  }
  // Compare the fields.
  bcm_fields.clear();
  for (const BcmField& field : bcm_table->fields()) {
    bcm_fields.insert(field.type());
  }
  EXPECT_THAT(bcm_fields, UnorderedElementsAreArray(expected_fields));
}

TEST_F(BcmAclManagerTest, TestPushForwardingPipelineConfig_SplitLinearStages) {
  // Generate the control block using the default table set.
  mock_tables_ = DefaultP4Tables();

  // Create a single ordered list for each stage for the tables.
  static constexpr int kMinimumRequiredTables = 9;
  ASSERT_GE(DefaultP4Tables().size(), kMinimumRequiredTables)
      << "There are not enough default tables for this test.";

  std::vector<::p4::config::v1::Table> ifp_tables;
  std::vector<::p4::config::v1::Table> vfp_tables;
  std::vector<::p4::config::v1::Table> efp_tables;
  int i = 0;
  for (const auto& pair : DefaultP4Tables()) {
    if (i < DefaultP4Tables().size() / 3) {
      ifp_tables.push_back(pair.second);
    } else if (i < DefaultP4Tables().size() * 2 / 3) {
      vfp_tables.push_back(pair.second);
    } else {
      efp_tables.push_back(pair.second);
    }
    ++i;
  }

  // Create each stage within the control block with sequential tables.
  ControlBlockHelper control_block_helper;
  control_block_helper.stage(P4Annotation::INGRESS_ACL);
  for (const ::p4::config::v1::Table& table : ifp_tables) {
    control_block_helper.append(table);
  }
  control_block_helper.stage(P4Annotation::VLAN_ACL);
  for (const ::p4::config::v1::Table& table : vfp_tables) {
    control_block_helper.append(table);
  }
  control_block_helper.stage(P4Annotation::EGRESS_ACL);
  for (const ::p4::config::v1::Table& table : efp_tables) {
    control_block_helper.append(table);
  }
  P4ControlBlock control_block = control_block_helper();

  // Expect to install each table into the BcmTableManager.
  EXPECT_CALL(*bcm_table_manager_mock_, AddAclTable(_))
      .Times(DefaultP4Tables().size());

  // Expect to install each table separately into the SDK. We will mock the Bcm
  // table IDs as 100 + the P4 table ID (e.g. P4 ID 1 --> BCM ID 101).
  for (const ::p4::config::v1::Table& table : ifp_tables) {
    int id = table.preamble().id();
    BcmAclTable ifp_bcm_table = DefaultBcmAclTables().at(id);
    ifp_bcm_table.set_stage(BCM_ACL_STAGE_IFP);
    EXPECT_CALL(
        *bcm_sdk_mock_,
        CreateAclTable(kUnit, PartiallyUnorderedEqualsProto(ifp_bcm_table)))
        .WillOnce(Return(100 + id));
  }
  for (const ::p4::config::v1::Table& table : vfp_tables) {
    int id = table.preamble().id();
    BcmAclTable vfp_bcm_table = DefaultBcmAclTables().at(id);
    vfp_bcm_table.set_stage(BCM_ACL_STAGE_VFP);
    EXPECT_CALL(
        *bcm_sdk_mock_,
        CreateAclTable(kUnit, PartiallyUnorderedEqualsProto(vfp_bcm_table)))
        .WillOnce(Return(100 + id));
  }
  for (const ::p4::config::v1::Table& table : efp_tables) {
    int id = table.preamble().id();
    BcmAclTable efp_bcm_table = DefaultBcmAclTables().at(id);
    efp_bcm_table.set_stage(BCM_ACL_STAGE_EFP);
    EXPECT_CALL(
        *bcm_sdk_mock_,
        CreateAclTable(kUnit, PartiallyUnorderedEqualsProto(efp_bcm_table)))
        .WillOnce(Return(100 + id));
  }

  SCOPED_TRACE(
      absl::StrCat("Input control block:\n", control_block.DebugString()));

  // Set up the P4 ForwardingPipelineConfig object.
  P4PipelineConfig p4_pipeline_config;
  *p4_pipeline_config.add_p4_controls()->mutable_main() = control_block;
  p4_pipeline_config.mutable_p4_controls(0)->set_name("test_control");
  ::p4::v1::ForwardingPipelineConfig forwarding_pipeline_config;
  p4_pipeline_config.SerializeToString(
      forwarding_pipeline_config.mutable_p4_device_config());

  // Push the forwarding pipeline config (invoke the unit under test).
  ASSERT_OK(bcm_acl_manager_->PushForwardingPipelineConfig(
      forwarding_pipeline_config));

  // Check the software tables.
  int previous_priority = 0;
  EXPECT_EQ(bcm_table_manager_->GetAllAclTableIDs().size(),
            DefaultP4Tables().size());
  for (const auto& table : ifp_tables) {
    SCOPED_TRACE(absl::StrCat("Failed verification for ifp table: ",
                              table.preamble().ShortDebugString()));
    const AclTable* acl_table;
    ASSERT_OK_AND_ASSIGN(acl_table, bcm_table_manager_->GetReadOnlyAclTable(
                                        table.preamble().id()));
    EXPECT_GT(acl_table->Priority(), previous_priority);
    previous_priority = acl_table->Priority();
    EXPECT_EQ(acl_table->PhysicalTableId(), table.preamble().id() + 100);
    EXPECT_EQ(acl_table->Stage(), BCM_ACL_STAGE_IFP);
  }
  // Priority only matters within a stage.
  previous_priority = 0;
  for (const auto& table : vfp_tables) {
    SCOPED_TRACE(absl::StrCat("Failed verification for vfp table: ",
                              table.preamble().ShortDebugString()));
    const AclTable* acl_table;
    ASSERT_OK_AND_ASSIGN(acl_table, bcm_table_manager_->GetReadOnlyAclTable(
                                        table.preamble().id()));
    EXPECT_GT(acl_table->Priority(), previous_priority);
    previous_priority = acl_table->Priority();
    EXPECT_EQ(acl_table->PhysicalTableId(), table.preamble().id() + 100);
    EXPECT_EQ(acl_table->Stage(), BCM_ACL_STAGE_VFP);
  }
  previous_priority = 0;
  for (const auto& table : efp_tables) {
    SCOPED_TRACE(absl::StrCat("Failed verification for efp table: ",
                              table.preamble().ShortDebugString()));
    const AclTable* acl_table;
    ASSERT_OK_AND_ASSIGN(acl_table, bcm_table_manager_->GetReadOnlyAclTable(
                                        table.preamble().id()));
    EXPECT_GT(acl_table->Priority(), previous_priority);
    previous_priority = acl_table->Priority();
    EXPECT_EQ(acl_table->PhysicalTableId(), table.preamble().id() + 100);
    EXPECT_EQ(acl_table->Stage(), BCM_ACL_STAGE_EFP);
  }
}

// Set up for a pipeline with no ACL tables should not result in any tables
// being setup.
TEST_F(BcmAclManagerTest, TestPushForwardingPipelineConfig_NoACLTables) {
  // Generate the control block using the default table set.
  mock_tables_ = DefaultP4Tables();

  // Generate a pipeline with all non-ACL tables.
  ControlBlockHelper control_block_helper;
  control_block_helper.stage(P4Annotation::L3_LPM);
  for (const auto& pair : DefaultP4Tables()) {
    control_block_helper.append(pair.second);
  }
  P4ControlBlock control_block = control_block_helper();
  SCOPED_TRACE(
      absl::StrCat("Input control block:\n", control_block.DebugString()));

  // Expect no HW and no SW installations.
  EXPECT_CALL(*bcm_table_manager_mock_, AddAclTable(_)).Times(0);
  EXPECT_CALL(*bcm_sdk_mock_, CreateAclTable(kUnit, _)).Times(0);

  // Set up the P4 ForwardingPipelineConfig object.
  P4PipelineConfig p4_pipeline_config;
  *p4_pipeline_config.add_p4_controls()->mutable_main() = control_block;
  p4_pipeline_config.mutable_p4_controls(0)->set_name("test_control");
  ::p4::v1::ForwardingPipelineConfig forwarding_pipeline_config;
  p4_pipeline_config.SerializeToString(
      forwarding_pipeline_config.mutable_p4_device_config());

  // Push the forwarding pipeline config (invoke the unit under test).
  ASSERT_OK(bcm_acl_manager_->PushForwardingPipelineConfig(
      forwarding_pipeline_config));
  EXPECT_THAT(bcm_table_manager_->GetAllAclTableIDs(), IsEmpty());
}

// Set up for an invalid pipeline should fail.
TEST_F(BcmAclManagerTest, TestPushForwardingPipelineConfig_PipelineFailure) {
  // Generate the control block using the default table set.
  mock_tables_ = DefaultP4Tables();

  // Generate a pipeline with bad references.
  ::p4::config::v1::Table table = DefaultP4Tables().begin()->second;
  table.mutable_preamble()->set_id(99999);

  ControlBlockHelper control_block;
  control_block.stage(P4Annotation::INGRESS_ACL).append(table);
  ::p4::v1::ForwardingPipelineConfig forwarding_pipeline_config =
      BuildForwardingPipelineConfig(control_block());

  SCOPED_TRACE(
      absl::StrCat("Input control block:\n", control_block().DebugString()));

  // Expect no HW and no SW installations.
  EXPECT_CALL(*bcm_table_manager_mock_, AddAclTable(_)).Times(0);
  EXPECT_CALL(*bcm_sdk_mock_, CreateAclTable(kUnit, _)).Times(0);

  // Push the forwarding pipeline config (invoke the unit under test).
  EXPECT_THAT(
      bcm_acl_manager_->PushForwardingPipelineConfig(
          forwarding_pipeline_config),
      StatusIs(StratumErrorSpace(), ERR_ENTRY_NOT_FOUND, HasSubstr("99999")));
  EXPECT_THAT(bcm_table_manager_->GetAllAclTableIDs(), IsEmpty());
}

// Set up should fail if the hardware does not return a good status.
TEST_F(BcmAclManagerTest, TestPushForwardingPipelineConfig_HardwareFailure) {
  // Generate the control block using the default table set.
  mock_tables_ = DefaultP4Tables();

  // Generate a pipeline with bad references.
  ::p4::config::v1::Table table = DefaultP4Tables().begin()->second;
  ControlBlockHelper control_block;
  control_block.stage(P4Annotation::INGRESS_ACL).append(table);
  ::p4::v1::ForwardingPipelineConfig forwarding_pipeline_config =
      BuildForwardingPipelineConfig(control_block());

  SCOPED_TRACE(
      absl::StrCat("Input control block:\n", control_block().DebugString()));

  // Mock the failed hardware insertion.
  EXPECT_CALL(*bcm_sdk_mock_, CreateAclTable(kUnit, _))
      .WillOnce(Return(DefaultError()));

  // Push the forwarding pipeline config (invoke the unit under test).
  EXPECT_THAT(bcm_acl_manager_->PushForwardingPipelineConfig(
                  forwarding_pipeline_config),
              DerivedFromStatus(DefaultError()));
  EXPECT_THAT(bcm_table_manager_->GetAllAclTableIDs(), IsEmpty());
}

// Pushing an identical forwarding pipeline config should result in a no-op
// success.
TEST_F(BcmAclManagerTest, TestPushForwardingPipelineConfig_IdenticalConfig) {
  // Generate the control block using the default table set.
  mock_tables_ = DefaultP4Tables();

  ::p4::v1::ForwardingPipelineConfig config =
      BuildForwardingPipelineConfig(DefaultControlBlock());
  // Expect all tables to created on the first push.
  EXPECT_CALL(*bcm_table_manager_mock_, AddAclTable(_))
      .Times(DefaultP4Tables().size());
  EXPECT_CALL(*bcm_sdk_mock_, CreateAclTable(kUnit, _)).Times(AtLeast(1));

  // Perform the push.
  EXPECT_OK(bcm_acl_manager_->PushForwardingPipelineConfig(config));

  // Expect no tables to be created on the second push.
  EXPECT_CALL(*bcm_table_manager_mock_, AddAclTable(_)).Times(0);
  EXPECT_CALL(*bcm_sdk_mock_, CreateAclTable(kUnit, _)).Times(0);
  EXPECT_OK(bcm_acl_manager_->PushForwardingPipelineConfig(config));
}

// Reconfiguring the forwarding pipeline config should clear out the current
// state and configure the new state.
TEST_F(BcmAclManagerTest, TestPushForwardingPipelineConfig_Reconfigure) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());

  constexpr int kEntriesPerTable = 2;

  EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_table_manager_mock_, AddAclTableEntry(_, _))
      .Times(AnyNumber());

  // Fill the tables.
  std::vector<::p4::v1::TableEntry> entries;
  int bcm_flow_id = 0;
  for (const auto& table : DefaultP4TablesVector()) {
    for (int i = 0; i < kEntriesPerTable; ++i) {
      ::p4::v1::TableEntry entry = BuildSimpleEntry(table, i);
      // Mock the hw responses.
      EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _))
          .WillOnce(Return(++bcm_flow_id));
      // Insert the entry.
      ASSERT_OK(bcm_acl_manager_->InsertTableEntry(entry));
      entries.push_back(entry);
    }
  }

  // We don't care exactly how BcmTableManager's state is cleared. We will check
  // that it is cleared at the end of the test.
  EXPECT_CALL(*bcm_table_manager_mock_, DeleteTableEntry(_)).Times(AnyNumber());
  EXPECT_CALL(*bcm_table_manager_mock_, DeleteTable(_)).Times(AnyNumber());

  // Expect all the table entries to be removed from hardware and software.
  for (const ::p4::v1::TableEntry& entry : entries) {
    ASSERT_OK_AND_ASSIGN(
        const AclTable* table,
        bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
    ASSERT_OK_AND_ASSIGN(int bcm_id, table->BcmAclId(entry));
    EXPECT_CALL(*bcm_sdk_mock_, RemoveAclFlow(kUnit, bcm_id))
        .WillOnce(Return(::util::OkStatus()));
  }
  // Expect all the tables to be removed from hardware and software.
  absl::flat_hash_set<int> physical_table_ids;
  for (int table_id : bcm_table_manager_->GetAllAclTableIDs()) {
    ASSERT_OK_AND_ASSIGN(const AclTable* table,
                         bcm_table_manager_->GetReadOnlyAclTable(table_id));
    physical_table_ids.insert(table->PhysicalTableId());
  }
  for (int physical_table_id : physical_table_ids) {
    EXPECT_CALL(*bcm_sdk_mock_, DestroyAclTable(kUnit, physical_table_id))
        .WillOnce(Return(::util::OkStatus()));
  }

  // Create a 4-table sequential control block.
  ASSERT_LE(4, DefaultP4TablesVector().size());
  std::vector<::p4::config::v1::Table> new_tables = DefaultP4TablesVector();
  new_tables.resize(4);
  ControlBlockHelper control_block;
  control_block.stage(P4Annotation::INGRESS_ACL);
  std::vector<int> new_table_ids;
  for (const auto& table : new_tables) {
    control_block.append(table);
    new_table_ids.push_back(table.preamble().id());
  }
  ::p4::v1::ForwardingPipelineConfig config =
      BuildForwardingPipelineConfig(control_block());
  // Expect all tables to created.
  EXPECT_CALL(*bcm_table_manager_mock_, AddAclTable(_))
      .Times(new_tables.size());
  EXPECT_CALL(*bcm_sdk_mock_, CreateAclTable(kUnit, _))
      .Times(new_tables.size());

  // Perform the push.
  EXPECT_OK(bcm_acl_manager_->PushForwardingPipelineConfig(config));

  // Compare the expected software state.
  EXPECT_THAT(bcm_table_manager_->GetAllAclTableIDs(),
              UnorderedElementsAreArray(new_table_ids));
}

TEST_F(BcmAclManagerTest, TestInsertTableEntry) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());

  constexpr int kEntriesPerTable = 8;

  // Fill the tables.
  std::vector<::p4::v1::TableEntry> entries;
  int bcm_flow_id = 0;
  for (const auto& table : DefaultP4TablesVector()) {
    for (int i = 0; i < kEntriesPerTable; ++i) {
      ::p4::v1::TableEntry entry = BuildSimpleEntry(table, i);
      // Mock the CommonFlowEntry conversion.
      BcmFlowEntry bfe;
      bfe.set_priority(i);
      EXPECT_CALL(
          *bcm_table_manager_mock_,
          FillBcmFlowEntry(EqualsProto(entry), ::p4::v1::Update::INSERT, _))
          .WillOnce(DoAll(SetArgPointee<2>(bfe), Return(::util::OkStatus())));
      // Mock the conversion & hw responses.
      EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(kUnit, EqualsProto(bfe), _, _))
          .WillOnce(Return(++bcm_flow_id));
      // Invoke the real InsertTableEntry.
      EXPECT_CALL(*bcm_table_manager_mock_,
                  AddAclTableEntry(EqualsProto(entry), bcm_flow_id))
          .Times(1);
      // Insert the entry.
      ASSERT_OK(bcm_acl_manager_->InsertTableEntry(entry));
      entries.push_back(entry);
    }
  }

  // Expect that the software state is correct.
  bcm_flow_id = 0;
  for (const ::p4::v1::TableEntry& entry : entries) {
    ASSERT_OK_AND_ASSIGN(
        const AclTable* table,
        bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
    ASSERT_THAT(table->Lookup(entry), IsOkAndHolds(EqualsProto(entry)));
    ASSERT_THAT(table->BcmAclId(entry), IsOkAndHolds(++bcm_flow_id));
  }
}

// InsertTableEntry should fail if the conversion fails and not install
// the entry to hardware.
TEST_F(BcmAclManagerTest, TestInsertTableEntryConversionFailures) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);

  // No table should be added to hardware for conversion failures.
  EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _)).Times(0);
  EXPECT_CALL(*bcm_table_manager_mock_, AddTableEntry(_)).Times(0);

  EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
      .WillOnce(Return(DefaultError()));
  EXPECT_THAT(bcm_acl_manager_->InsertTableEntry(entry),
              DerivedFromStatus(DefaultError()));
}

// InsertTableEntry should fail if installing to hardware fails.
TEST_F(BcmAclManagerTest, TestInsertTableEntryHardwareFailure) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);

  EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bcm_table_manager_mock_, AddTableEntry(_)).Times(0);
  EXPECT_THAT(bcm_acl_manager_->InsertTableEntry(entry),
              DerivedFromStatus(DefaultError()));
}

// InsertTableEntry should fail if the table entry is rejected and not install
// the hardware to hardware.
TEST_F(BcmAclManagerTest, TestInsertTableEntryRejectionFailure) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());
  ::p4::v1::TableEntry entry;
  entry.set_table_id(9999999);  // Unknown table id.

  EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_table_manager_mock_, CommonFlowEntryToBcmFlowEntry(_, _, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _)).Times(0);
  EXPECT_CALL(*bcm_table_manager_mock_, AddTableEntry(_)).Times(0);
  EXPECT_THAT(bcm_acl_manager_->InsertTableEntry(entry),
              StatusIs(StratumErrorSpace(), ERR_ENTRY_NOT_FOUND,
                       HasSubstr("9999999")));
}

TEST_F(BcmAclManagerTest, TestModifyTableEntry) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());

  constexpr int kEntriesPerTable = 8;

  // Fill the tables.
  int bcm_flow_id = 0;
  for (const auto& table : DefaultP4TablesVector()) {
    for (int i = 0; i < kEntriesPerTable; ++i) {
      ::p4::v1::TableEntry entry = BuildSimpleEntry(table, i);
      // Mock the conversions.
      EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _))
          .WillOnce(Return(++bcm_flow_id));
      EXPECT_CALL(*bcm_table_manager_mock_, AddAclTableEntry(_, _)).Times(1);
      // Insert the entry.
      ASSERT_OK(bcm_acl_manager_->InsertTableEntry(entry));
    }
  }

  // Modify the entries.
  bcm_flow_id = 0;
  std::vector<::p4::v1::TableEntry> entries;
  for (const auto& table : DefaultP4TablesVector()) {
    for (int i = 0; i < kEntriesPerTable; ++i) {
      // Create the entries from above, but with different actions.
      ::p4::v1::TableEntry entry = BuildSimpleEntry(table, i);
      entry.mutable_action()->mutable_action()->set_action_id(i);
      // Mock the CommonFlowEntry conversion.
      BcmFlowEntry bfe;
      bfe.set_priority(i);
      EXPECT_CALL(
          *bcm_table_manager_mock_,
          FillBcmFlowEntry(EqualsProto(entry), ::p4::v1::Update::MODIFY, _))
          .WillOnce(DoAll(SetArgPointee<2>(bfe), Return(::util::OkStatus())));
      // Mock the conversion & hw responses.
      EXPECT_CALL(*bcm_sdk_mock_,
                  ModifyAclFlow(kUnit, ++bcm_flow_id, EqualsProto(bfe)))
          .WillOnce(Return(::util::OkStatus()));
      EXPECT_CALL(*bcm_table_manager_mock_,
                  UpdateTableEntry(EqualsProto(entry)))
          .WillOnce(Invoke(bcm_table_manager_.get(),
                           &BcmTableManager::UpdateTableEntry));
      // Modify the entry.
      ASSERT_OK(bcm_acl_manager_->ModifyTableEntry(entry));
      entries.push_back(entry);
    }
  }

  // Expect that the software state is correct.
  bcm_flow_id = 0;
  for (const ::p4::v1::TableEntry& entry : entries) {
    ASSERT_OK_AND_ASSIGN(
        const AclTable* table,
        bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
    ASSERT_THAT(table->Lookup(entry), IsOkAndHolds(EqualsProto(entry)));
    ASSERT_THAT(table->BcmAclId(entry), IsOkAndHolds(++bcm_flow_id));
  }
}

// Modifying a table entry should fail if the lookup fails.
TEST_F(BcmAclManagerTest, TestModifyTableEntryLookupFailure) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);

  // Conversions are okay.
  EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_table_manager_mock_, CommonFlowEntryToBcmFlowEntry(_, _, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  // The hardware installation should not occur.
  EXPECT_CALL(*bcm_sdk_mock_, ModifyAclFlow(_, _, _)).Times(0);

  ASSERT_FALSE(bcm_acl_manager_->ModifyTableEntry(entry).ok());

  // The table should be unchanged.
  ASSERT_OK_AND_ASSIGN(const AclTable* table,
                       bcm_table_manager_->GetReadOnlyAclTable(
                           DefaultP4Tables().begin()->first));
  EXPECT_EQ(table->EntryCount(), 0);
}

// Modifying a table entry should fail if the conversion fails.
TEST_F(BcmAclManagerTest, TestModifyTableEntryConversionFailure) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());

  // Set up the entry.
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);

  // Insert the original entry.
  EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _)).WillOnce(Return(1));
  ASSERT_OK(bcm_acl_manager_->InsertTableEntry(entry));

  // Modify the entry action.
  ::p4::v1::TableEntry modified_entry = entry;
  modified_entry.mutable_action()->mutable_action()->set_action_id(10);

  // Fake conversion failures.
  EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
      .WillOnce(Return(DefaultError()))
      .WillRepeatedly(Return(::util::OkStatus()));
  // The hardware installation should not occur.
  EXPECT_CALL(*bcm_sdk_mock_, ModifyAclFlow(_, _, _)).Times(0);

  // The modify should fail for the conversion.
  ASSERT_FALSE(bcm_acl_manager_->ModifyTableEntry(modified_entry).ok());

  // The table should be unchanged.
  ASSERT_OK_AND_ASSIGN(
      const AclTable* table,
      bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
  EXPECT_THAT(table->Lookup(modified_entry), IsOkAndHolds(EqualsProto(entry)));
  EXPECT_EQ(table->EntryCount(), 1);

  // The next one should pass.
  EXPECT_CALL(*bcm_sdk_mock_, ModifyAclFlow(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  ASSERT_OK(bcm_acl_manager_->ModifyTableEntry(modified_entry));

  // Verify the software state.
  EXPECT_THAT(table->Lookup(modified_entry),
              IsOkAndHolds(EqualsProto(modified_entry)));
  EXPECT_EQ(table->EntryCount(), 1);
}

// Modifying a table entry should fail if the lookup fails.
TEST_F(BcmAclManagerTest, TestModifyTableEntryHardwareFailure) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());

  // Set up the entry.
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);

  // Insert the original entry.
  EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _)).WillOnce(Return(1));
  ASSERT_OK(bcm_acl_manager_->InsertTableEntry(entry));

  // Modify the entry action.
  ::p4::v1::TableEntry modified_entry = entry;
  modified_entry.mutable_action()->mutable_action()->set_action_id(10);

  // Fake the hardware failure.
  EXPECT_CALL(*bcm_sdk_mock_, ModifyAclFlow(_, _, _))
      .WillOnce(Return(DefaultError()));
  ASSERT_FALSE(bcm_acl_manager_->ModifyTableEntry(modified_entry).ok());

  // The table should be unchanged.
  ASSERT_OK_AND_ASSIGN(
      const AclTable* table,
      bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
  EXPECT_THAT(table->Lookup(modified_entry), IsOkAndHolds(EqualsProto(entry)));
  EXPECT_EQ(table->EntryCount(), 1);

  // The next one should pass.
  EXPECT_CALL(*bcm_sdk_mock_, ModifyAclFlow(_, _, _))
      .WillOnce(Return(::util::OkStatus()));

  // Verify the software state.
  ASSERT_OK(bcm_acl_manager_->ModifyTableEntry(modified_entry));
  EXPECT_THAT(table->Lookup(modified_entry),
              IsOkAndHolds(EqualsProto(modified_entry)));
  EXPECT_EQ(table->EntryCount(), 1);
}

// Deletion should work as long as the flow lookup & bcm operations succeed.
TEST_F(BcmAclManagerTest, TestDeleteTableEntry) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());
  EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  constexpr int kEntriesPerTable = 8;
  // Fill the tables.
  int bcm_flow_id = 0;
  std::vector<::p4::v1::TableEntry> entries;
  for (const auto& table : DefaultP4TablesVector()) {
    for (int i = 0; i < kEntriesPerTable; ++i) {
      ::p4::v1::TableEntry entry = BuildSimpleEntry(table, i);
      // Mock the conversions.
      EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _))
          .WillOnce(Return(++bcm_flow_id));
      EXPECT_CALL(*bcm_table_manager_mock_, AddAclTableEntry(_, _)).Times(1);
      // Insert the entry.
      ASSERT_OK(bcm_acl_manager_->InsertTableEntry(entry));
      entries.push_back(entry);
    }
  }

  // Expect each allocated bcm_flow_id to be removed from hardware.
  for (int id = 1; id <= bcm_flow_id; ++id) {
    EXPECT_CALL(*bcm_sdk_mock_, RemoveAclFlow(kUnit, id))
        .WillOnce(Return(::util::OkStatus()));
  }

  // Delete the entries and make sure the state is updated.
  for (const auto& entry : entries) {
    ASSERT_OK_AND_ASSIGN(
        const AclTable* table,
        bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
    int pre_delete_table_size = table->EntryCount();
    EXPECT_OK(table->Lookup(entry).status());
    ASSERT_OK(bcm_acl_manager_->DeleteTableEntry(entry));
    EXPECT_EQ(table->EntryCount(), pre_delete_table_size - 1);
    EXPECT_FALSE(table->Lookup(entry).ok());
  }
}

// Deletion should fail if the table entry lookup fails.
TEST_F(BcmAclManagerTest, TestDeleteTableEntryLookupFailure) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);
  // No hardware operations are expected.
  EXPECT_CALL(*bcm_sdk_mock_, RemoveAclFlow(_, _)).Times(0);
  EXPECT_THAT(bcm_acl_manager_->DeleteTableEntry(entry),
              StatusIs(StratumErrorSpace(), ERR_ENTRY_NOT_FOUND, _));
}

// Deletion should fail if the hardware operation fails.
TEST_F(BcmAclManagerTest, TestDeleteTableEntryHardwareFailure) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);
  // Insert the entry.
  EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _)).WillOnce(Return(1));
  EXPECT_OK(bcm_acl_manager_->InsertTableEntry(entry));
  // Mock the hardware failure.
  EXPECT_CALL(*bcm_sdk_mock_, RemoveAclFlow(_, _))
      .WillOnce(Return(DefaultError()));
  // Attempt to delete the entry.
  EXPECT_FALSE(bcm_acl_manager_->DeleteTableEntry(entry).ok());
}

// Stats retrieval should succeed as long as flow lookup and bcm operations
// succeed.
TEST_F(BcmAclManagerTest, TestGetTableEntryStats) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);
  EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _)).WillOnce(Return(100));
  EXPECT_OK(bcm_acl_manager_->InsertTableEntry(entry));

  BcmAclStats stats;
  stats.mutable_total()->set_bytes(1024);
  stats.mutable_total()->set_packets(8);
  EXPECT_CALL(*bcm_sdk_mock_, GetAclStats(kUnit, 100, _))
      .WillOnce(DoAll(SetArgPointee<2>(stats), Return(::util::OkStatus())));
  ::p4::v1::CounterData received, expected;
  EXPECT_OK(bcm_acl_manager_->GetTableEntryStats(entry, &received));
  expected.set_byte_count(1024);
  expected.set_packet_count(8);
  EXPECT_TRUE(ProtoEqual(expected, received));
}

// Stats retrieval should fail if the flow lookup fails.
TEST_F(BcmAclManagerTest, TestGetTableEntryStatsLookupFailure) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);
  ::p4::v1::CounterData counter;
  EXPECT_CALL(*bcm_sdk_mock_, GetAclStats(_, _, _)).Times(0);
  EXPECT_FALSE(bcm_acl_manager_->GetTableEntryStats(entry, &counter).ok());
}

// Stats retrieval should fail if the Bcm operation fails.
TEST_F(BcmAclManagerTest, TestGetTableEntryStatsBcmFailure) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);
  EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _)).WillOnce(Return(100));
  EXPECT_OK(bcm_acl_manager_->InsertTableEntry(entry));

  BcmAclStats stats;
  stats.mutable_total()->set_bytes(1024);
  stats.mutable_total()->set_packets(8);
  EXPECT_CALL(*bcm_sdk_mock_, GetAclStats(kUnit, 100, _))
      .WillOnce(DoAll(SetArgPointee<2>(stats), Return(DefaultError())));
  ::p4::v1::CounterData counter;
  EXPECT_FALSE(bcm_acl_manager_->GetTableEntryStats(entry, &counter).ok());
}

// Meter configuration should succeed as long as flow lookup and bcm operations
// succeed.
TEST_F(BcmAclManagerTest, TestUpdateTableEntryMeter) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);
  EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _)).WillOnce(Return(100));
  EXPECT_OK(bcm_acl_manager_->InsertTableEntry(entry));

  // Test valid meter configuration.
  ::p4::v1::DirectMeterEntry p4_meter;
  *p4_meter.mutable_table_entry() = entry;
  p4_meter.mutable_config()->set_cir(512);
  p4_meter.mutable_config()->set_cburst(8);
  p4_meter.mutable_config()->set_pir(1024);
  p4_meter.mutable_config()->set_pburst(8);
  // Translated meter configuration.
  BcmMeterConfig bcm_meter;
  bcm_meter.set_committed_rate(512);
  bcm_meter.set_committed_burst(8);
  bcm_meter.set_peak_rate(1024);
  bcm_meter.set_peak_burst(8);
  EXPECT_CALL(*bcm_sdk_mock_, SetAclPolicer(kUnit, 100, EqualsProto(bcm_meter)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_table_manager_mock_,
              UpdateTableEntryMeter(EqualsProto(p4_meter)))
      .Times(1);
  EXPECT_OK(bcm_acl_manager_->UpdateTableEntryMeter(p4_meter));

  // Check the software state.
  ASSERT_OK_AND_ASSIGN(
      const AclTable* table,
      bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
  ASSERT_OK_AND_ASSIGN(::p4::v1::TableEntry lookup, table->Lookup(entry));
  EXPECT_THAT(lookup.meter_config(), EqualsProto(p4_meter.config()));
}

TEST_F(BcmAclManagerTest, TestUpdateTableEntryMeterLookupFailure) {
  ASSERT_OK(SetUpDefaultTables());

  // Test valid meter configuration.
  ::p4::v1::DirectMeterEntry p4_meter;
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);
  *p4_meter.mutable_table_entry() = entry;
  p4_meter.mutable_config()->set_cir(512);
  p4_meter.mutable_config()->set_cburst(8);
  p4_meter.mutable_config()->set_pir(1024);
  p4_meter.mutable_config()->set_pburst(8);

  EXPECT_CALL(*bcm_sdk_mock_, SetAclPolicer(_, _, _)).Times(0);
  EXPECT_FALSE(bcm_acl_manager_->UpdateTableEntryMeter(p4_meter).ok());
}

TEST_F(BcmAclManagerTest, TestUpdateTableEntryMeterBcmFailure) {
  // Perform the initial configuration.
  ASSERT_OK(SetUpDefaultTables());
  ::p4::v1::TableEntry entry =
      BuildSimpleEntry(*DefaultP4TablesVector().begin(), 0);
  EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InsertAclFlow(_, _, _, _)).WillOnce(Return(100));
  EXPECT_OK(bcm_acl_manager_->InsertTableEntry(entry));

  // Test valid meter configuration.
  ::p4::v1::DirectMeterEntry p4_meter;
  *p4_meter.mutable_table_entry() = entry;
  p4_meter.mutable_config()->set_cir(512);
  p4_meter.mutable_config()->set_cburst(8);
  p4_meter.mutable_config()->set_pir(1024);
  p4_meter.mutable_config()->set_pburst(8);
  EXPECT_CALL(*bcm_sdk_mock_, SetAclPolicer(_, _, _))
      .WillOnce(Return(DefaultError()));
  EXPECT_FALSE(bcm_acl_manager_->UpdateTableEntryMeter(p4_meter).ok());

  // Check the software state.
  ASSERT_OK_AND_ASSIGN(
      const AclTable* table,
      bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
  ASSERT_OK_AND_ASSIGN(::p4::v1::TableEntry lookup, table->Lookup(entry));
  EXPECT_FALSE(lookup.has_meter_config());
}

TEST_F(BcmAclManagerTest, TestInstallPhysicalTableWithConstConditions) {
  std::vector<string> table_strings = {
      R"PROTO(
        preamble { id: 1 name: "table_1" }
        match_fields { id: 1 name: "P4_FIELD_TYPE_ETH_SRC" match_type: TERNARY }
        size: 10
      )PROTO",
      R"PROTO(
        preamble { id: 2 name: "table_2" }
        match_fields { id: 1 name: "P4_FIELD_TYPE_ETH_DST" match_type: TERNARY }
        size: 10
      )PROTO",
      R"PROTO(
        preamble { id: 3 name: "table_3" }
        match_fields {
          id: 1
          name: "P4_FIELD_TYPE_IPV4_SRC"
          match_type: TERNARY
        }
        size: 10
      )PROTO"};

  std::vector<::p4::config::v1::Table> tables;
  for (const string& table_string : table_strings) {
    ::p4::config::v1::Table table;
    CHECK_OK(ParseProtoFromString(table_string, &table));
    tables.push_back(table);
  }

  P4ControlBlock control_block;
  *control_block.add_statements() =
      ApplyTable(tables[0], P4Annotation::INGRESS_ACL);
  *control_block.add_statements() =
      IsValidBuilder()
          .Header(P4HeaderType::P4_HEADER_IPV4)
          .DoIfValid(ApplyTable(tables[1], P4Annotation::INGRESS_ACL))
          .DoIfValid(ApplyTable(tables[2], P4Annotation::INGRESS_ACL))
          .Build();

  std::vector<string> bcm_table_strings = {
      R"PROTO(fields { type: ETH_SRC })PROTO",
      R"PROTO(fields { type: ETH_DST } fields { type: IP_TYPE })PROTO",
      R"PROTO(fields { type: IPV4_SRC } fields { type: IP_TYPE })PROTO",
  };
  std::vector<BcmAclTable> expected_bcm_tables;
  for (const string& table_string : bcm_table_strings) {
    BcmAclTable table;
    CHECK_OK(ParseProtoFromString(table_string, &table));
    expected_bcm_tables.push_back(table);
  }

  EXPECT_CALL(*bcm_sdk_mock_,
              CreateAclTable(
                  kUnit, PartiallyUnorderedEqualsProto(expected_bcm_tables[0])))
      .Times(1);
  EXPECT_CALL(*bcm_sdk_mock_,
              CreateAclTable(
                  kUnit, PartiallyUnorderedEqualsProto(expected_bcm_tables[1])))
      .Times(1);
  EXPECT_CALL(*bcm_sdk_mock_,
              CreateAclTable(
                  kUnit, PartiallyUnorderedEqualsProto(expected_bcm_tables[2])))
      .Times(1);

  ASSERT_OK(SetUpTables(tables, control_block));
}

TEST_F(BcmAclManagerTest, TestInsertTableEntryWithConstConditions) {
  ::p4::config::v1::Table table;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    preamble { id: 2 name: "table_2" }
    match_fields { id: 1 name: "P4_FIELD_TYPE_ETH_DST" match_type: TERNARY }
    size: 10
  )PROTO", &table));

  P4ControlBlock control_block;
  *control_block.add_statements() =
      IsValidBuilder()
          .Header(P4HeaderType::P4_HEADER_IPV4)
          .DoIfValid(ApplyTable(table, P4Annotation::INGRESS_ACL))
          .Build();

  ::p4::v1::TableEntry table_entry;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    table_id: 2
    match { field_id: 1 ternary { value: "\00A" } }
    priority: 15
  )PROTO", &table_entry));

  BcmFlowEntry bcm_flow_entry;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    bcm_table_type: BCM_TABLE_ACL
    fields { type: ETH_DST value { u32: 10 } }
    fields { type: IP_TYPE value { u32: 0x800 } }
  )PROTO", &bcm_flow_entry));

  ASSERT_OK(SetUpTables({table}, control_block));
  EXPECT_CALL(*bcm_table_manager_mock_, FillBcmFlowEntry(_, _, _))
      .WillOnce(
          DoAll(SetArgPointee<2>(bcm_flow_entry), Return(util::OkStatus())));
  EXPECT_CALL(
      *bcm_sdk_mock_,
      InsertAclFlow(_, PartiallyUnorderedEqualsProto(bcm_flow_entry), _, _))
      .WillOnce(Return(1));
  EXPECT_OK(bcm_acl_manager_->InsertTableEntry(table_entry));
}

}  // namespace
}  // namespace bcm
}  // namespace hal
}  // namespace stratum
