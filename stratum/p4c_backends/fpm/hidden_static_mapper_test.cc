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

// This file contains HiddenStaticMapper unit tests.

#include "stratum/p4c_backends/fpm/hidden_static_mapper.h"

#include <memory>
#include <string>
#include <vector>

#include "stratum/hal/lib/p4/p4_info_manager_mock.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/hidden_table_mapper.h"
#include "stratum/p4c_backends/fpm/tunnel_optimizer_mock.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "gtest/gtest.h"
#include "absl/strings/substitute.h"
#include "external/com_github_p4lang_p4c/frontends/common/options.h"
#include "external/com_github_p4lang_p4c/lib/compile_context.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/gtl/map_util.h"

using ::testing::AnyNumber;
using ::testing::Return;

namespace stratum {
namespace p4c_backends {

// This class is the HiddenTableMapper test fixture.
class HiddenStaticMapperTest : public testing::Test {
 protected:
  static constexpr const char* kHiddenTable1Name = "hidden-table1";
  static constexpr const char* kHiddenTable2Name = "hidden-table2";
  static constexpr const char* kHiddenTableKeyName = "hidden-table-key";

  // This struct contains the P4Info entries for the hidden tables that
  // tests use.  It contains members with the hidden table's P4Info plus
  // the P4Info for two actions in the table.
  struct HiddenTableWithActions {
    ::p4::config::v1::Table table_info;      // P4Info for table.
    ::p4::config::v1::Action action_1_info;  // P4Info for table's first action.
    ::p4::config::v1::Action action_2_info;  // P4Info for table's 2nd action.
  };

  HiddenStaticMapperTest()
      : test_mapper_(absl::make_unique<HiddenStaticMapper>(
            mock_p4_info_manager_, &mock_tunnel_optimizer_)),
        next_p4_id_(1),
        test_p4c_context_(new P4CContextWithOptions<CompilerOptions>) {
  }

  // These methods set up the necessary data for test use.  The minimum
  // required data for testing a HiddenStaticMapper is:
  //  - P4Info for hidden tables and their actions, which is provided to the
  //    tested HiddenStaticMapper via a mock P4InfoManager.
  //  - A P4PipelineConfig (test_pipeline_config_) that contains:
  //      1) The static table entries that populate the hidden tables.
  //      2) Action descriptors for all hidden table actions.
  //  - An ActionRedirectMap (test_redirect_map_) that specifies the
  //    redirecting actions that a HiddenTableMapper would normally identify
  //    as actions that set local metadata key fields for lookup in hidden
  //    tables.
  // Upon input to the tested HiddenStaticMapper, test_pipeline_config_ does
  // not contain any action descriptors for the actions in test_redirect_map_.
  // HiddenStaticMapper does not look for these original descriptors, but it
  // updates them in test_pipeline_config_ after successfully processing the
  // static entries.  Thus, the presence or absence of an updated descriptor
  // in test_pipeline_config_ provides a simple test for whether the tested
  // HiddenStaticMapper succeeded.  See additional comments in the method
  // implementations.
  void SetUpHiddenTables();
  void AddHiddenTableWithActions(
      const std::string& table_name, HiddenTableWithActions* hidden_table);
  void AddStaticEntry(uint32 table_id, uint32 action_id,
                      const std::string& key_value);
  void SetUpActionRedirect(
      const std::string& redirecting_action_name, const std::string& key_name,
      int64 key_value, const std::string& hidden_table_name);
  void SetUpActionDescriptor(const std::string& action_name,
                             hal::P4ActionDescriptor* new_descriptor);

  // VerifyLinkToInternalAction evaluates the expectations for whether a
  // redirecting action descriptor correctly links to the InternalAction
  // that the HiddenStaticMapper creates.  The original_descriptor is the
  // input descriptor to HiddenStaticMapper, the output_descriptor is the
  // updated descriptor formed by HiddenStaticMapper, and the vector names
  // the hidden static table actions that HiddenStaticMapper combines into
  // an internal action.
  void VerifyLinkToInternalAction(
    const hal::P4ActionDescriptor& original_descriptor,
    const hal::P4ActionDescriptor& output_descriptor,
    const std::vector<std::string>& expected_hidden_actions);

