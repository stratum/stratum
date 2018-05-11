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


#include "third_party/stratum/hal/lib/bcm/bcm_flow_table.h"

#include "third_party/stratum/glue/status/status_test_util.h"
#include "third_party/stratum/lib/test_utils/matchers.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

namespace stratum {
namespace hal {
namespace bcm {
namespace {

using test_utils::EqualsProto;
using test_utils::IsOkAndHolds;

constexpr char kMockTableEntry[] = R"PROTO(
    table_id: 1
    match {
      field_id: 1
      exact { value: "2" }
    }
    match {
      field_id: 2
      ternary {
        value: "3"
        mask: "4"
      }
    }
    match {
      field_id: 3
      lpm {
        value: "5"
        prefix_len: 6
      }
    }
    priority: 10
    action {
      action_profile_member_id: 11
    })PROTO";

const ::p4::TableEntry& MockTableEntry() {
  static const ::p4::TableEntry* entry = []() {
    auto* entry = new ::p4::TableEntry();
    CHECK_OK(ParseProtoFromString(kMockTableEntry, entry));
    return entry;
  }();
  return *entry;
}

// Verify properties of an initialized, but empty table.
TEST(BcmFlowTableTest, Initialize) {
  BcmFlowTable table(1);
  EXPECT_EQ(1, table.Id());
  EXPECT_EQ(0, table.EntryCount());
  EXPECT_FALSE(table.HasEntry(MockTableEntry()));
  EXPECT_EQ(table.Lookup(MockTableEntry()).status().error_code(),
            ERR_ENTRY_NOT_FOUND);
}

// Verify properties of a table when installing and removing a single entry.
TEST(BcmFlowTableTest, InstallRemoveSingleEntry) {
  BcmFlowTable table(1);
  // Install entry.
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  EXPECT_EQ(table.EntryCount(), 1);
  EXPECT_TRUE(table.HasEntry(MockTableEntry()));
  ASSERT_THAT(table.Lookup(MockTableEntry()),
              IsOkAndHolds(EqualsProto(MockTableEntry())));
  // Remove entry.
  ASSERT_THAT(table.DeleteEntry(MockTableEntry()),
              IsOkAndHolds(EqualsProto(MockTableEntry())));
  EXPECT_EQ(table.EntryCount(), 0);
  EXPECT_FALSE(table.HasEntry(MockTableEntry()));
}

// Verify properties of a table when installing and removing a multiple entries.
TEST(BcmFlowTableTest, InstallRemoveMultipleEntries) {
  // Set up mock entries.
  constexpr int kNumEntries = 4;
  std::vector<p4::TableEntry> mock_entries;
  for (int i = 0; i < kNumEntries; ++i) {
    mock_entries.push_back(MockTableEntry());
    mock_entries.back().mutable_match(0)->set_field_id(100 + i);
  }
  BcmFlowTable table(1);
  // Install entries.
  for (int i = 0; i < kNumEntries; ++i) {
    ASSERT_OK(table.InsertEntry(mock_entries[i]));
  }
  EXPECT_EQ(table.EntryCount(), kNumEntries);
  for (const auto& entry : mock_entries) {
    EXPECT_TRUE(table.HasEntry(entry));
  }
  // Remove entries.
  for (int i = kNumEntries - 1; i >= 0; --i) {
    ASSERT_THAT(table.DeleteEntry(mock_entries[i]),
                IsOkAndHolds(EqualsProto(mock_entries[i])));
    EXPECT_EQ(i, table.EntryCount());
    EXPECT_FALSE(table.HasEntry(mock_entries[i]));
  }
}

// Verify that duplicate entries cannot be inserted in the table.
TEST(BcmFlowTableTest, RejectInsertDuplicateEntry) {
  BcmFlowTable table(1);
  // Install default entry.
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  // Attempt to install a duplicate entry.
  EXPECT_EQ(table.InsertEntry(MockTableEntry()).error_code(), ERR_ENTRY_EXISTS);
}

// Verify that the following properties of an entry can be modified:
// 1) Action
// 2) Controller Metadata
// 3) Meter Config
// 4) Counter Data

// Verify that an entry's action can be modified.
TEST(BcmFlowTableTest, ModifyEntryAction) {
  ::p4::TableEntry mod = MockTableEntry();
  mod.mutable_action()->set_action_profile_member_id(
      mod.action().action_profile_member_id() + 1);

  BcmFlowTable table(1);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ASSERT_THAT(table.ModifyEntry(mod),
              IsOkAndHolds(EqualsProto(MockTableEntry())));

  // Verify State.
  EXPECT_TRUE(table.HasEntry(MockTableEntry()));
  EXPECT_TRUE(table.HasEntry(mod));
  EXPECT_THAT(table.Lookup(MockTableEntry()), IsOkAndHolds(EqualsProto(mod)));
}

// Verify that an entry's controller metadata can be modified.
TEST(BcmFlowTableTest, ModifyEntryControllerMetadata) {
  ::p4::TableEntry mod = MockTableEntry();
  mod.set_controller_metadata(mod.controller_metadata() + 1);

  BcmFlowTable table(1);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ASSERT_THAT(table.ModifyEntry(mod),
              IsOkAndHolds(EqualsProto(MockTableEntry())));

  // Verify State.
  EXPECT_TRUE(table.HasEntry(MockTableEntry()));
  EXPECT_TRUE(table.HasEntry(mod));
  EXPECT_THAT(table.Lookup(MockTableEntry()), IsOkAndHolds(EqualsProto(mod)));
}

// Verify that an entry's meter config can be modified.
TEST(BcmFlowTableTest, ModifyEntryMeterConfig) {
  ::p4::TableEntry mod = MockTableEntry();
  mod.mutable_meter_config()->set_cir(mod.meter_config().cir() + 1);

  BcmFlowTable table(1);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ASSERT_THAT(table.ModifyEntry(mod),
              IsOkAndHolds(EqualsProto(MockTableEntry())));

  // Verify State.
  EXPECT_TRUE(table.HasEntry(MockTableEntry()));
  EXPECT_TRUE(table.HasEntry(mod));
  EXPECT_THAT(table.Lookup(MockTableEntry()), IsOkAndHolds(EqualsProto(mod)));
}

// Verify that an entry's counter data can be modified.
TEST(BcmFlowTableTest, ModifyEntryCounterData) {
  ::p4::TableEntry mod = MockTableEntry();
  mod.mutable_counter_data()->set_packet_count(
      mod.counter_data().packet_count() + 1);

  BcmFlowTable table(1);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ASSERT_THAT(table.ModifyEntry(mod),
              IsOkAndHolds(EqualsProto(MockTableEntry())));

  // Verify State.
  EXPECT_TRUE(table.HasEntry(MockTableEntry()));
  EXPECT_TRUE(table.HasEntry(mod));
  EXPECT_THAT(table.Lookup(MockTableEntry()), IsOkAndHolds(EqualsProto(mod)));
}

// Verify that multiple valid entry parameters can be modified at once.
TEST(BcmFlowTableTest, ModifyEntryMultipleFields) {
  ::p4::TableEntry mod = MockTableEntry();
  mod.mutable_action()->set_action_profile_member_id(
      mod.action().action_profile_member_id() + 1);
  mod.set_controller_metadata(mod.controller_metadata() + 1);
  mod.mutable_meter_config()->set_cir(mod.meter_config().cir() + 1);
  mod.mutable_counter_data()->set_packet_count(
      mod.counter_data().packet_count() + 1);

  BcmFlowTable table(1);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ASSERT_THAT(table.ModifyEntry(mod),
              IsOkAndHolds(EqualsProto(MockTableEntry())));

  // Verify State.
  EXPECT_TRUE(table.HasEntry(MockTableEntry()));
  EXPECT_TRUE(table.HasEntry(mod));
  EXPECT_THAT(table.Lookup(MockTableEntry()), IsOkAndHolds(EqualsProto(mod)));

  // Modify back to MockTableEntry.
  ASSERT_THAT(table.ModifyEntry(MockTableEntry()),
              IsOkAndHolds(EqualsProto(mod)));

  // Verify State.
  EXPECT_TRUE(table.HasEntry(MockTableEntry()));
  EXPECT_TRUE(table.HasEntry(mod));
  EXPECT_THAT(table.Lookup(MockTableEntry()),
              IsOkAndHolds(EqualsProto(MockTableEntry())));
}

// Verify that lookup fails when the match parameters are removed.
TEST(BcmFlowTableTest, LookupRemovedMatchFailure) {
  ::p4::TableEntry mod = MockTableEntry();
  mod.clear_match();

  BcmFlowTable table(1);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ASSERT_EQ(table.Lookup(mod).status().error_code(), ERR_ENTRY_NOT_FOUND);
}

// Verify that lookup fails when a match parameter is missing.
TEST(BcmFlowTableTest, LookupMissingMatchFailure) {
  ::p4::TableEntry mod = MockTableEntry();
  mod.mutable_match()->erase(mod.mutable_match()->begin());

  BcmFlowTable table(1);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ASSERT_EQ(table.Lookup(mod).status().error_code(), ERR_ENTRY_NOT_FOUND);
}

// Verify that lookup fails when a match parameter is added.
TEST(BcmFlowTableTest, LookupExtraMatchFailure) {
  ::p4::TableEntry mod = MockTableEntry();
  *mod.add_match() = mod.match(0);

  BcmFlowTable table(1);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ASSERT_EQ(table.Lookup(mod).status().error_code(), ERR_ENTRY_NOT_FOUND);
}

// Verify that lookup fails when a match parameter is modified.
TEST(BcmFlowTableTest, LookupModifiedMatchFailure) {
  ::p4::TableEntry mod = MockTableEntry();
  mod.mutable_match(1)->set_field_id(mod.match(1).field_id() + 1);

  BcmFlowTable table(1);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ASSERT_EQ(table.Lookup(mod).status().error_code(), ERR_ENTRY_NOT_FOUND);
}

// Verify that lookup fails when the priority is modified.
TEST(BcmFlowTableTest, LookupPriorityFailure) {
  ::p4::TableEntry mod = MockTableEntry();
  mod.set_priority(mod.priority() + 1);

  BcmFlowTable table(1);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ASSERT_EQ(table.Lookup(mod).status().error_code(), ERR_ENTRY_NOT_FOUND);
}

// Verify that lookup fails when is_default_action is modified.
TEST(BcmFlowTableTest, LookupIsDefaultActionFailure) {
  ::p4::TableEntry mod = MockTableEntry();
  mod.set_is_default_action(!mod.is_default_action());

  BcmFlowTable table(1);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ASSERT_EQ(table.Lookup(mod).status().error_code(), ERR_ENTRY_NOT_FOUND);
}

// Verify that an equivalent entry can be deleted even if it is not exactly the
// same.
TEST(BcmFlowTableTest, DeleteEquivalentEntry) {
  ::p4::TableEntry mod = MockTableEntry();
  mod.clear_action();

  BcmFlowTable table(1);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ASSERT_THAT(table.DeleteEntry(mod),
              IsOkAndHolds(EqualsProto(MockTableEntry())));
}

// Verify that delete fails to delete a missing entry.
TEST(BcmFlowTableTest, DeleteMissingEntryFailure) {
  BcmFlowTable table(1);
  ASSERT_EQ(table.DeleteEntry(MockTableEntry()).status().error_code(),
            ERR_ENTRY_NOT_FOUND);
  ASSERT_OK(table.InsertEntry(MockTableEntry()));
  ::p4::TableEntry mod = MockTableEntry();
  mod.clear_match();
  ASSERT_EQ(table.DeleteEntry(mod).status().error_code(), ERR_ENTRY_NOT_FOUND);
}

}  // namespace
}  // namespace bcm
}  // namespace hal
}  // namespace stratum
