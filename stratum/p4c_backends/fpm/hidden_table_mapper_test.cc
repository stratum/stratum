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

// Contains HiddenTableMapper unit tests.

#include "stratum/p4c_backends/fpm/hidden_table_mapper.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "google/protobuf/util/message_differencer.h"
#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/p4/p4_info_manager_mock.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/lib/macros.h"

using ::google::protobuf::util::MessageDifferencer;
using ::testing::AnyNumber;
using ::testing::Invoke;

namespace stratum {
namespace p4c_backends {

// This unnamed namespace defines constants for use by parameterized tests.
namespace {

// Changes the tested table's match field type to something non-EXACT.
constexpr int kTableSetupNonExactMatch = 1;

// Adds an extra match field to the tested table.
constexpr int kTableSetupTooManyMatch = 2;

// Changes the tested table's pipeline stage to a non-HIDDEN value.
constexpr int kTableSetupNotHidden = 3;

// Clears the tested table's static entries flag.
constexpr int kTableSetupNoStaticEntries = 4;

}  // namespace

// The FakeP4InfoManager handles delegation of the FindTableByName method
// by the test fixture's mock_p4_info_manager_.
class FakeP4InfoManager : public hal::P4InfoManager {
 public:
  explicit FakeP4InfoManager(const ::p4::config::v1::P4Info& p4_info)
      : p4_info_(p4_info) {}

  // Tests may modify the test fixture's P4Info during test-specific setup.
  // The real P4InfoManager makes its own copy of the P4Info in its constructor.
  // To handle dynamic P4Info changes during a test, the fake class keeps a
  // P4Info reference to the constructor P4Info.  For table lookups, it always
  // does a brute force search for the table name to get the latest P4 table
  // entry with any test-dependent changes.
  ::util::StatusOr<const ::p4::config::v1::Table> FindTableByName(
      const std::string& table_name) const override {
    for (const auto& p4_table : p4_info_.tables()) {
      if (table_name == p4_table.preamble().name()) {
        return p4_table;
      }
    }
    return MAKE_ERROR(ERR_INVALID_P4_INFO) << "Table not found";
  }

 private:
  const ::p4::config::v1::P4Info& p4_info_;
};

// This class is the base HiddenTableMapper test fixture.  See
// INSTANTIATE_TEST_SUITE_P near the end of this file for parameter usage.
class HiddenTableMapperTest
    : public testing::TestWithParam<std::tuple<std::string, int>> {
 protected:
  static constexpr const char* kMetaKeyDecap = "test_meta.smaller_metadata";
  static constexpr const char* kMetaKeyEncap = "test_meta.other_metadata";
  static constexpr const char* kDecapAction1 = "ingress.set_decap_key_1";
  static constexpr const char* kDecapAction2 = "ingress.set_decap_key_2";
  static constexpr const char* kEncapAction1 = "ingress.set_encap_key_1";
  static constexpr const char* kEncapAction2 = "ingress.set_encap_key_2";

  // SetUpTestIR uses ir_helper_ to load an IR file in JSON format.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIR(kTestP4File));
  }