  // HiddenStaticMapper for common test use.
  std::unique_ptr<HiddenStaticMapper> test_mapper_;

  hal::P4InfoManagerMock mock_p4_info_manager_;  // Mock for tests.
  TunnelOptimizerMock mock_tunnel_optimizer_;

  // SetUpHiddenTables populates this member, as described above.  The
  // test_pipeline_config_ also contains the HiddenStaticMapper output.
  hal::P4PipelineConfig test_pipeline_config_;

  // SetUpActionRedirect populates test_redirect_map_, as if the input
  // was coming from a previous HiddenTableMapper pass.
  HiddenTableMapper::ActionRedirectMap test_redirect_map_;

  // These members contains all of the necessary P4Info for two hidden tables.
  HiddenTableWithActions hidden1_;
  HiddenTableWithActions hidden2_;

  uint32 next_p4_id_;  // Provides a unique P4 ID for each tested object.

  // This test uses its own p4c context since it doesn't have the context
  // that IRTestHelperJson commonly provides to many backend unit tests.
  AutoCompileContext test_p4c_context_;
};

// Sets up the P4Info and the static table entries in the P4PipelineConfig
// to define two hidden tables for testing.
void HiddenStaticMapperTest::SetUpHiddenTables() {
  AddHiddenTableWithActions(kHiddenTable1Name, &hidden1_);
  AddHiddenTableWithActions(kHiddenTable2Name, &hidden2_);

  AddStaticEntry(hidden1_.table_info.preamble().id(),
                 hidden1_.action_1_info.preamble().id(), std::string({1}));
  AddStaticEntry(hidden1_.table_info.preamble().id(),
                 hidden1_.action_2_info.preamble().id(), std::string({2}));
  AddStaticEntry(hidden2_.table_info.preamble().id(),
                 hidden2_.action_1_info.preamble().id(), std::string({1}));
  AddStaticEntry(hidden2_.table_info.preamble().id(),
                 hidden2_.action_2_info.preamble().id(), std::string({2}));
}

// Sets up the test data for the hidden table identified by table_name.
// Upon return:
//  - The hidden_table output contains the P4Info for a hidden table with
//    two actions.
//  - Mock P4InfoManager expectations are in place for the tested
//    HiddenStaticMapper to be able to find the hidden table and its actions.
//  - Action descriptors in test_pipeline_config_ describe the hidden
//    table's actions.
void HiddenStaticMapperTest::AddHiddenTableWithActions(
    const std::string& table_name, HiddenTableWithActions* hidden_table) {
  hidden_table->table_info.mutable_preamble()->set_name(table_name);
  hidden_table->table_info.mutable_preamble()->set_id(next_p4_id_++);
  hidden_table->action_1_info.mutable_preamble()->set_name(
      absl::Substitute("$0-action-1", table_name.c_str()));
  hidden_table->action_1_info.mutable_preamble()->set_id(next_p4_id_++);
  hidden_table->action_2_info.mutable_preamble()->set_name(
      absl::Substitute("$0-action-2", table_name.c_str()));
  hidden_table->action_2_info.mutable_preamble()->set_id(next_p4_id_++);
  hidden_table->table_info.add_action_refs()->set_id(
      hidden_table->action_1_info.preamble().id());
  hidden_table->table_info.add_action_refs()->set_id(
      hidden_table->action_2_info.preamble().id());
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(table_name))
      .Times(AnyNumber()).WillRepeatedly(Return(hidden_table->table_info));
  EXPECT_CALL(mock_p4_info_manager_,
              FindActionByID(hidden_table->action_1_info.preamble().id()))
      .Times(AnyNumber()).WillRepeatedly(Return(hidden_table->action_1_info));
  EXPECT_CALL(mock_p4_info_manager_,
              FindActionByID(hidden_table->action_2_info.preamble().id()))
      .Times(AnyNumber()).WillRepeatedly(Return(hidden_table->action_2_info));

