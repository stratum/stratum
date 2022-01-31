// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// This file contains P4ActionMapper unit tests.

#include "stratum/hal/lib/p4/p4_action_mapper.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/p4/p4_info_manager_mock.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {

using ::testing::AnyNumber;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::ReturnRef;

// This hidden namespace stores constants related to the test input file.
namespace {

constexpr char kTestPipelineFile[] = "action_mapper_test_pipeline.pb.txt";
constexpr char kActionNoInternals[] = "action-no-internal-links";
constexpr char kParameterizedAction[] = "parameterized-action";
constexpr char kInternalActionNameFormat[] = "internal-action-$0";
constexpr char kActionAppliedTable[] = "action-applied-table";
constexpr char kAppliedTable[] = "applied-table";
constexpr char kActionBadInternalLink[] = "action-bad-internal-link";
constexpr char kMissingInternalAction[] = "missing-internal-action";
constexpr char kActionBadAppliedTables[] = "action-bad-applied-tables";
constexpr char kDuplicateAppliedTable[] = "duplicate-applied-table";
constexpr char kActionBadAppliedTables2[] = "action-bad-applied-tables-2";

}  // namespace

// This class is the P4ActionMapper test fixture.  For parameterized tests,
// the string specificies a sequence of internal links according to whether
// they are qualified by applied tables.
class P4ActionMapperTest : public testing::TestWithParam<std::string> {
 protected:
  // The file_name refers to a text file with P4PipelineConfig data for tests.
  // It may be empty to to test with a test_pipeline_config_ that was created
  // by the caller.
  void SetUpP4Config(const std::string& file_name) {
    if (!file_name.empty()) {
      ReadP4PipelineConfig(file_name);
    }

    test_action_mapper_ =
        absl::make_unique<P4ActionMapper>(test_pipeline_config_);
    EXPECT_CALL(mock_p4info_manager_, p4_info())
        .Times(AnyNumber())
        .WillRepeatedly(ReturnRef(test_p4_info_));
  }

  // Reads file_name with proto buffer text into test_pipeline_config_.
  void ReadP4PipelineConfig(const std::string& file_name) {
    const std::string test_pipeline_file =
        "stratum/hal/lib/p4/testdata/" + file_name;
    ASSERT_OK(
        ReadProtoFromTextFile(test_pipeline_file, &test_pipeline_config_));
  }

  // Creates a P4Info actions() entry in test_p4_info_.
  void SetUpP4InfoAction(const std::string& action_name, uint32 action_id) {
    auto new_action = test_p4_info_.add_actions();
    new_action->mutable_preamble()->set_name(action_name);
    new_action->mutable_preamble()->set_id(action_id);
  }