  // SetUpTestP4InfoAndPipeline populates the P4Info and P4PipelineConfig
  // (test_p4_info_ and test_pipeline_config_, respectively) for test use.
  // The P4Info comes from the P4 program processed by SetUpTestIR, which must
  // be called first.  SetUpTestP4InfoAndPipeline derives the P4PipelineConfig
  // from the P4Info in a form that is suitable for many tests.  Individual
  // tests can adapt test_p4_info_ and test_pipeline_config_ to specific
  // test conditions upon return.
  void SetUpTestP4InfoAndPipeline() {
    ASSERT_TRUE(ir_helper_->GenerateP4Info(&test_p4_info_));

    // This TableMapGenerator helps build a pipeline config for testing.
    // The loop below populates field descriptor data based on the match
    // fields in each table's P4Info.  Note that all fields other than
    // the one used as the hidden table metadata key are set to an arbitrary
    // type, which doesn't matter as long as it's not treated as an
    // unknown type.  Metadata key fields have metadata_keys table names
    // filled as if previously processed by MetaKeyMapper.
    TableMapGenerator table_mapper;
    for (const auto& table : test_p4_info_.tables()) {
      for (const auto& match_field : table.match_fields()) {
        table_mapper.AddField(match_field.name());
        if (match_field.name().find("test_meta") == 0) {
          const hal::P4FieldDescriptor* field_descriptor =
              FindFieldDescriptorOrNull(match_field.name(),
                                        table_mapper.generated_map());
          ASSERT_TRUE(field_descriptor != nullptr);
          hal::P4FieldDescriptor new_descriptor = *field_descriptor;
          new_descriptor.set_is_local_metadata(true);
          new_descriptor.add_metadata_keys()->set_table_name(
              table.preamble().name());
          table_mapper.ReplaceFieldDescriptor(match_field.name(),
                                              new_descriptor);
        } else {
          table_mapper.SetFieldType(match_field.name(), P4_FIELD_TYPE_ETH_DST);
        }
      }
    }

    // The table map also needs action descriptors.
    int constant_key = 0;
    for (const auto& action : test_p4_info_.actions()) {
      P4AssignSourceValue source_value;
      source_value.set_bit_width(16);
      source_value.set_constant_param(++constant_key);
      if (action.preamble().name().find("set_decap_key") != std::string::npos) {
        table_mapper.AddAction(action.preamble().name());
        table_mapper.AssignActionSourceValueToField(
            action.preamble().name(), source_value, kMetaKeyDecap);
      }
      if (action.preamble().name().find("set_encap_key") != std::string::npos) {
        table_mapper.AddAction(action.preamble().name());
        table_mapper.AssignActionSourceValueToField(
            action.preamble().name(), source_value, kMetaKeyEncap);
      }
    }

    // The TableMapGenerator doesn't support every attribute needed to set
    // up tables for these tests, so the loop below brute forces the
    // necessary table descriptors into test_pipeline_config_.
    test_pipeline_config_ = table_mapper.generated_map();
    for (const auto& table : test_p4_info_.tables()) {
      for (const auto& annotation : table.preamble().annotations()) {
        if (annotation.find("pipeline_stage") == std::string::npos) continue;
        hal::P4TableMapValue new_table;
        P4Annotation::PipelineStage stage = P4Annotation::VLAN_ACL;
        if (annotation.find("HIDDEN") != std::string::npos) {
          new_table.mutable_table_descriptor()->set_has_static_entries(true);
          stage = P4Annotation::HIDDEN;
          CHECK_EQ(1, table.match_fields_size());
          if (table.preamble().name().find("encap") != std::string::npos)
            expected_hidden_encap_tables_.insert(table.preamble().name());
          else
            expected_hidden_decap_tables_.insert(table.preamble().name());
        }
        new_table.mutable_table_descriptor()->set_pipeline_stage(stage);
        (*test_pipeline_config_.mutable_table_map())[table.preamble().name()] =
            new_table;
      }
    }

    // The mock_p4_info_manager_ delegates all FindTableByName queries to
    // the fake_p4_info_manager_ below.
    fake_p4_info_manager_ = absl::make_unique<FakeP4InfoManager>(test_p4_info_);
    ON_CALL(mock_p4_info_manager_, FindTableByName)
        .WillByDefault(Invoke(fake_p4_info_manager_.get(),
                              &FakeP4InfoManager::FindTableByName));

    // HiddenTableMapper ignores a P4PipelineConfig that has no static
    // table entries, so a dummy update is added below.  The update content
    // currently doesn't matter to any tests.
    test_pipeline_config_.mutable_static_table_entries()->add_updates();
    original_pipeline_config_ = test_pipeline_config_;
  }

  // Returns the P4FieldDescriptor for the input field name.
  hal::P4FieldDescriptor* GetFieldDescriptorOrDie(const std::string& name) {
    hal::P4FieldDescriptor* descriptor =
        FindMutableFieldDescriptorOrNull(name, &test_pipeline_config_);
    CHECK(descriptor != nullptr);
    return descriptor;
  }

  // Returns the P4TableDescriptor for the input table name.
  hal::P4TableDescriptor* GetTableDescriptorOrDie(const std::string& name) {
    return FindMutableTableDescriptorOrDie(name, &test_pipeline_config_);
  }

  // Returns the P4ActionDescriptor for the input action name.
  hal::P4ActionDescriptor* GetActionDescriptorOrDie(const std::string& name) {
    return FindMutableActionDescriptorOrDie(name, &test_pipeline_config_);
  }

  // Returns a pointer to the test_p4_info_ table with the given name.
  p4::config::v1::Table* GetP4InfoTableOrDie(const std::string& table_name) {
    for (auto& p4_table : *test_p4_info_.mutable_tables()) {
      if (p4_table.preamble().name() == table_name)
        return &p4_table;
    }
    LOG(FATAL) << "Table " << table_name << " does not exist in test_p4_info_";
    return nullptr;
  }

  // Causes test failure if test_pipeline_config_ differs from
  // original_pipeline_config.
  void ExpectNoP4PipelineConfigChanges(
      const hal::P4PipelineConfig& original_pipeline_config) {
    MessageDifferencer msg_differencer;
    msg_differencer.set_repeated_field_comparison(
        google::protobuf::util::MessageDifferencer::AS_SET);
    if (!msg_differencer.Compare(
        original_pipeline_config, test_pipeline_config_)) {
      FAIL() << "Unexpected change in P4PipelineConfig";
    }
  }