  hal::P4TableMapValue table_map_value;
  SetUpActionDescriptor(hidden_table->action_1_info.preamble().name(),
                        table_map_value.mutable_action_descriptor());
  gtl::InsertOrDie(test_pipeline_config_.mutable_table_map(),
                   hidden_table->action_1_info.preamble().name(),
                   table_map_value);
  table_map_value.Clear();
  SetUpActionDescriptor(hidden_table->action_2_info.preamble().name(),
                        table_map_value.mutable_action_descriptor());
  gtl::InsertOrDie(test_pipeline_config_.mutable_table_map(),
                   hidden_table->action_2_info.preamble().name(),
                   table_map_value);
}

// Adds one static table entry to the test_pipeline_config_.  The table
// entry refers to the input table_id and action_id, and it has one match
// field with the input key_value.
void HiddenStaticMapperTest::AddStaticEntry(
    uint32 table_id, uint32 action_id, const std::string& key_value) {
  auto static_entries = test_pipeline_config_.mutable_static_table_entries();
  auto update = static_entries->add_updates();
  update->set_type(::p4::v1::Update::INSERT);
  auto table_entry = update->mutable_entity()->mutable_table_entry();
  table_entry->set_table_id(table_id);
  auto table_match = table_entry->add_match();
  table_match->set_field_id(1);
  table_match->mutable_exact()->set_value(key_value);
  table_entry->mutable_action()->mutable_action()->set_action_id(action_id);
}

// Adds or updates an entry in the test_redirect_map_.  The input
// redirecting_action_name is the test_redirect_map_ key.  SetUpActionRedirect
// forms the map from the key_name, key_value, and hidden_table_name inputs.
void HiddenStaticMapperTest::SetUpActionRedirect(
    const std::string& redirecting_action_name, const std::string& key_name,
    int64 key_value, const std::string& hidden_table_name) {
  hal::P4ActionDescriptor::P4ActionRedirect new_action_redirect;
  new_action_redirect.set_key_field_name(key_name);
  new_action_redirect.set_key_value(key_value);
  new_action_redirect.add_internal_links()->set_hidden_table_name(
      hidden_table_name);

  // If the redirect_map entry already exists, its descriptor gets expanded
  // with an additional internal link, else a new entry is created.  Each new
  // entry also gets a dummy assignment for later content validation.
  hal::P4ActionDescriptor* redirect_descriptor = gtl::FindOrNull(
      test_redirect_map_, redirecting_action_name);
  if (redirect_descriptor != nullptr) {
    bool existing_redirect = false;
    for (auto& redirect : *redirect_descriptor->mutable_action_redirects()) {
      if (redirect.key_field_name() == key_name &&
          redirect.key_value() == key_value) {
        *redirect.add_internal_links() = new_action_redirect.internal_links(0);
        existing_redirect = true;
      }
    }
    if (!existing_redirect) {
      *redirect_descriptor->add_action_redirects() = new_action_redirect;
    }
  } else {
    hal::P4ActionDescriptor new_descriptor;
    SetUpActionDescriptor(redirecting_action_name, &new_descriptor);
    *new_descriptor.add_action_redirects() = new_action_redirect;
    gtl::InsertOrDie(
        &test_redirect_map_, redirecting_action_name, new_descriptor);
  }
}

// Tests generally create action descriptors with a dummy assignment that
// refers to the action name, which facilitates simple verification of content.
void HiddenStaticMapperTest::SetUpActionDescriptor(
    const std::string& action_name, hal::P4ActionDescriptor* new_descriptor) {
  new_descriptor->set_type(P4_ACTION_TYPE_FUNCTION);
  auto assignment = new_descriptor->add_assignments();
  assignment->set_destination_field_name(action_name);
  assignment->mutable_assigned_value()->set_constant_param(0);
}

void HiddenStaticMapperTest::VerifyLinkToInternalAction(
    const hal::P4ActionDescriptor& original_descriptor,
    const hal::P4ActionDescriptor& output_descriptor,
    const std::vector<std::string>& expected_hidden_actions) {
  // The output descriptor should redirect to exactly one internal action, which
  // must have an entry in the test_pipeline_config_'s table map.
  ASSERT_EQ(1, output_descriptor.action_redirects_size());
  const auto& action_redirect = output_descriptor.action_redirects(0);
  ASSERT_EQ(1, action_redirect.internal_links_size());
  const hal::P4ActionDescriptor::P4InternalActionLink& internal_link =
      action_redirect.internal_links(0);
  ASSERT_FALSE(internal_link.internal_action_name().empty());
  const auto table_map_entry = gtl::FindOrNull(
      test_pipeline_config_.table_map(), internal_link.internal_action_name());
  ASSERT_TRUE(table_map_entry != nullptr);
  ASSERT_TRUE(table_map_entry->has_internal_action());
  const auto& internal_descriptor = table_map_entry->internal_action();

  // The assignments in the new internal action should be combined from
  // the original descriptor and the descriptors for all hidden static actions.
  ASSERT_EQ(1 + expected_hidden_actions.size(),
            internal_descriptor.assignments_size());
  EXPECT_TRUE(ProtoEqual(original_descriptor.assignments(0),
                         internal_descriptor.assignments(0)));
  int assignment_i = 1;
  for (const auto& hidden_action : expected_hidden_actions) {
    const auto& hidden_descriptor = FindActionDescriptorOrDie(
        hidden_action, test_pipeline_config_);
    EXPECT_TRUE(ProtoEqual(hidden_descriptor.assignments(0),
                           internal_descriptor.assignments(assignment_i++)));
  }

  // The input action redirects from the original_descriptor should be moved
  // to input_redirects fields in the output descriptor.
  ASSERT_EQ(original_descriptor.action_redirects_size(),
            output_descriptor.action_redirects(0).input_redirects_size());
  for (int i = 0; i < original_descriptor.action_redirects_size(); ++i) {
    EXPECT_TRUE(ProtoEqual(
        original_descriptor.action_redirects(i),
        output_descriptor.action_redirects(0).input_redirects(i)));
  }
}

// Tests basic static entry mapping from a single original action to a single
// hidden action.
TEST_F(HiddenStaticMapperTest, TestOneActionToOneTable) {
  SetUpHiddenTables();
  const std::string kRedirectingAction = "redirecting-action";
  const int64 kHiddenKeyValue = 1;
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName, kHiddenKeyValue,
                      hidden1_.table_info.preamble().name());
  const hal::P4ActionDescriptor test_descriptor = gtl::FindOrDie(
      test_redirect_map_, kRedirectingAction);
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);

  // The "redirecting-action" should have one internal link to the first
  // action in the hidden1_ P4Info definitions.
  EXPECT_EQ(0, ::errorCount());
  const auto& output_descriptor =
      FindActionDescriptorOrDie(kRedirectingAction, test_pipeline_config_);
  SCOPED_TRACE("");
  VerifyLinkToInternalAction(test_descriptor, output_descriptor,
                             {hidden1_.action_1_info.preamble().name()});
}