  // Sets up the action named by kParameterizedAction in test_pipeline_config_
  // with a set of links to internal actions.  The internal_sequence string
  // typically comes from GetParam(), but it may also contain a test-specific
  // sequence in the format defined by INSTANTIATE_TEST_CASE_P.  The two
  // boolean flags control options for how the p4c backend might choose
  // to encode the internal action links in the pipeline config:
  //  maximize_action_redirects minimize_internal_links Description
  //           false                     false          Note 1
  //           true                      false          Note 2
  //           false                     true           Note 3
  //           true                      true           Invalid
  // Note 1 - a single action_redirects entry contains all the links to
  //          internal actions.
  // Note 2 - multiple action_redirects each contain one link to internal
  //          actions.
  // Note 3 - similar to Note 1, but applied_tables for the same internal
  //          action are kept in the same internal_links entry where possible.
  void SetUpInternalLinkSequence(const std::string& internal_sequence,
                                 bool maximize_action_redirects,
                                 bool minimize_internal_links) {
    // The action descriptor for kParameterizedAction needs to be modified
    // according to the input parameters.
    ASSERT_GE(CountInternalActions(), internal_sequence.size());
    P4TableMapValue* table_map_entry = gtl::FindOrNull(
        *test_pipeline_config_.mutable_table_map(), kParameterizedAction);
    ASSERT_TRUE(table_map_entry != nullptr);
    ASSERT_TRUE(table_map_entry->has_action_descriptor());
    auto action_descriptor = table_map_entry->mutable_action_descriptor();
    P4ActionDescriptor::P4ActionRedirect* redirects = nullptr;

    // All internal links appear in the same P4ActionRedirect unless the
    // maximize_action_redirects flag is enabled.
    if (!maximize_action_redirects) {
      redirects = action_descriptor->add_action_redirects();
    }
    P4ActionDescriptor::P4InternalActionLink* internal_link = nullptr;
    char previous_sequence = ' ';

    for (int i = 0; i < internal_sequence.size(); ++i) {
      if (maximize_action_redirects) {
        redirects = action_descriptor->add_action_redirects();
        internal_link = nullptr;
      }

      // The internal_link is cleared to force creation of a new link when:
      // - A 'U' indicates an internal action unqualified by certain applied
      //   tables, or
      // - A 'U' in the preceding sequence entry indicates an unqualified
      //   internal action.
      // In other word, each unqualified internal action always has its own
      // internal_link.
      if (internal_sequence[i] == 'U' || previous_sequence == 'U') {
        internal_link = nullptr;
      }
      if (!minimize_internal_links || internal_link == nullptr) {
        internal_link = redirects->add_internal_links();
        internal_link->set_internal_action_name(
            absl::Substitute(kInternalActionNameFormat, i + 1));
      }

      // A 'Q' in the sequence appends an applied_tables entry in the current
      // internal_link.
      if (internal_sequence[i] == 'Q') {
        const std::string applied_table_name =
            absl::Substitute("applied-table-$0", i);
        internal_link->add_applied_tables(applied_table_name);
        ::p4::config::v1::Table applied_table_info;
        applied_table_info.mutable_preamble()->set_id(1 + i);
        EXPECT_CALL(mock_p4info_manager_, FindTableByName(applied_table_name))
            .WillOnce(Return(applied_table_info));
      }

      previous_sequence = internal_sequence[i];
    }
  }

  // Counts the number of internal actions in test_pipeline_config_.
  int CountInternalActions() {
    int count = 0;
    for (const auto& table_map_entry : test_pipeline_config_.table_map()) {
      if (table_map_entry.second.has_internal_action()) ++count;
    }
    return count;
  }

  // This is the P4ActionMapper for test use, created by SetUpP4Config.
  std::unique_ptr<P4ActionMapper> test_action_mapper_;

  // The test_pipeline_config_ is populated by SetUpP4Config or
  // ReadP4PipelineConfig, with optional additions by SetUpInternalLinkSequence
  // and/or individual tests.  SetUpP4InfoAction generally fills in the
  // test_p4_info_.
  P4PipelineConfig test_pipeline_config_;
  ::p4::config::v1::P4Info test_p4_info_;

  P4InfoManagerMock mock_p4info_manager_;  // Mocks P4InfoManager for test use.
};

// This type uses the P4ActionMapperTest fixture with different
// parameter sets.
typedef P4ActionMapperTest P4ActionMapperInvalidSequenceTest;

// Tests empty P4PipelineConfig and P4Info.
TEST_F(P4ActionMapperTest, TestAddEmptyP4Actions) {
  SetUpP4Config("");
  EXPECT_OK(test_action_mapper_->AddP4Actions(mock_p4info_manager_));
}

// Tests adding an action with no internal links.
TEST_F(P4ActionMapperTest, TestAddActionNoInternals) {
  SetUpP4Config(kTestPipelineFile);
  constexpr uint32 kTestActionId = 1;
  SetUpP4InfoAction(kActionNoInternals, kTestActionId);
  EXPECT_OK(test_action_mapper_->AddP4Actions(mock_p4info_manager_));
}

// TODO(teverman): Update test when implementation is ready.
// TEST_F(P4ActionMapperTest, TestMapActionIDAndTableID) {
//   SetUpP4Config("");
//   ASSERT_OK(test_action_mapper_->AddP4Actions(mock_p4info_manager_));
//   EXPECT_OK(test_action_mapper_->MapActionIDAndTableID(1, 1));
// }

