// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// P4InfoManager unit tests.

#include "stratum/hal/lib/p4/p4_info_manager.h"

#include <memory>

#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"
#include "gflags/gflags.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/p4_table_defs.pb.h"

DECLARE_bool(skip_p4_min_objects_check);

namespace stratum {
namespace hal {

using gflags::FlagSaver;
using ::testing::HasSubstr;

// This class is the P4InfoManager test fixture.
class P4InfoManagerTest : public testing::Test {
 protected:
  static const int kNumTestTables = 2;
  static const int kNumActionsPerTable = 3;
  static const int kFirstActionID = 10000;
  static const int kFirstActionProfileID = 20000;
  static const int kFirstCounterID = 10000000;
  static const int kFirstMeterID = 20000000;
  static const int kFirstValueSetID = 30000000;
  static const int kFirstRegisterID = 40000000;
  static const int kFirstDirectCounterID = 50000000;
  static const int kFirstDirectMeterID = 60000000;
  static const int kFirstDigestID = 70000000;

  // The default constructor creates p4_test_manager_ with empty p4_test_info_.
  P4InfoManagerTest() : p4_test_manager_(new P4InfoManager(p4_test_info_)) {}

  // Most tests were written before P4InfoManager verified presence of a minimal
  // set of objects.  To keep tests simple and allow then to define only the
  // objects relevant for test use, the skip flag is enabled, and individual
  // tests can choose to disable it.
  void SetUp() override { FLAGS_skip_p4_min_objects_check = true; }

  // Replaces the existing p4_test_manager_ with one constructed from the
  // current data in p4_test_info_.
  void SetUpNewP4Info() {
    p4_test_manager_ = absl::make_unique<P4InfoManager>(p4_test_info_);
  }

  // The P4Info setup functions below populate p4_test_info_ with common
  // resources for unit tests.  Each function creates a set of tables,
  // actions, or other resources and then calls SetUpNewP4Info to prepare
  // p4_test_manager_ with the revised P4Info.  Each function appends its
  // resources to any existing resources that are already part of p4_test_info_.
  //  SetUpAllTestP4Info: calls all other P4 setup functions, assuring that the
  //      content of p4_test_info_ is internally consistent and contains valid
  //      cross references among resources.
  //  SetUpTestP4Tables: creates a common set of Table entries in p4_test_info_.
  //      Tests should be aware that if they add no match fields or actions
  //      to satisfy the table cross references, they are likely to fail unless
  //      this is their intended behavior.  The alternative is to pass false
  //      as the need_actions parameter.
  //  SetUpTestP4Actions: populates p4_test_info_ with common actions.
  //  SetUpTestP4ActionProfiles: populates p4_test_info_ with action profiles
  //      for test use.
  //  SetUpTestP4Counters: populates p4_test_info_ with test counters.
  //  SetUpTestP4Meters: populates p4_test_info_ with test meters.
  //  SetUpTestP4ValueSets: populates p4_test_info_ with value sets.
  void SetUpAllTestP4Info() {
    FLAGS_skip_p4_min_objects_check = false;  // Minimum object set is present.
    SetUpTestP4Tables();
    SetUpTestP4Actions();
    SetUpTestP4ActionProfiles();
    SetUpTestP4Counters();
    SetUpTestP4Meters();
    SetUpTestP4ValueSets();
    SetUpTestP4Registers();
    SetUpTestP4DirectCounters();
    SetUpTestP4DirectMeters();
    SetUpTestP4Digests();
  }

  void SetUpTestP4Tables(bool need_actions = true) {
    int32 dummy_action_id = kFirstActionID;

    // Each table entry is assigned an ID and name in the preamble.  Each table
    // optionally gets a set of action IDs.
    for (int t = 1; t <= kNumTestTables; ++t) {
      ::p4::config::v1::Table* new_table = p4_test_info_.add_tables();
      new_table->mutable_preamble()->set_id(t);
      new_table->mutable_preamble()->set_name(absl::Substitute("Table-$0", t));
      for (int a = 0; need_actions && a < kNumActionsPerTable; ++a) {
        auto action_ref = new_table->add_action_refs();
        action_ref->set_id(dummy_action_id++);
      }
    }
    SetUpNewP4Info();
  }