// Tests a single action redirecting to two hidden tables.  This corresponds
// to the case where an action in a v4/v6 agnostic table sets a metadata key
// that ultimately refers to separate hidden v4 and v6 tables.
TEST_F(HiddenStaticMapperTest, TestOneActionToMultipleTables) {
  SetUpHiddenTables();
  const std::string kRedirectingAction = "redirecting-action";
  const int64 kHiddenKeyValue = 1;
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName, kHiddenKeyValue,
                      hidden1_.table_info.preamble().name());
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName, kHiddenKeyValue,
                      hidden2_.table_info.preamble().name());
  const hal::P4ActionDescriptor test_descriptor = gtl::FindOrDie(
      test_redirect_map_, kRedirectingAction);
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);

  // The "redirecting-action" should have two internal links, one to the first
  // action in the hidden1_ P4Info definitions and another to the first action
  // in the hidden2_ P4Info definitions.
  EXPECT_EQ(0, ::errorCount());
  const auto& output_descriptor =
      FindActionDescriptorOrDie(kRedirectingAction, test_pipeline_config_);
  SCOPED_TRACE("");
  VerifyLinkToInternalAction(test_descriptor, output_descriptor,
                             {hidden1_.action_1_info.preamble().name(),
                              hidden2_.action_1_info.preamble().name()});
}