// // TODO(teverman): Update test when implementation is ready.
// TEST_F(P4ActionMapperTest, TestMapActionID) {
//   SetUpP4Config("");
//   ASSERT_OK(test_action_mapper_->AddP4Actions(mock_p4info_manager_));
//   EXPECT_OK(test_action_mapper_->MapActionID(1));
// }

// Tests a P4Info reference to an action with no P4PipelineConfig descriptor.
TEST_F(P4ActionMapperTest, TestMissingActionDescriptor) {
  SetUpP4Config("");
  SetUpP4InfoAction("missing-action", 1);
  ::util::Status status =
      test_action_mapper_->AddP4Actions(mock_p4info_manager_);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("missing-action"));
}

// Tests an action descriptor internal link reference to an unknown
// internal action.
TEST_F(P4ActionMapperTest, TestMissingInternalAction) {
  SetUpP4Config(kTestPipelineFile);
  constexpr uint32 kTestActionId = 1;
  SetUpP4InfoAction(kActionBadInternalLink, kTestActionId);
  ::util::Status status =
      test_action_mapper_->AddP4Actions(mock_p4info_manager_);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr(kActionBadInternalLink));
  EXPECT_THAT(status.error_message(), HasSubstr(kMissingInternalAction));
}

// Tests an internal link reference to an applied table with no P4Info.
TEST_F(P4ActionMapperTest, TestMissingP4InfoTable) {
  SetUpP4Config(kTestPipelineFile);
  constexpr uint32 kTestActionId = 1;
  SetUpP4InfoAction(kActionAppliedTable, kTestActionId);
  const ::util::Status table_error = MAKE_ERROR(ERR_INTERNAL)
                                     << "Table not found";
  EXPECT_CALL(mock_p4info_manager_, FindTableByName(kAppliedTable))
      .WillOnce(Return(table_error));
  ::util::Status status =
      test_action_mapper_->AddP4Actions(mock_p4info_manager_);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr(table_error.error_message()));
}

// Tests an internal link with multiple references to the same applied table.
TEST_F(P4ActionMapperTest, TestDuplicateAppliedTable) {
  SetUpP4Config(kTestPipelineFile);
  constexpr uint32 kTestActionId = 1;
  SetUpP4InfoAction(kActionBadAppliedTables, kTestActionId);
  ::p4::config::v1::Table applied_table_info;
  applied_table_info.mutable_preamble()->set_id(200);
  EXPECT_CALL(mock_p4info_manager_, FindTableByName(kDuplicateAppliedTable))
      .Times(2)
      .WillRepeatedly(Return(applied_table_info));
  ::util::Status status =
      test_action_mapper_->AddP4Actions(mock_p4info_manager_);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Unexpected duplicate"));
  EXPECT_THAT(status.error_message(), HasSubstr(kDuplicateAppliedTable));
}

// Tests internal links with separate references to the same applied table.
TEST_F(P4ActionMapperTest, TestDuplicateAppliedTableMultiLink) {
  SetUpP4Config(kTestPipelineFile);
  constexpr uint32 kTestActionId = 1;
  SetUpP4InfoAction(kActionBadAppliedTables2, kTestActionId);
  ::p4::config::v1::Table applied_table_info;
  applied_table_info.mutable_preamble()->set_id(200);
  EXPECT_CALL(mock_p4info_manager_, FindTableByName(kDuplicateAppliedTable))
      .Times(2)
      .WillRepeatedly(Return(applied_table_info));
  ::util::Status status =
      test_action_mapper_->AddP4Actions(mock_p4info_manager_);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Unexpected duplicate"));
  EXPECT_THAT(status.error_message(), HasSubstr(kDuplicateAppliedTable));
}

// Tests error after multiple calls to AddP4Actions.
TEST_F(P4ActionMapperTest, TestAddActionTwice) {
  SetUpP4Config(kTestPipelineFile);
  constexpr uint32 kTestActionId = 1;
  SetUpP4InfoAction(kActionNoInternals, kTestActionId);
  EXPECT_OK(test_action_mapper_->AddP4Actions(mock_p4info_manager_));
  ::util::Status status2 =
      test_action_mapper_->AddP4Actions(mock_p4info_manager_);
  EXPECT_FALSE(status2.ok());
  EXPECT_THAT(status2.error_message(),
              HasSubstr("already processed this P4PipelineConfig"));
}