  void SetUpTestP4Actions() {
    const int kNumTestActions = kNumTestTables * kNumActionsPerTable;
    const int kNumParamsPerAction = 2;
    int32 dummy_param_id = 100000;

    for (int a = 0; a < kNumTestActions; ++a) {
      ::p4::config::v1::Action* new_action = p4_test_info_.add_actions();
      new_action->mutable_preamble()->set_id(a + kFirstActionID);
      new_action->mutable_preamble()->set_name(
          absl::Substitute("Action-$0", a));
      for (int p = 0; p < kNumParamsPerAction; ++p) {
        auto new_param = new_action->add_params();
        new_param->set_id(dummy_param_id);
        new_param->set_name(
            absl::Substitute("Action-Param-$0", dummy_param_id++));
      }
    }

    SetUpNewP4Info();
  }

  void SetUpTestP4ActionProfiles() {
    // TODO(unknown): Tests get only one basic profile preamble at present.
    auto new_profile = p4_test_info_.add_action_profiles();
    new_profile->mutable_preamble()->set_id(kFirstActionProfileID);
    new_profile->mutable_preamble()->set_name("Action-Profile-1");
    SetUpNewP4Info();
  }

  void SetUpTestP4Counters() {
    // TODO(unknown): Tests get only one basic counter preamble at present.
    auto new_counter = p4_test_info_.add_counters();
    new_counter->mutable_preamble()->set_id(kFirstCounterID);
    new_counter->mutable_preamble()->set_name("Counter-1");
    SetUpNewP4Info();
  }

  void SetUpTestP4Meters() {
    // TODO(unknown): Tests get only one basic meter preamble at present.
    auto new_meter = p4_test_info_.add_meters();
    new_meter->mutable_preamble()->set_id(kFirstMeterID);
    new_meter->mutable_preamble()->set_name("Meter-1");
    SetUpNewP4Info();
  }

  void SetUpTestP4ValueSets() {
    // TODO(teverman): Tests get only one basic value set preamble at present.
    auto new_value_set = p4_test_info_.add_value_sets();
    new_value_set->mutable_preamble()->set_id(kFirstValueSetID);
    new_value_set->mutable_preamble()->set_name("Value-Set-1");
    new_value_set->add_match()->set_bitwidth(8);
    SetUpNewP4Info();
  }

  void SetUpTestP4Registers() {
    // TODO(unknown): Tests get only one basic register preamble at present.
    auto new_register = p4_test_info_.add_registers();
    new_register->mutable_preamble()->set_id(kFirstRegisterID);
    new_register->mutable_preamble()->set_name("Register-1");
    SetUpNewP4Info();
  }

  void SetUpTestP4DirectCounters() {
    // TODO(unknown): Tests get only one basic direct counter preamble at
    // present.
    auto new_counter = p4_test_info_.add_direct_counters();
    new_counter->mutable_preamble()->set_id(kFirstDirectCounterID);
    new_counter->mutable_preamble()->set_name("Direct-Counter-1");
    SetUpNewP4Info();
  }

  void SetUpTestP4DirectMeters() {
    // TODO(unknown): Tests get only one basic direct meter preamble at
    // present.
    auto new_meter = p4_test_info_.add_direct_meters();
    new_meter->mutable_preamble()->set_id(kFirstDirectMeterID);
    new_meter->mutable_preamble()->set_name("Direct-Meter-1");
    SetUpNewP4Info();
  }

  void SetUpTestP4Digests() {
    // TODO(unknown): Tests get only one basic digest preamble at present.
    auto new_digest = p4_test_info_.add_digests();
    new_digest->mutable_preamble()->set_id(kFirstDigestID);
    new_digest->mutable_preamble()->set_name("Digest-1");
    SetUpNewP4Info();
  }

  // FIXME(boc) disabling test due to missing tor_p4_info.pb.txt
  // Populates p4_test_info_ with all resources from the tor.p4 spec.  This
  // data provides assurance that P4InfoManager can handle real P4 compiler
  // output.
  //  void SetUpTorP4Info() {
  //    const std::string kTorP4File =
  //        "stratum/hal/lib/p4/testdata/tor_p4_info.pb.txt";
  //    ASSERT_TRUE(ReadProtoFromTextFile(kTorP4File, &p4_test_info_).ok());
  //    SetUpNewP4Info();
  //  }