// Tests multiple actions, with each action having a one-to-one relationship
// with an action in a specific hidden table.
TEST_F(HiddenStaticMapperTest, TestMultipleActionsToOneTable) {
  SetUpHiddenTables();
  const std::string kRedirectingAction1 = "redirecting-action-1";
  const int64 kHiddenKeyValue1 = 1;
  const std::string kRedirectingAction2 = "redirecting-action-2";
  const int64 kHiddenKeyValue2 = 2;
  SetUpActionRedirect(kRedirectingAction1, kHiddenTableKeyName,
                      kHiddenKeyValue1,
                      hidden1_.table_info.preamble().name());
  const hal::P4ActionDescriptor test_descriptor1 = gtl::FindOrDie(
      test_redirect_map_, kRedirectingAction1);
  SetUpActionRedirect(kRedirectingAction2, kHiddenTableKeyName,
                      kHiddenKeyValue2, hidden2_.table_info.preamble().name());
  const hal::P4ActionDescriptor test_descriptor2 = gtl::FindOrDie(
      test_redirect_map_, kRedirectingAction2);
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);

  // The "redirecting-action-1" should have one internal link to the first
  // action in the hidden1_ P4Info definitions.
  EXPECT_EQ(0, ::errorCount());
  {
    const auto& output_descriptor =
        FindActionDescriptorOrDie(kRedirectingAction1, test_pipeline_config_);
    SCOPED_TRACE("");
    VerifyLinkToInternalAction(test_descriptor1, output_descriptor,
                               {hidden1_.action_1_info.preamble().name()});
  }

  // The "redirecting-action-2" should have one internal link to the second
  // action in the hidden2_ P4Info definitions.
  {
    const auto& output_descriptor =
        FindActionDescriptorOrDie(kRedirectingAction2, test_pipeline_config_);
    SCOPED_TRACE("");
    VerifyLinkToInternalAction(test_descriptor2, output_descriptor,
                               {hidden2_.action_2_info.preamble().name()});
  }
}

// Tests HiddenStaticMapper behavior when test_redirect_map_ is empty.
TEST_F(HiddenStaticMapperTest, TestEmptyRedirectMap) {
  SetUpHiddenTables();
  hal::P4PipelineConfig original_pipeline_config = test_pipeline_config_;
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(original_pipeline_config, test_pipeline_config_));
}

// Tests HiddenStaticMapper behavior when no static table entry exists for
// a test_redirect_map_ entry.
TEST_F(HiddenStaticMapperTest, TestNoStaticEntryForRedirectKey) {
  SetUpHiddenTables();
  const std::string kRedirectingAction = "redirecting-action";
  const int64 kBadHiddenKeyValue = 123;  // No static entries with this key.
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName,
                      kBadHiddenKeyValue,
                      hidden1_.table_info.preamble().name());
  hal::P4PipelineConfig original_pipeline_config = test_pipeline_config_;
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(original_pipeline_config, test_pipeline_config_));
}

// The next series of tests verifies the ability to ignore static table
// entries in the P4PipelineConfig that don't meet the hidden table criteria.
// Each test should successfully process the remaining static entries.
// This test is for an update MODIFY instead of INSERT.
TEST_F(HiddenStaticMapperTest, TestStaticEntryNotInsert) {
  AddStaticEntry(1, 2, "1");  // The non-hidden update entry goes first.
  auto static_entries = test_pipeline_config_.mutable_static_table_entries();
  auto first_update = static_entries->mutable_updates(0);
  first_update->set_type(::p4::v1::Update::MODIFY);  // Changes INSERT to MODIFY
  SetUpHiddenTables();
  const std::string kRedirectingAction = "redirecting-action";
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName, 1,
                      hidden1_.table_info.preamble().name());
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);

  EXPECT_EQ(0, ::errorCount());
  const auto& redirecting_descriptor =
      FindActionDescriptorOrDie(kRedirectingAction, test_pipeline_config_);
  EXPECT_EQ(1, redirecting_descriptor.assignments_size());
  EXPECT_EQ(1, redirecting_descriptor.action_redirects_size());
}