  // Verifies no changes to metadata_keys in field descriptors.  With the
  // addition of MetaKeyMapper, HiddenTableMapper should treat the metadata_keys
  // as immutable.
  void ExpectUnchangedMetadataKeys() {
    for (const auto& table_map_iter : test_pipeline_config_.table_map()) {
      if (table_map_iter.second.has_field_descriptor()) {
        const hal::P4FieldDescriptor& new_field_descriptor =
            table_map_iter.second.field_descriptor();
        const hal::P4FieldDescriptor* old_field_descriptor =
            FindFieldDescriptorOrNull(table_map_iter.first,
                                      original_pipeline_config_);
        ASSERT_TRUE(old_field_descriptor != nullptr);
        EXPECT_EQ(old_field_descriptor->metadata_keys_size(),
                  new_field_descriptor.metadata_keys_size());
      }
    }
  }

  // Verifies that the redirect_map has an entry for action_name that refers
  // to field_name as its key_field_name.  The entry should include all
  // tables in expected_table_names.
  void ExpectActionRedirects(
      const HiddenTableMapper::ActionRedirectMap& redirect_map,
      const std::string& action_name, const std::string& field_name,
      const std::set<std::string>& expected_table_names) {
    const hal::P4ActionDescriptor* descriptor =
        gtl::FindOrNull(redirect_map, action_name);
    if (descriptor == nullptr) {
      FAIL() << "Missing ActionRedirectMap entry for " << action_name;
    }
    bool field_found = false;
    std::set<std::string> redirected_table_set;
    for (const auto& redirect : descriptor->action_redirects()) {
      if (redirect.key_field_name() == field_name) {
        field_found = true;
        EXPECT_NE(0, redirect.key_value());
        for (const auto& internal_link : redirect.internal_links()) {
          redirected_table_set.insert(internal_link.hidden_table_name());
        }
      }
    }
    if (!field_found) {
      FAIL() << "Action " << action_name << " has no redirect for field "
             << field_name;
    }
    EXPECT_EQ(redirected_table_set, expected_table_names);
  }

  // Returns true if at least one action descriptor in the redirect_map
  // has an assignment to field_name.
  bool ActionsHaveFieldAssignments(
      const HiddenTableMapper::ActionRedirectMap& redirect_map,
      const std::string& field_name) {
    for (const auto& map_entry : redirect_map) {
      for (const auto& assignment : map_entry.second.assignments()) {
        if (!assignment.destination_field_name().empty()) {
          if (field_name == assignment.destination_field_name())
            return true;
        }
      }
    }
    return false;
  }

  // Verifies metadata key consistency between the input redirect_map and
  // corresponding metadata_keys entries in pipeline config field descriptors.
  // TODO: This code might be useful in p4_config_verifier.
  void ExpectMetaDataKeyConsistency(
      const HiddenTableMapper::ActionRedirectMap& redirect_map) {
    // These sets accumulate field and table references from action descriptors
    // for later cross-checking against field descriptors.
    std::set<std::string> meta_keys_in_actions;
    std::set<std::string> hidden_tables_in_actions;

    // Every action in the redirect_map should have a key_field_name that
    // refers to a known field descriptor, and every hidden table referenced
    // by the action should correspond to a metadata_keys entry in the field
    // descriptor.
    for (const auto& map_entry : redirect_map) {
      for (const auto& redirect_entry : map_entry.second.action_redirects()) {
        meta_keys_in_actions.insert(redirect_entry.key_field_name());
        const hal::P4FieldDescriptor& field_descriptor =
            *GetFieldDescriptorOrDie(redirect_entry.key_field_name());
        if (field_descriptor.type() != P4_FIELD_TYPE_METADATA_MATCH) {
          FAIL() << "Action " << map_entry.first << " redirects via match "
                 << "field " << redirect_entry.key_field_name() << ", which "
                 << "is not a metadata match field type: "
                 << field_descriptor.ShortDebugString();
        }
        for (const auto& internal_link : redirect_entry.internal_links()) {
          hidden_tables_in_actions.insert(internal_link.hidden_table_name());
          bool field_has_key = false;
          for (const auto& field_metadata_key :
               field_descriptor.metadata_keys()) {
            if (internal_link.hidden_table_name() ==
                field_metadata_key.table_name()) {
              field_has_key = true;
              break;
            }
          }
          if (!field_has_key) {
            FAIL() << "Action " << map_entry.first << " redirects to hidden "
                   << "table " << internal_link.hidden_table_name()
                   << " using field " << redirect_entry.key_field_name()
                   << " with no corresponding field descriptor metadata key: "
                   << field_descriptor.ShortDebugString();
          }
        }
      }
    }

    // The metadata_keys entries in every field descriptor with type
    // P4_FIELD_TYPE_METADATA_MATCH should refer back to match keys and
    // hidden tables from actions in the redirect_map.  Since tables in
    // some metadata_keys entries may be disqualified, all entries may not
    // appear in the redirect_map, but at least one key should.  Otherwise,
    // the field descriptor should not be P4_FIELD_TYPE_METADATA_MATCH.
    for (const auto& table_map_iter : test_pipeline_config_.table_map()) {
      if (table_map_iter.second.has_field_descriptor()) {
        const hal::P4FieldDescriptor& field_descriptor =
            table_map_iter.second.field_descriptor();
        if (field_descriptor.type() != P4_FIELD_TYPE_METADATA_MATCH) continue;
        const std::string& field = table_map_iter.first;
        if (meta_keys_in_actions.find(field) == meta_keys_in_actions.end()) {
          FAIL() << "Field " << field << " is a metadata match key, "
                 << "but it has no related action descriptor redirects";
        }
        bool at_least_one_redirect = false;
        for (const auto& field_metadata_key :
             field_descriptor.metadata_keys()) {
          if (hidden_tables_in_actions.find(field_metadata_key.table_name()) !=
              hidden_tables_in_actions.end()) {
            at_least_one_redirect = true;
          }
        }
        if (!at_least_one_redirect) {
          FAIL() << "Field " << field << " is a metadata match key for at "
                 << "least one table, but no action redirects are associated "
                 << "with this field";
        }
      }
    }
  }