  ::p4::config::v1::P4Info p4_test_info_;           // Sets up test P4Info.
  std::unique_ptr<P4InfoManager> p4_test_manager_;  // P4InfoManager for tests.
  FlagSaver flag_saver_;  // Reverts FLAGS_skip_p4_min_objects_check to default.
};

// This test is for coverage of the default constructor.
TEST(P4InfoManagerCoverage, TestDefaultConstructor) {
  class P4InfoManagerSubClass : public P4InfoManager {
    // This subclass calls P4InfoManager's protected default constructor.
  };
  P4InfoManagerSubClass p4_info_manager;
  EXPECT_EQ(0, p4_info_manager.p4_info().tables_size());
}

// Tests InitializeAndVerify with populated p4_test_info_.
TEST_F(P4InfoManagerTest, TestInitialize) {
  SetUpAllTestP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_TRUE(status.ok());
}

// Tests InitializeAndVerify when the input P4Info is empty.
TEST_F(P4InfoManagerTest, TestInitializeEmpty) {
  FLAGS_skip_p4_min_objects_check = false;
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(),
              HasSubstr("P4Info is missing these required resources"));
  EXPECT_THAT(status.error_message(), HasSubstr("Tables"));
  EXPECT_THAT(status.error_message(), HasSubstr("Actions"));
}

// The next 3 tests verify that P4InfoManager correctly detects when required
// objects are missing.  The tests avoid defining any cross-references
// between tables and actions to avoid other errors as side
// effects.
TEST_F(P4InfoManagerTest, TestMissingTables) {
  FLAGS_skip_p4_min_objects_check = false;
  auto new_action = p4_test_info_.add_actions();
  new_action->mutable_preamble()->set_id(kFirstActionID);
  new_action->mutable_preamble()->set_name("Required-Action");
  SetUpNewP4Info();

  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(),
              HasSubstr("P4Info is missing these required resources"));
  EXPECT_THAT(status.error_message(), HasSubstr("Tables"));
  EXPECT_THAT(status.error_message(), Not(HasSubstr("Actions")));
}

TEST_F(P4InfoManagerTest, TestMissingActions) {
  FLAGS_skip_p4_min_objects_check = false;
  auto new_table = p4_test_info_.add_tables();
  new_table->mutable_preamble()->set_id(1);
  new_table->mutable_preamble()->set_name("Required-Table");
  SetUpNewP4Info();

  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(),
              HasSubstr("P4Info is missing these required resources"));
  EXPECT_THAT(status.error_message(), Not(HasSubstr("Tables")));
  EXPECT_THAT(status.error_message(), HasSubstr("Actions"));
}

// Tests multiple InitializeAndVerify attempts.
TEST_F(P4InfoManagerTest, TestInitializeTwice) {
  SetUpTestP4Tables(false);
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_TRUE(status.ok());
  ::util::Status status2 = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status2.ok());
  EXPECT_EQ(ERR_INTERNAL, status2.error_code());
  EXPECT_FALSE(status2.error_message().empty());
  EXPECT_THAT(status2.error_message(), HasSubstr("already initialized"));
}

// Tests InitializeAndVerify when the input P4Info has a bad table ID.
TEST_F(P4InfoManagerTest, TestInitializeBadTableID) {
  ::p4::config::v1::Table* new_table = p4_test_info_.add_tables();
  new_table->mutable_preamble()->set_id(0);  // 0 is an invalid ID.
  new_table->mutable_preamble()->set_name("Table-0");
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("requires a non-zero ID"));
  EXPECT_THAT(status.error_message(), HasSubstr("Table"));
}

// Tests InitializeAndVerify when the input P4Info has an undefined table ID.
TEST_F(P4InfoManagerTest, TestInitializeNoTableID) {
  ::p4::config::v1::Table* new_table = p4_test_info_.add_tables();
  new_table->mutable_preamble()->set_name("Table-X");
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("requires a non-zero ID"));
  EXPECT_THAT(status.error_message(), HasSubstr("Table"));
}

// Tests InitializeAndVerify when the P4Info has 2 tables with the same ID.
TEST_F(P4InfoManagerTest, TestInitializeDuplicateTableID) {
  SetUpTestP4Tables(false);
  ASSERT_LE(2, p4_test_info_.tables_size());
  auto dup_preamble = p4_test_info_.mutable_tables(1)->mutable_preamble();
  dup_preamble->set_id(p4_test_info_.tables(0).preamble().id());
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("not unique"));
  EXPECT_THAT(status.error_message(), HasSubstr("Table ID"));
}