// This test is for an update with no table_entry.
TEST_F(HiddenStaticMapperTest, TestStaticEntryNoTable) {
  AddStaticEntry(1, 2, "1");  // The non-hidden update entry goes first.
  auto static_entries = test_pipeline_config_.mutable_static_table_entries();
  auto first_update = static_entries->mutable_updates(0);
  first_update->mutable_entity()->clear_table_entry();
  SetUpHiddenTables();
  const std::string kRedirectingAction = "redirecting-action";
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName, 1,
                      hidden1_.table_info.preamble().name());
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);

  EXPECT_EQ(0, ::errorCount());
  const auto& redirecting_descriptor =
      FindActionDescriptorOrDie(kRedirectingAction, test_pipeline_config_);
  EXPECT_EQ(1, redirecting_descriptor.assignments_size());
  EXPECT_EQ(1, redirecting_descriptor.action_redirects_size());
}

// This test is for a static table entry with multiple match fields.
TEST_F(HiddenStaticMapperTest, TestStaticEntryMultiMatch) {
  AddStaticEntry(1, 2, "1");  // The non-hidden update entry goes first.
  auto static_entries = test_pipeline_config_.mutable_static_table_entries();
  auto first_update = static_entries->mutable_updates(0);
  auto extra_match_field =
      first_update->mutable_entity()->mutable_table_entry()->add_match();
  extra_match_field->set_field_id(2);
  extra_match_field->mutable_exact()->set_value("123");
  SetUpHiddenTables();
  const std::string kRedirectingAction = "redirecting-action";
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName, 1,
                      hidden1_.table_info.preamble().name());
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);

  EXPECT_EQ(0, ::errorCount());
  const auto& redirecting_descriptor =
      FindActionDescriptorOrDie(kRedirectingAction, test_pipeline_config_);
  EXPECT_EQ(1, redirecting_descriptor.assignments_size());
  EXPECT_EQ(1, redirecting_descriptor.action_redirects_size());
}

// This test is for a static table entry with a non-exact match field.
TEST_F(HiddenStaticMapperTest, TestStaticEntryNonExactMatch) {
  AddStaticEntry(1, 2, "1");  // The non-hidden update entry goes first.
  auto static_entries = test_pipeline_config_.mutable_static_table_entries();
  auto first_update = static_entries->mutable_updates(0);
  auto lpm_match_field =
      first_update->mutable_entity()->mutable_table_entry()->mutable_match(0);
  lpm_match_field->clear_exact();  // Flip exact match to LPM match.
  lpm_match_field->mutable_lpm()->set_value("123");
  SetUpHiddenTables();
  const std::string kRedirectingAction = "redirecting-action";
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName, 1,
                      hidden1_.table_info.preamble().name());
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);

  EXPECT_EQ(0, ::errorCount());
  const auto& redirecting_descriptor =
      FindActionDescriptorOrDie(kRedirectingAction, test_pipeline_config_);
  EXPECT_EQ(1, redirecting_descriptor.assignments_size());
  EXPECT_EQ(1, redirecting_descriptor.action_redirects_size());
}