// The next three tests work with various valid sequences of internal links
// parameterized by GetParam().  The SetUpInternalLinkSequence flags
// simulate various possible ways the compiler may encode the internal links.
TEST_P(P4ActionMapperTest, TestMultipleValidSequences1) {
  ReadP4PipelineConfig(kTestPipelineFile);
  SetUpInternalLinkSequence(GetParam(), false, false);
  SetUpP4Config("");
  constexpr uint32 kTestActionId = 100;
  SetUpP4InfoAction(kParameterizedAction, kTestActionId);
  EXPECT_OK(test_action_mapper_->AddP4Actions(mock_p4info_manager_));
}

TEST_P(P4ActionMapperTest, TestMultipleValidSequences2) {
  ReadP4PipelineConfig(kTestPipelineFile);
  SetUpInternalLinkSequence(GetParam(), true, false);
  SetUpP4Config("");
  constexpr uint32 kTestActionId = 100;
  SetUpP4InfoAction(kParameterizedAction, kTestActionId);
  EXPECT_OK(test_action_mapper_->AddP4Actions(mock_p4info_manager_));
}

TEST_P(P4ActionMapperTest, TestMultipleValidSequences3) {
  ReadP4PipelineConfig(kTestPipelineFile);
  SetUpInternalLinkSequence(GetParam(), false, true);
  SetUpP4Config("");
  constexpr uint32 kTestActionId = 100;
  SetUpP4InfoAction(kParameterizedAction, kTestActionId);
  EXPECT_OK(test_action_mapper_->AddP4Actions(mock_p4info_manager_));
}

// The next three tests verify various invalid sequences of internal links
// parameterized by GetParam().  The SetUpInternalLinkSequence flags
// simulate various possible ways the compiler may encode the internal links.
TEST_P(P4ActionMapperInvalidSequenceTest, TestInvalidSequences1) {
  ReadP4PipelineConfig(kTestPipelineFile);
  SetUpInternalLinkSequence(GetParam(), false, false);
  SetUpP4Config("");
  constexpr uint32 kTestActionId = 100;
  SetUpP4InfoAction(kParameterizedAction, kTestActionId);
  EXPECT_FALSE(test_action_mapper_->AddP4Actions(mock_p4info_manager_).ok());
}

TEST_P(P4ActionMapperInvalidSequenceTest, TestInvalidSequences2) {
  ReadP4PipelineConfig(kTestPipelineFile);
  SetUpInternalLinkSequence(GetParam(), true, false);
  SetUpP4Config("");
  constexpr uint32 kTestActionId = 100;
  SetUpP4InfoAction(kParameterizedAction, kTestActionId);
  EXPECT_FALSE(test_action_mapper_->AddP4Actions(mock_p4info_manager_).ok());
}

TEST_P(P4ActionMapperInvalidSequenceTest, TestInvalidSequences3) {
  ReadP4PipelineConfig(kTestPipelineFile);
  SetUpInternalLinkSequence(GetParam(), false, true);
  SetUpP4Config("");
  constexpr uint32 kTestActionId = 100;
  SetUpP4InfoAction(kParameterizedAction, kTestActionId);
  EXPECT_FALSE(test_action_mapper_->AddP4Actions(mock_p4info_manager_).ok());
}

// A 'Q' in the parameter string means an action with an applied table
// qualifier, and a 'U' indicates an unqualified internal action.  The
// parameter string length is bounded by the number of internal actions
// in the pipeline config test file.
INSTANTIATE_TEST_CASE_P(ValidAppliedTablesSequence, P4ActionMapperTest,
                        ::testing::Values("Q", "QQ", "QQQ", "QQU", "U", "UQ",
                                          "UQQ", "QU", "QUQ"));

INSTANTIATE_TEST_CASE_P(InvalidAppliedTablesSequence,
                        P4ActionMapperInvalidSequenceTest,
                        ::testing::Values("UU", "UQU", "UUQ", "QUU"));

}  // namespace hal
}  // namespace stratum