// Tests InitializeAndVerify when the input P4Info has a bad table name.
TEST_F(P4InfoManagerTest, TestInitializeBadTableName) {
  ::p4::config::v1::Table* new_table = p4_test_info_.add_tables();
  new_table->mutable_preamble()->set_id(1);
  new_table->mutable_preamble()->set_name("");  // Empty string is invalid.
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("requires a non-empty name"));
  EXPECT_THAT(status.error_message(), HasSubstr("Table"));
}

// Tests InitializeAndVerify when the input P4Info has an undefined table name.
TEST_F(P4InfoManagerTest, TestInitializeNoTableName) {
  ::p4::config::v1::Table* new_table = p4_test_info_.add_tables();
  new_table->mutable_preamble()->set_id(1);
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("requires a non-empty name"));
  EXPECT_THAT(status.error_message(), HasSubstr("Table"));
}

// Tests InitializeAndVerify when the P4Info has 2 tables with the same name.
TEST_F(P4InfoManagerTest, TestInitializeDuplicateTableName) {
  SetUpTestP4Tables(false);
  ASSERT_LE(2, p4_test_info_.tables_size());
  auto dup_preamble = p4_test_info_.mutable_tables(1)->mutable_preamble();
  dup_preamble->set_name(p4_test_info_.tables(0).preamble().name());
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("not unique"));
  EXPECT_THAT(status.error_message(), HasSubstr("Table name"));
}

// The Table ID and name tests above provide coverage for other types of
// resources since the implementation is common, so Actions, etc
// just do representative samples of the invalid ID and name tests.

// Tests InitializeAndVerify when the input P4Info has an undefined action ID.
TEST_F(P4InfoManagerTest, TestInitializeNoActionID) {
  ::p4::config::v1::Action* new_action = p4_test_info_.add_actions();
  new_action->mutable_preamble()->set_name("Action-X");
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("requires a non-zero ID"));
  EXPECT_THAT(status.error_message(), HasSubstr("Action"));
}

// Tests InitializeAndVerify when the P4Info has 2 actions with the same name.
TEST_F(P4InfoManagerTest, TestInitializeDuplicateActionName) {
  SetUpTestP4Actions();
  ASSERT_LE(2, p4_test_info_.actions_size());
  auto dup_preamble = p4_test_info_.mutable_actions(1)->mutable_preamble();
  dup_preamble->set_name(p4_test_info_.actions(0).preamble().name());
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("not unique"));
  EXPECT_THAT(status.error_message(), HasSubstr("Action name"));
}

// Tests InitializeAndVerify when the P4Info has 2 action profiles with the
// same name.
TEST_F(P4InfoManagerTest, TestInitializeDuplicateActionProfileName) {
  SetUpTestP4ActionProfiles();
  ASSERT_LE(1, p4_test_info_.action_profiles_size());
  auto dup_preamble = p4_test_info_.add_action_profiles()->mutable_preamble();
  dup_preamble->set_name(p4_test_info_.action_profiles(0).preamble().name());
  dup_preamble->set_id(p4_test_info_.action_profiles(0).preamble().id() + 100);
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("not unique"));
  EXPECT_THAT(status.error_message(), HasSubstr("Action-Profile name"));
}

// Tests InitializeAndVerify when the P4Info has an undefined counter name.
TEST_F(P4InfoManagerTest, TestInitializeNoCounterName) {
  auto new_counter = p4_test_info_.add_counters();
  new_counter->mutable_preamble()->set_id(1);
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("requires a non-empty name"));
  EXPECT_THAT(status.error_message(), HasSubstr("Counter"));
}

// Tests InitializeAndVerify when the P4Info has an undefined meter ID.
TEST_F(P4InfoManagerTest, TestInitializeNoMeterID) {
  auto new_meter = p4_test_info_.add_meters();
  new_meter->mutable_preamble()->set_name("bad-meter");
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("requires a non-zero ID"));
  EXPECT_THAT(status.error_message(), HasSubstr("Meter"));
}