  // Does test table adjustments according to test parameters.
  void SetUpParameterizedTableTest() {
    const std::string& test_table = test_table_param();
    switch (test_type_param()) {
      case kTableSetupNonExactMatch: {
        p4::config::v1::Table* p4_table = GetP4InfoTableOrDie(test_table);
        ASSERT_EQ(1, p4_table->match_fields_size());
        p4_table->mutable_match_fields(0)->set_match_type(
            p4::config::v1::MatchField::LPM);
        break;
      }

      case kTableSetupTooManyMatch: {
        p4::config::v1::Table* p4_table = GetP4InfoTableOrDie(test_table);
        ASSERT_EQ(1, p4_table->match_fields_size());
        auto extra_match = p4_table->add_match_fields();
        *extra_match = p4_table->match_fields(0);
        extra_match->set_id(2);
        extra_match->set_name("extra-match");
        break;
      }

      case kTableSetupNotHidden: {
        hal::P4TableDescriptor* descriptor =
            GetTableDescriptorOrDie(test_table);
        descriptor->set_pipeline_stage(P4Annotation::L3_LPM);
        break;
      }

      case kTableSetupNoStaticEntries: {
        hal::P4TableDescriptor* descriptor =
            GetTableDescriptorOrDie(test_table);
        descriptor->set_has_static_entries(false);
        break;
      }

      default:
        LOG(FATAL) << "Invalid table test type " << test_type_param();
        break;
    }
  }

  // Test parameter accessors.
  const std::string& test_table_param() const {
    return ::testing::get<0>(GetParam());
  }
  const int test_type_param() const {
    return ::testing::get<1>(GetParam());
  }

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.

  // The P4InfoManagerMock below delegates some calls to its
  // fake_p4_info_manager_ companion.
  testing::NiceMock<hal::P4InfoManagerMock> mock_p4_info_manager_;
  std::unique_ptr<FakeP4InfoManager> fake_p4_info_manager_;

  // SetUpTestP4InfoAndPipeline populates these members, as derived from the
  // P4 program file input to SetUpTestIR.  Upon return from
  // SetUpTestP4InfoAndPipeline, original_pipeline_config_ is an exact copy
  // of test_pipeline_config_.
  ::p4::config::v1::P4Info test_p4_info_;
  hal::P4PipelineConfig test_pipeline_config_;
  hal::P4PipelineConfig original_pipeline_config_;