// This test is for a static table entry with a match field that is
// too large to encode as a 64-bit key.
TEST_F(HiddenStaticMapperTest, TestStaticEntryMatchTooBig) {
  AddStaticEntry(1, 2, "1");  // The non-hidden update entry goes first.
  auto static_entries = test_pipeline_config_.mutable_static_table_entries();
  auto first_update = static_entries->mutable_updates(0);
  auto big_match_field =
      first_update->mutable_entity()->mutable_table_entry()->mutable_match(0);
  const std::string kVeryLongMatchKey = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  big_match_field->mutable_exact()->set_value(kVeryLongMatchKey);
  SetUpHiddenTables();
  const std::string kRedirectingAction = "redirecting-action";
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName, 1,
                      hidden1_.table_info.preamble().name());
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);

  EXPECT_EQ(0, ::errorCount());
  const auto& redirecting_descriptor =
      FindActionDescriptorOrDie(kRedirectingAction, test_pipeline_config_);
  EXPECT_EQ(1, redirecting_descriptor.assignments_size());
  EXPECT_EQ(1, redirecting_descriptor.action_redirects_size());
}

// This test is for a static table entry with no action reference.
TEST_F(HiddenStaticMapperTest, TestStaticEntryNoAction) {
  AddStaticEntry(1, 2, "1");  // The non-hidden update entry goes first.
  auto static_entries = test_pipeline_config_.mutable_static_table_entries();
  auto first_update = static_entries->mutable_updates(0);
  first_update->mutable_entity()->mutable_table_entry()->mutable_action()->
      clear_action();
  SetUpHiddenTables();
  const std::string kRedirectingAction = "redirecting-action";
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName, 1,
                      hidden1_.table_info.preamble().name());
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);

  EXPECT_EQ(0, ::errorCount());
  const auto& redirecting_descriptor =
      FindActionDescriptorOrDie(kRedirectingAction, test_pipeline_config_);
  EXPECT_EQ(1, redirecting_descriptor.assignments_size());
  EXPECT_EQ(1, redirecting_descriptor.action_redirects_size());
}

// This test is for a static table entry with unexpected action parameters.
TEST_F(HiddenStaticMapperTest, TestStaticEntryActionWithParam) {
  AddStaticEntry(1, 2, "1");  // The non-hidden update entry goes first.
  auto static_entries = test_pipeline_config_.mutable_static_table_entries();
  auto entity = static_entries->mutable_updates(0)->mutable_entity();
  auto table_action =
      entity->mutable_table_entry()->mutable_action()->mutable_action();
  auto action_param = table_action->add_params();
  action_param->set_param_id(1);
  action_param->set_value(std::string({1}));
  SetUpHiddenTables();
  const std::string kRedirectingAction = "redirecting-action";
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName, 1,
                      hidden1_.table_info.preamble().name());
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);

  EXPECT_EQ(0, ::errorCount());
  const auto& redirecting_descriptor =
      FindActionDescriptorOrDie(kRedirectingAction, test_pipeline_config_);
  EXPECT_EQ(1, redirecting_descriptor.assignments_size());
  EXPECT_EQ(1, redirecting_descriptor.action_redirects_size());
}

// This test checks for a p4c error when one of the input redirections is
// conditional on applying a specific table.
TEST_F(HiddenStaticMapperTest, TestAppliedTablesError) {
  SetUpHiddenTables();
  const std::string kRedirectingAction = "redirecting-action";
  const int64 kHiddenKeyValue = 1;
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName, kHiddenKeyValue,
                      hidden1_.table_info.preamble().name());
  SetUpActionRedirect(kRedirectingAction, kHiddenTableKeyName, kHiddenKeyValue,
                      hidden2_.table_info.preamble().name());
  hal::P4ActionDescriptor* test_descriptor = gtl::FindOrNull(
      test_redirect_map_, kRedirectingAction);
  ASSERT_TRUE(test_descriptor != nullptr);
  ASSERT_EQ(1, test_descriptor->action_redirects_size());
  ASSERT_LE(1, test_descriptor->action_redirects(0).internal_links_size());
  auto internal_link =
      test_descriptor->mutable_action_redirects(0)->mutable_internal_links(0);
  internal_link->add_applied_tables("any-table");
  hal::P4PipelineConfig original_pipeline_config = test_pipeline_config_;
  test_mapper_->ProcessStaticEntries(
      test_redirect_map_, &test_pipeline_config_);

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(original_pipeline_config, test_pipeline_config_));
}

}  // namespace p4c_backends
}  // namespace stratum