// All valid tables in the p4_test_info_ should have successful name/ID lookups,
// and the returned data should match the table's original p4_test_info_ entry.
TEST_F(P4InfoManagerTest, TestFindTable) {
  SetUpTestP4Tables(false);
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  for (const auto& table : p4_test_info_.tables()) {
    auto id_status = p4_test_manager_->FindTableByID(table.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(table, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindTableByName(table.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(table, name_status.ValueOrDie()));
  }
}

// Verifies table lookup failure with an unknown table ID.
TEST_F(P4InfoManagerTest, TestFindTableUnknownID) {
  SetUpTestP4Tables(false);
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindTableByID(123456);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// Verifies table lookup failure with an unknown table name.
TEST_F(P4InfoManagerTest, TestFindTableUnknownName) {
  SetUpTestP4Tables(false);
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindTableByName("unknown-table");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// All valid actions in p4_test_info_ should have successful name/ID lookups,
// and the returned data should match the action's original p4_test_info_ entry.
TEST_F(P4InfoManagerTest, TestFindAction) {
  SetUpTestP4Actions();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  for (const auto& action : p4_test_info_.actions()) {
    auto id_status = p4_test_manager_->FindActionByID(action.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(action, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindActionByName(action.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(action, name_status.ValueOrDie()));
  }
}

// Verifies action lookup failure with an unknown action ID.
TEST_F(P4InfoManagerTest, TestFindActionUnknownID) {
  SetUpTestP4Actions();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindActionByID(654321);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// Verifies action lookup failure with an unknown action name.
TEST_F(P4InfoManagerTest, TestFindActionUnknownName) {
  SetUpTestP4Actions();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindActionByName("unknown-action");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// All valid action profiles in p4_test_info_ should have successful name/ID
// lookups, and the returned data should match the action profile's original
// p4_test_info_ entry.
TEST_F(P4InfoManagerTest, TestFindActionProfile) {
  SetUpTestP4ActionProfiles();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  for (const auto& profile : p4_test_info_.action_profiles()) {
    auto id_status =
        p4_test_manager_->FindActionProfileByID(profile.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(profile, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindActionProfileByName(profile.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(profile, name_status.ValueOrDie()));
  }
}

// Verifies lookup failure with an unknown action profile ID.
TEST_F(P4InfoManagerTest, TestFindActionProfileUnknownID) {
  SetUpTestP4ActionProfiles();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindActionProfileByID(654321);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// Verifies lookup failure with an unknown action profile name.
TEST_F(P4InfoManagerTest, TestFindActionProfileUnknownName) {
  SetUpTestP4ActionProfiles();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindActionProfileByName("unknown-profile");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// All valid counters in p4_test_info_ should have successful name/ID
// lookups, and the returned data should match the counter's original
// p4_test_info_ entry.
TEST_F(P4InfoManagerTest, TestFindCounter) {
  SetUpTestP4Counters();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  for (const auto& counter : p4_test_info_.counters()) {
    auto id_status = p4_test_manager_->FindCounterByID(counter.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(counter, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindCounterByName(counter.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(counter, name_status.ValueOrDie()));
  }
}

// Verifies lookup failure with an unknown counter ID.
TEST_F(P4InfoManagerTest, TestFindCounterUnknownID) {
  SetUpTestP4Counters();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindCounterByID(0x9abcd);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// Verifies lookup failure with an unknown counter name.
TEST_F(P4InfoManagerTest, TestFindCounterUnknownName) {
  SetUpTestP4Counters();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindCounterByName("unknown-counter");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// All valid meters in p4_test_info_ should have successful name/ID
// lookups, and the returned data should match the meter's original
// p4_test_info_ entry.
TEST_F(P4InfoManagerTest, TestFindMeter) {
  SetUpTestP4Meters();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  for (const auto& meter : p4_test_info_.meters()) {
    auto id_status = p4_test_manager_->FindMeterByID(meter.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(meter, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindMeterByName(meter.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(meter, name_status.ValueOrDie()));
  }
}

// Verifies lookup failure with an unknown meter ID.
TEST_F(P4InfoManagerTest, TestFindMeterUnknownID) {
  SetUpTestP4Meters();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindMeterByID(0xfedcba);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// Verifies lookup failure with an unknown meter name.
TEST_F(P4InfoManagerTest, TestFindMeterUnknownName) {
  SetUpTestP4Meters();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindMeterByName("unknown-meter");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// All valid value sets in p4_test_info_ should have successful name/ID
// lookups, and the returned data should match the value set's original
// p4_test_info_ entry.
TEST_F(P4InfoManagerTest, TestFindValueSet) {
  SetUpTestP4ValueSets();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  for (const auto& value_set : p4_test_info_.value_sets()) {
    auto id_status =
        p4_test_manager_->FindValueSetByID(value_set.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(value_set, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindValueSetByName(value_set.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(value_set, name_status.ValueOrDie()));
  }
}

// Verifies lookup failure with an unknown value set ID.
TEST_F(P4InfoManagerTest, TestFindValueSetUnknownID) {
  SetUpTestP4ValueSets();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindValueSetByID(0xfedcba);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// Verifies lookup failure with an unknown value set name.
TEST_F(P4InfoManagerTest, TestFindValueSetUnknownName) {
  SetUpTestP4ValueSets();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindValueSetByName("unknown-value-set");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// All valid registers in p4_test_info_ should have successful name/ID
// lookups, and the returned data should match the value set's original
// p4_test_info_ entry.
TEST_F(P4InfoManagerTest, TestFindRegister) {
  SetUpTestP4Registers();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  for (const auto& register_entry : p4_test_info_.registers()) {
    auto id_status =
        p4_test_manager_->FindRegisterByID(register_entry.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(register_entry, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindRegisterByName(register_entry.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(register_entry, name_status.ValueOrDie()));
  }
}

// Verifies lookup failure with an unknown register ID.
TEST_F(P4InfoManagerTest, TestFindRegisterUnknownID) {
  SetUpTestP4Registers();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindRegisterByID(0xfedcba);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// Verifies lookup failure with an unknown register name.
TEST_F(P4InfoManagerTest, TestFindRegisterUnknownName) {
  SetUpTestP4Registers();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindRegisterByName("unknown-register");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// All valid direct counters in p4_test_info_ should have successful name/ID
// lookups, and the returned data should match the counter's original
// p4_test_info_ entry.
TEST_F(P4InfoManagerTest, TestFindDirectCounter) {
  SetUpTestP4DirectCounters();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  for (const auto& counter : p4_test_info_.direct_counters()) {
    auto id_status =
        p4_test_manager_->FindDirectCounterByID(counter.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(counter, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindDirectCounterByName(counter.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(counter, name_status.ValueOrDie()));
  }
}

// Verifies lookup failure with an unknown counter ID.
TEST_F(P4InfoManagerTest, TestFindDirectCounterUnknownID) {
  SetUpTestP4DirectCounters();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindDirectCounterByID(0x9abcd);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// Verifies lookup failure with an unknown direct counter name.
TEST_F(P4InfoManagerTest, TestFindDirectCounterUnknownName) {
  SetUpTestP4DirectCounters();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindDirectCounterByName("unknown-counter");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// All valid direct meters in p4_test_info_ should have successful name/ID
// lookups, and the returned data should match the meter's original
// p4_test_info_ entry.
TEST_F(P4InfoManagerTest, TestFindDirectMeter) {
  SetUpTestP4DirectMeters();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  for (const auto& counter : p4_test_info_.direct_counters()) {
    auto id_status =
        p4_test_manager_->FindDirectCounterByID(counter.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(counter, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindDirectCounterByName(counter.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(counter, name_status.ValueOrDie()));
  }
}

// Verifies lookup failure with an unknown direct meter ID.
TEST_F(P4InfoManagerTest, TestFindDirectMeterUnknownID) {
  SetUpTestP4DirectMeters();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindDirectMeterByID(0x9abcd);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// Verifies lookup failure with an unknown direct meter name.
TEST_F(P4InfoManagerTest, TestFindDirectMeterUnknownName) {
  SetUpTestP4DirectMeters();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindDirectMeterByName("unknown-meter");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// All valid digests in p4_test_info_ should have successful name/ID
// lookups, and the returned data should match the digest's original
// p4_test_info_ entry.
TEST_F(P4InfoManagerTest, TestFindDigest) {
  SetUpTestP4Digests();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  for (const auto& digest : p4_test_info_.digests()) {
    auto id_status = p4_test_manager_->FindDigestByID(digest.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(digest, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindDigestByName(digest.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(digest, name_status.ValueOrDie()));
  }
}

// Verifies lookup failure with an unknown digest ID.
TEST_F(P4InfoManagerTest, TestFindDigestUnknownID) {
  SetUpTestP4Digests();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindDigestByID(0x9abcd);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

// Verifies lookup failure with an unknown digest name.
TEST_F(P4InfoManagerTest, TestFindDigestUnknownName) {
  SetUpTestP4Digests();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->FindDigestByName("unknown-digest");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr("not found"));
}

TEST_F(P4InfoManagerTest, TestDumpNamesToIDs) {
  SetUpAllTestP4Info();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  p4_test_manager_->DumpNamesToIDs();
}

// FIXME(boc) disabling test due to missing tor_p4_info.pb.txt
/*
// Tests ability to handle a "real" P4 spec (tor.p4).
TEST_F(P4InfoManagerTest, TestTorP4Info) {
  SetUpTorP4Info();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());

  // This loop verifies that p4_test_manager_ finds all expected P4 tables.
  for (const auto& table : p4_test_info_.tables()) {
    auto id_status = p4_test_manager_->FindTableByID(table.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(table, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindTableByName(table.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(table, name_status.ValueOrDie()));
  }

  // This loop verifies that p4_test_manager_ finds all expected P4 actions.
  for (const auto& action : p4_test_info_.actions()) {
    auto id_status = p4_test_manager_->FindActionByID(action.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(action, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindActionByName(action.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(action, name_status.ValueOrDie()));
  }

  // This loop verifies that p4_test_manager_ finds all expected P4 action
  // profiles.
  for (const auto& profile : p4_test_info_.action_profiles()) {
    auto id_status =
        p4_test_manager_->FindActionProfileByID(profile.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(profile, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindActionProfileByName(profile.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(profile, name_status.ValueOrDie()));
  }

  // This loop verifies that p4_test_manager_ finds all expected P4 counters.
  for (const auto& counter : p4_test_info_.counters()) {
    auto id_status = p4_test_manager_->FindCounterByID(counter.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(counter, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindCounterByName(counter.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(counter, name_status.ValueOrDie()));
  }

  // This loop verifies that p4_test_manager_ finds all expected P4 meters.
  for (const auto& meter : p4_test_info_.meters()) {
    auto id_status = p4_test_manager_->FindMeterByID(meter.preamble().id());
    EXPECT_TRUE(id_status.ok());
    EXPECT_TRUE(ProtoEqual(meter, id_status.ValueOrDie()));
    auto name_status =
        p4_test_manager_->FindMeterByName(meter.preamble().name());
    EXPECT_TRUE(name_status.ok());
    EXPECT_TRUE(ProtoEqual(meter, name_status.ValueOrDie()));
  }
}

// Tests ID duplication across resource types.
TEST_F(P4InfoManagerTest, TestDuplicateIDTableAndAction) {
  SetUpTorP4Info();
  ASSERT_LE(1, p4_test_info_.tables_size());
  ASSERT_LE(1, p4_test_info_.actions_size());
  auto dup_preamble1 = p4_test_info_.add_actions()->mutable_preamble();
  auto dup_preamble2 = p4_test_info_.add_tables()->mutable_preamble();
  dup_preamble1->set_id(0xabcdef);
  dup_preamble1->set_name("action-with-duplicate-ID");
  dup_preamble2->set_id(dup_preamble1->id());
  dup_preamble2->set_name("table-with-duplicate-ID");
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("not unique"));
}
 */

#if 0
// TODO(unknown): Rework this test for 2 objects with global IDs.
// Tests name duplication across resource types.
TEST_F(P4InfoManagerTest, TestDuplicateNameHeaderFieldAndAction) {
  SetUpTorP4Info();
  ASSERT_LE(1, p4_test_info_.header_fields_size());
  ASSERT_LE(1, p4_test_info_.actions_size());
  auto dup_preamble1 = p4_test_info_.add_actions()->mutable_preamble();
  auto dup_preamble2 = p4_test_info_.add_header_fields()->mutable_preamble();
  dup_preamble1->set_name("same-name-in-action-and-header-field");
  dup_preamble2->set_name(dup_preamble1->name());
  dup_preamble1->set_id(0xabcdef);
  dup_preamble2->set_id(0xfedcba);
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(), HasSubstr("not unique"));
}
#endif

// Tests table that refers to an unknown action.
TEST_F(P4InfoManagerTest, TestTableMissingActionXref) {
  SetUpAllTestP4Info();
  auto bad_xref_table = p4_test_info_.add_tables();
  bad_xref_table->mutable_preamble()->set_id(654321);
  bad_xref_table->mutable_preamble()->set_name("table-xref-unknown-action");
  bad_xref_table->add_action_refs()->set_id(kFirstActionID - 1);
  SetUpNewP4Info();
  ::util::Status status = p4_test_manager_->InitializeAndVerify();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.error_code());
  EXPECT_FALSE(status.error_message().empty());
  EXPECT_THAT(status.error_message(),
              HasSubstr(bad_xref_table->preamble().name()));
  EXPECT_THAT(status.error_message(), HasSubstr("refers to an invalid"));
}

// FIXME(boc) disabling test due to missing tor_p4_info.pb.txt
// Tests GetSwitchStackAnnotations with a pipeline_stage.
/*
TEST_F(P4InfoManagerTest, TestPipelineStageAnnotations) {
  SetUpTorP4Info();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status =
      p4_test_manager_->GetSwitchStackAnnotations("class_id_assignment_table");
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(P4Annotation::VLAN_ACL, status.ValueOrDie().pipeline_stage());
}
 */

// Tests GetSwitchStackAnnotations with multiple annotations.
TEST_F(P4InfoManagerTest, TestGetAnnotationsMultiple) {
  // SetUpTestP4Actions doesn't add annotations, so the test adds a mix
  // of @switchstack with other type of annotations.
  SetUpTestP4Actions();
  auto test_preamble = p4_test_info_.mutable_actions(0)->mutable_preamble();
  test_preamble->add_annotations("@switch stack(1234)");
  test_preamble->add_annotations("@dummy(456)");
  test_preamble->add_annotations("@switchstack(\"pipeline_stage: L2\")");
  test_preamble->add_annotations(
      "@switchstack(\"field_type: P4_FIELD_TYPE_VRF\")");
  test_preamble->add_annotations("@dummy(789)");
  SetUpNewP4Info();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status =
      p4_test_manager_->GetSwitchStackAnnotations(test_preamble->name());
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(P4Annotation::L2, status.ValueOrDie().pipeline_stage());
  EXPECT_EQ(P4_FIELD_TYPE_VRF, status.ValueOrDie().field_type());
}

// Tests GetSwitchStackAnnotations with @switchstack parsing error.
TEST_F(P4InfoManagerTest, TestGetAnnotationsParseError) {
  SetUpTestP4Actions();
  auto test_preamble = p4_test_info_.mutable_actions(0)->mutable_preamble();
  test_preamble->add_annotations("@switchstack(\"bogus string\")");
  SetUpNewP4Info();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status =
      p4_test_manager_->GetSwitchStackAnnotations(test_preamble->name());
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(status.status().error_message().empty());
}

// Tests GetSwitchStackAnnotations with @switchstack trailing suffix error.
TEST_F(P4InfoManagerTest, TestGetAnnotationsSwitchStackSuffix) {
  SetUpTestP4Actions();
  auto test_preamble = p4_test_info_.mutable_actions(0)->mutable_preamble();
  test_preamble->add_annotations("@switchstack(\"pipeline_stage: L2\")))");
  SetUpNewP4Info();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status =
      p4_test_manager_->GetSwitchStackAnnotations(test_preamble->name());
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(),
              HasSubstr(test_preamble->name()));
  EXPECT_THAT(status.status().error_message(), HasSubstr("has invalid syntax"));
}

// Tests GetSwitchStackAnnotations when the named object has no annotations.
TEST_F(P4InfoManagerTest, TestGetAnnotationsNotAnnotated) {
  SetUpTestP4Actions();  // The SetUp function doesn't add annotations.
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  auto status = p4_test_manager_->GetSwitchStackAnnotations(
      p4_test_info_.actions(0).preamble().name());
  EXPECT_TRUE(status.ok());
  P4Annotation empty_annotation;
  EXPECT_TRUE(ProtoEqual(empty_annotation, status.ValueOrDie()));
}

// Tests GetSwitchStackAnnotations when the named object does not exist.
TEST_F(P4InfoManagerTest, TestGetAnnotationsBadInputName) {
  SetUpAllTestP4Info();
  ASSERT_TRUE(p4_test_manager_->InitializeAndVerify().ok());
  const std::string kUnknownName = "unknown-p4-name";
  auto status = p4_test_manager_->GetSwitchStackAnnotations(kUnknownName);
  EXPECT_EQ(ERR_INVALID_P4_INFO, status.status().error_code());
  EXPECT_FALSE(status.status().error_message().empty());
  EXPECT_THAT(status.status().error_message(), HasSubstr(kUnknownName));
  EXPECT_THAT(status.status().error_message(),
              HasSubstr("does not exist or does not contain a Preamble"));
}

}  // namespace hal
}  // namespace stratum