  // These sets provide useful input to ExpectActionRedirects.  They are
  // populated by SetUpTestP4InfoAndPipeline.
  std::set<std::string> expected_hidden_encap_tables_;
  std::set<std::string> expected_hidden_decap_tables_;
};

// Tests HiddenTableMapper normal behavior for the tables setup exactly as
// defined in the test P4 program.
TEST_F(HiddenTableMapperTest, TestNormalHiddenTables) {
  SetUpTestIR("hidden_table1.ir.json");
  SetUpTestP4InfoAndPipeline();

  HiddenTableMapper test_hidden_mapper;
  test_hidden_mapper.ProcessTables(
      mock_p4_info_manager_, &test_pipeline_config_);

  ExpectUnchangedMetadataKeys();
  const HiddenTableMapper::ActionRedirectMap& redirect_map =
      test_hidden_mapper.action_redirects();
  EXPECT_EQ(4, redirect_map.size());
  ExpectActionRedirects(redirect_map, kDecapAction1, kMetaKeyDecap,
                        expected_hidden_decap_tables_);
  ExpectActionRedirects(redirect_map, kDecapAction2, kMetaKeyDecap,
                        expected_hidden_decap_tables_);
  ExpectActionRedirects(redirect_map, kEncapAction1, kMetaKeyEncap,
                        expected_hidden_encap_tables_);
  ExpectActionRedirects(redirect_map, kEncapAction2, kMetaKeyEncap,
                        expected_hidden_encap_tables_);
  EXPECT_FALSE(ActionsHaveFieldAssignments(redirect_map, kMetaKeyDecap));
  EXPECT_FALSE(ActionsHaveFieldAssignments(redirect_map, kMetaKeyEncap));
  ExpectMetaDataKeyConsistency(redirect_map);
}

// Tests HiddenTableMapper normal behavior.  The P4 config is modified with
// metadata key references to a non-hidden table.  The additional key usage
// should not affect the normal output.
TEST_F(HiddenTableMapperTest, TestNormalHiddenTablesWithExtraKey) {
  SetUpTestIR("hidden_table1.ir.json");
  SetUpTestP4InfoAndPipeline();

  // A primitive table with no actions or match fields works for this test.
  const std::string kNonHiddenTable = "non-hidden-ingress-table";
  auto new_p4_table = test_p4_info_.add_tables();  // Add new table in P4Info.
  new_p4_table->mutable_preamble()->set_name(kNonHiddenTable);
  new_p4_table->mutable_preamble()->set_id(87654);
  hal::P4TableMapValue new_table;
  new_table.mutable_table_descriptor()->set_pipeline_stage(
      P4Annotation::VLAN_ACL);
  (*test_pipeline_config_.mutable_table_map())[kNonHiddenTable] = new_table;

  // This test adds the new table to all field descriptors that already
  // have existing metadata keys.
  for (auto& iter : *test_pipeline_config_.mutable_table_map()) {
    if (!iter.second.has_field_descriptor()) continue;
    auto field_descriptor = iter.second.mutable_field_descriptor();
    if (field_descriptor->metadata_keys_size()) {
      field_descriptor->add_metadata_keys()->set_table_name(kNonHiddenTable);
    }
  }
  original_pipeline_config_ = test_pipeline_config_;

  HiddenTableMapper test_hidden_mapper;
  test_hidden_mapper.ProcessTables(mock_p4_info_manager_,
                                   &test_pipeline_config_);

  ExpectUnchangedMetadataKeys();
  const HiddenTableMapper::ActionRedirectMap& redirect_map =
      test_hidden_mapper.action_redirects();
  EXPECT_EQ(4, redirect_map.size());
  ExpectActionRedirects(redirect_map, kDecapAction1,
                        kMetaKeyDecap, expected_hidden_decap_tables_);
  ExpectActionRedirects(redirect_map, kDecapAction2,
                        kMetaKeyDecap, expected_hidden_decap_tables_);
  ExpectActionRedirects(redirect_map, kEncapAction1,
                        kMetaKeyEncap, expected_hidden_encap_tables_);
  ExpectActionRedirects(redirect_map, kEncapAction2,
                        kMetaKeyEncap, expected_hidden_encap_tables_);
  EXPECT_FALSE(ActionsHaveFieldAssignments(redirect_map, kMetaKeyDecap));
  EXPECT_FALSE(ActionsHaveFieldAssignments(redirect_map, kMetaKeyEncap));
  ExpectMetaDataKeyConsistency(redirect_map);
}

// Verifies that hidden table mapping produces no output when the hidden
// table keys are not local metadata.
TEST_F(HiddenTableMapperTest, TestNonMetadataKeys) {
  SetUpTestIR("hidden_table1.ir.json");
  SetUpTestP4InfoAndPipeline();
  auto field_descriptor1 = GetFieldDescriptorOrDie(kMetaKeyDecap);
  field_descriptor1->set_is_local_metadata(false);
  auto field_descriptor2 = GetFieldDescriptorOrDie(kMetaKeyEncap);
  field_descriptor2->set_is_local_metadata(false);

  original_pipeline_config_ = test_pipeline_config_;
  HiddenTableMapper test_hidden_mapper;
  test_hidden_mapper.ProcessTables(
      mock_p4_info_manager_, &test_pipeline_config_);

  // The HiddenTableMapper action_redirects output should be empty, and the
  // P4 pipeline config should be unchanged.
  EXPECT_TRUE(test_hidden_mapper.action_redirects().empty());
  ExpectNoP4PipelineConfigChanges(original_pipeline_config_);
}

// Verifies that hidden table mapping produces no output when the hidden
// table keys already have known field types.
TEST_F(HiddenTableMapperTest, TestMetadataKeyKnownFieldType) {
  SetUpTestIR("hidden_table1.ir.json");
  SetUpTestP4InfoAndPipeline();
  auto field_descriptor1 = GetFieldDescriptorOrDie(kMetaKeyDecap);
  field_descriptor1->set_type(P4_FIELD_TYPE_VRF);
  auto field_descriptor2 = GetFieldDescriptorOrDie(kMetaKeyEncap);
  field_descriptor2->set_type(P4_FIELD_TYPE_COLOR);

  original_pipeline_config_ = test_pipeline_config_;
  HiddenTableMapper test_hidden_mapper;
  test_hidden_mapper.ProcessTables(
      mock_p4_info_manager_, &test_pipeline_config_);

  // The HiddenTableMapper action_redirects output should be empty, and the
  // P4 pipeline config should be unchanged.
  EXPECT_TRUE(test_hidden_mapper.action_redirects().empty());
  ExpectNoP4PipelineConfigChanges(original_pipeline_config_);
}

// Verifies that hidden table mapping produces no output when the hidden
// table keys use non-exact match types.
TEST_F(HiddenTableMapperTest, TestMetadataKeyNoExactMatch) {
  SetUpTestIR("hidden_table1.ir.json");
  SetUpTestP4InfoAndPipeline();
  std::set<std::string> all_hidden_tables = expected_hidden_encap_tables_;
  all_hidden_tables.insert(expected_hidden_decap_tables_.begin(),
                           expected_hidden_decap_tables_.end());
  for (const auto& iter : all_hidden_tables) {
    p4::config::v1::Table* p4_table = GetP4InfoTableOrDie(iter);
    ASSERT_EQ(1, p4_table->match_fields_size());
    p4_table->mutable_match_fields(0)->set_match_type(
        p4::config::v1::MatchField::LPM);  // Changes EXACT to LPM.
  }

  HiddenTableMapper test_hidden_mapper;
  test_hidden_mapper.ProcessTables(
      mock_p4_info_manager_, &test_pipeline_config_);

  // The HiddenTableMapper action_redirects output should be empty, and the
  // P4 pipeline config should be unchanged.
  EXPECT_TRUE(test_hidden_mapper.action_redirects().empty());
  ExpectNoP4PipelineConfigChanges(original_pipeline_config_);
}

// Verifies behavior for parameterized hidden table setup variations that
// prevent normal hidden table usage.
TEST_P(HiddenTableMapperTest, TestDisqualifiedTableVariations) {
  SetUpTestIR("hidden_table1.ir.json");
  SetUpTestP4InfoAndPipeline();
  SetUpParameterizedTableTest();
  const std::string kTestTableName = test_table_param();

  HiddenTableMapper test_hidden_mapper;
  test_hidden_mapper.ProcessTables(
      mock_p4_info_manager_, &test_pipeline_config_);

  // The table under test should no longer be one of the expected hidden tables.
  expected_hidden_decap_tables_.erase(kTestTableName);
  expected_hidden_encap_tables_.erase(kTestTableName);
  ExpectUnchangedMetadataKeys();

  // The remaining set of action redirects depends on which table is being
  // tested.  Actions that originally redirect to multiple tables will still
  // be part of the redirect map when only one of the tables no longer
  // qualifies as hidden.
  const HiddenTableMapper::ActionRedirectMap& redirect_map =
      test_hidden_mapper.action_redirects();
  int expected_redirects = 0;
  if (!expected_hidden_decap_tables_.empty()) {
    // Both decap actions should still redirect to at least one hidden table.
    expected_redirects += 2;
    ExpectActionRedirects(redirect_map, kDecapAction1,
                          kMetaKeyDecap, expected_hidden_decap_tables_);
    ExpectActionRedirects(redirect_map, kDecapAction2,
                          kMetaKeyDecap, expected_hidden_decap_tables_);
    EXPECT_FALSE(ActionsHaveFieldAssignments(redirect_map, kMetaKeyDecap));
  }
  if (!expected_hidden_encap_tables_.empty()) {
    // Both encap actions should still redirect to at least one hidden table.
    expected_redirects += 2;
    ExpectActionRedirects(redirect_map, kEncapAction1,
                          kMetaKeyEncap, expected_hidden_encap_tables_);
    ExpectActionRedirects(redirect_map, kEncapAction2,
                          kMetaKeyEncap, expected_hidden_encap_tables_);
    EXPECT_FALSE(ActionsHaveFieldAssignments(redirect_map, kMetaKeyEncap));
  }
  EXPECT_EQ(expected_redirects, redirect_map.size());
  ExpectMetaDataKeyConsistency(redirect_map);
}

// Verifies that hidden table mapping produces no output when the entire
// P4 pipeline config is missing static entries.
TEST_F(HiddenTableMapperTest, TestNoStaticEntries) {
  SetUpTestIR("hidden_table1.ir.json");
  SetUpTestP4InfoAndPipeline();
  test_pipeline_config_.mutable_static_table_entries()->
      mutable_updates()->Clear();

  original_pipeline_config_ = test_pipeline_config_;
  HiddenTableMapper test_hidden_mapper;
  test_hidden_mapper.ProcessTables(
      mock_p4_info_manager_, &test_pipeline_config_);

  // The HiddenTableMapper action_redirects output should be empty, and the
  // P4 pipeline config should be unchanged.
  EXPECT_TRUE(test_hidden_mapper.action_redirects().empty());
  ExpectNoP4PipelineConfigChanges(original_pipeline_config_);
}

// Verifies behavior when a potential metadata key is assigned a non-constant
// value by an action.
TEST_F(HiddenTableMapperTest, TestNonConstKeyAssignment) {
  SetUpTestIR("hidden_table1.ir.json");
  SetUpTestP4InfoAndPipeline();
  const std::string kNonConstAction = kEncapAction1;
  hal::P4ActionDescriptor* descriptor =
      GetActionDescriptorOrDie(kNonConstAction);
  ASSERT_EQ(1, descriptor->assignments_size());
  // The assignment source changes from a constant to an action parameter.
  auto assignment = descriptor->mutable_assignments(0);
  assignment->mutable_assigned_value()->clear_constant_param();
  assignment->mutable_assigned_value()->set_parameter_name("dummy-param");
  const auto old_encap_descriptor = *GetFieldDescriptorOrDie(kMetaKeyEncap);

  HiddenTableMapper test_hidden_mapper;
  test_hidden_mapper.ProcessTables(
      mock_p4_info_manager_, &test_pipeline_config_);

  // The outputs for field kMetaKeyDecap and associated actions and tables
  // should be present, but all outputs affected by kMetaKeyEncap should
  // be absent.
  ExpectUnchangedMetadataKeys();
  const HiddenTableMapper::ActionRedirectMap& redirect_map =
      test_hidden_mapper.action_redirects();
  EXPECT_EQ(2, redirect_map.size());
  ExpectActionRedirects(redirect_map, kDecapAction1,
                        kMetaKeyDecap, expected_hidden_decap_tables_);
  ExpectActionRedirects(redirect_map, kDecapAction2,
                        kMetaKeyDecap, expected_hidden_decap_tables_);
  const auto& field_descriptor = *GetFieldDescriptorOrDie(kMetaKeyEncap);
  EXPECT_TRUE(
      MessageDifferencer::Equals(old_encap_descriptor, field_descriptor));
  EXPECT_FALSE(ActionsHaveFieldAssignments(redirect_map, kMetaKeyDecap));
  ExpectMetaDataKeyConsistency(redirect_map);
}

// Verifies behavior when the same action assigns a potential metadata key
// two different values.
TEST_F(HiddenTableMapperTest, TestDualKeyValuesInOneAction) {
  SetUpTestIR("hidden_table1.ir.json");
  SetUpTestP4InfoAndPipeline();
  const std::string kDualValueAction = kEncapAction2;
  hal::P4ActionDescriptor* descriptor =
      GetActionDescriptorOrDie(kDualValueAction);
  ASSERT_EQ(1, descriptor->assignments_size());
  // The cloned assignment assigns another constant to the metadata key.
  auto cloned_assignment = descriptor->add_assignments();
  *cloned_assignment = descriptor->assignments(0);
  cloned_assignment->mutable_assigned_value()->set_constant_param(0xfff);
  const auto old_encap_descriptor = *GetFieldDescriptorOrDie(kMetaKeyEncap);

  HiddenTableMapper test_hidden_mapper;
  test_hidden_mapper.ProcessTables(
      mock_p4_info_manager_, &test_pipeline_config_);

  // The outputs for field kMetaKeyDecap and associated actions and tables
  // should be present, but all outputs affected by kMetaKeyEncap should
  // be absent.
  ExpectUnchangedMetadataKeys();
  const HiddenTableMapper::ActionRedirectMap& redirect_map =
      test_hidden_mapper.action_redirects();
  EXPECT_EQ(2, redirect_map.size());
  ExpectActionRedirects(redirect_map, kDecapAction1,
                        kMetaKeyDecap, expected_hidden_decap_tables_);
  ExpectActionRedirects(redirect_map, kDecapAction2,
                        kMetaKeyDecap, expected_hidden_decap_tables_);
  const auto& field_descriptor = *GetFieldDescriptorOrDie(kMetaKeyEncap);
  EXPECT_TRUE(
      MessageDifferencer::Equals(old_encap_descriptor, field_descriptor));
  EXPECT_FALSE(ActionsHaveFieldAssignments(redirect_map, kMetaKeyDecap));
  ExpectMetaDataKeyConsistency(redirect_map);
}

// Verifies behavior when one action assigns two different metadata key fields.
TEST_F(HiddenTableMapperTest, TestOneActionMultipleKeys) {
  SetUpTestIR("hidden_table1.ir.json");
  SetUpTestP4InfoAndPipeline();
  const std::string kDualDestAction = kEncapAction2;
  hal::P4ActionDescriptor* descriptor =
      GetActionDescriptorOrDie(kDualDestAction);
  ASSERT_EQ(1, descriptor->assignments_size());
  // After adding the new assignment below, kEncapAction2 now uses both
  // kMetaKeyDecap and kMetaKeyEncap.
  auto new_assignment = descriptor->add_assignments();
  new_assignment->mutable_assigned_value()->set_constant_param(123);
  new_assignment->set_destination_field_name(kMetaKeyDecap);

  HiddenTableMapper test_hidden_mapper;
  test_hidden_mapper.ProcessTables(
      mock_p4_info_manager_, &test_pipeline_config_);

  // Since kEncapAction2 assigns the keys for both the hidden encap and decap
  // tables, it should redirect to the decap tables in addition to all the
  // normal encap tables.
  ExpectUnchangedMetadataKeys();
  const HiddenTableMapper::ActionRedirectMap& redirect_map =
      test_hidden_mapper.action_redirects();
  EXPECT_FALSE(ActionsHaveFieldAssignments(redirect_map, kMetaKeyDecap));
  EXPECT_FALSE(ActionsHaveFieldAssignments(redirect_map, kMetaKeyEncap));
  EXPECT_EQ(4, redirect_map.size());
  ExpectActionRedirects(redirect_map, kDecapAction1,
                        kMetaKeyDecap, expected_hidden_decap_tables_);
  ExpectActionRedirects(redirect_map, kDecapAction2,
                        kMetaKeyDecap, expected_hidden_decap_tables_);
  ExpectActionRedirects(redirect_map, kEncapAction1,
                        kMetaKeyEncap, expected_hidden_encap_tables_);
  ExpectActionRedirects(redirect_map, kEncapAction2,
                        kMetaKeyEncap, expected_hidden_encap_tables_);
  ExpectActionRedirects(redirect_map, kEncapAction2,
                        kMetaKeyDecap, expected_hidden_decap_tables_);
  ExpectMetaDataKeyConsistency(redirect_map);
}

// For the HiddenTableMapperTest fixture type, the string parameter
// identifies the table to test, and the integer constant describes how to
// setup the table for the test.
INSTANTIATE_TEST_SUITE_P(
  TestedHiddenTables,
  HiddenTableMapperTest,
  ::testing::Values(
      std::make_tuple(
          "ingress.hidden_decap_table", kTableSetupNonExactMatch),
      std::make_tuple(
          "ingress.hidden_encap_table_v4", kTableSetupNonExactMatch),
      std::make_tuple(
          "ingress.hidden_encap_table_v6", kTableSetupNonExactMatch),
      std::make_tuple(
          "ingress.hidden_decap_table", kTableSetupTooManyMatch),
      std::make_tuple(
          "ingress.hidden_encap_table_v4", kTableSetupTooManyMatch),
      std::make_tuple(
          "ingress.hidden_encap_table_v6", kTableSetupTooManyMatch),
      std::make_tuple(
          "ingress.hidden_decap_table", kTableSetupNotHidden),
      std::make_tuple(
          "ingress.hidden_encap_table_v4", kTableSetupNotHidden),
      std::make_tuple(
          "ingress.hidden_encap_table_v6", kTableSetupNotHidden),
      std::make_tuple(
          "ingress.hidden_decap_table", kTableSetupNoStaticEntries),
      std::make_tuple(
          "ingress.hidden_encap_table_v4", kTableSetupNoStaticEntries),
      std::make_tuple(
          "ingress.hidden_encap_table_v6", kTableSetupNoStaticEntries)));

}  // namespace p4c_backends
}  // namespace stratum
