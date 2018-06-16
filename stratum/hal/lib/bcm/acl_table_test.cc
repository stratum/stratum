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


#include "stratum/hal/lib/bcm/acl_table.h"

#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "github.com/p4lang/PI/p4/v1/p4runtime.grpc.pb.h"

namespace stratum {
namespace hal {
namespace bcm {
namespace {

using test_utils::EqualsProto;
using testing::_;
using testing::HasSubstr;
using testing::UnorderedElementsAreArray;
using testing::status::IsOkAndHolds;
using testing::status::StatusIs;

constexpr char kDefaultP4Table[] = R"PROTO(
    preamble {
      id: 1
      name: "table_1"
    }
    match_fields { id: 100 }
    match_fields { id: 200 }
    match_fields { id: 300 }
    size: 10
    )PROTO";

p4::config::Table DefaultP4Table() {
  ::p4::config::Table p4_table;
  CHECK_OK(ParseProtoFromString(kDefaultP4Table, &p4_table));
  return p4_table;
}

//*****************************************************************************
//  Constructor Tests
//*****************************************************************************
// Verifies tables created by constructor tests.
void VerifyConstructorTable(const AclTable& table) {
  EXPECT_EQ(table.Id(), 1);
  EXPECT_EQ(table.Name(), "table_1");
  EXPECT_EQ(table.EntryCount(), 0);
  EXPECT_EQ(table.Stage(), BCM_ACL_STAGE_IFP);
  EXPECT_EQ(table.Priority(), 12);
  EXPECT_EQ(table.Size(), 10);
  EXPECT_EQ(table.PhysicalTableId(), 11);
  EXPECT_FALSE(table.HasUdf());
  EXPECT_FALSE(table.IsUdfField(100));
  EXPECT_FALSE(table.IsUdfField(200));
  EXPECT_FALSE(table.IsUdfField(300));
  EXPECT_THAT(table.MatchFields(), UnorderedElementsAreArray({100, 200, 300}));
}

TEST(AclTableTest, BcmAclStageConstructor) {
  ::p4::config::Table p4_table = DefaultP4Table();
  AclTable table(p4_table, BCM_ACL_STAGE_IFP, 12);
  table.SetPhysicalTableId(11);
  // Check the values in the ACL table.
  VerifyConstructorTable(table);
}

TEST(AclTableTest, P4PipelineConstructor) {
  ::p4::config::Table p4_table = DefaultP4Table();
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);
  table.SetPhysicalTableId(11);
  // Check the values in the ACL table.
  VerifyConstructorTable(table);
}

TEST(AclTableTest, CopyConstructor) {
  ::p4::config::Table p4_table = DefaultP4Table();
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);
  table.SetPhysicalTableId(11);
  // Copy and verify the table.
  AclTable copy_table = table;
  VerifyConstructorTable(copy_table);
}

TEST(AclTableTest, MoveConstructor) {
  ::p4::config::Table p4_table = DefaultP4Table();
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);
  table.SetPhysicalTableId(11);
  // Move and verify the table.
  AclTable move_table = std::move(table);
  VerifyConstructorTable(move_table);
}

//*****************************************************************************
//  Table Entry Management Tests
//*****************************************************************************

// Verify that valid entries can be added to and read from an AclTable.
TEST(AclTableTest, InsertEntry) {
  ::p4::config::Table p4_table = DefaultP4Table();
  // Create & initialize the ACL table.
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);

  // Set up the expected entry. For each match field supported by the table, the
  // entry will appear as:
  //   table_id: <table id>
  //   match {
  //     field_id: <match_field id>
  //     exact { value: "<match_field id>" }
  //   }
  std::vector<p4::TableEntry> entries;
  for (const auto& match_field : p4_table.match_fields()) {
    ::p4::TableEntry entry;
    entry.set_table_id(p4_table.preamble().id());
    auto* match = entry.add_match();
    match->set_field_id(match_field.id());
    match->mutable_exact()->set_value(absl::StrCat(match_field.id()));
    entries.push_back(entry);
  }

  // Insert the entries and verify the table state.
  // We use the string representation since:
  //   1. The protobufs are simple enough for the strings to be stable.
  //   2. Strings show up better than google::protobuf::Message in the error
  //      output of *ElementsAre*.
  std::vector<string> inserted_entries;
  for (const auto& entry : entries) {
    ASSERT_OK(table.InsertEntry(entry));
    inserted_entries.push_back(entry.ShortDebugString());
    std::vector<string> table_entries;
    for (const ::p4::TableEntry& entry : table) {
      table_entries.push_back(entry.ShortDebugString());
    }
    ASSERT_THAT(table_entries, UnorderedElementsAreArray(inserted_entries));
    EXPECT_EQ(table.EntryCount(), inserted_entries.size());
  }

  // Clear the table.
  for (const auto& entry : entries) {
    ASSERT_OK(table.DeleteEntry(entry));
  }
  EXPECT_TRUE(table.Empty());
}

// Verify that entries can be added, deleted, added again, etc.
TEST(AclTableTest, ReInsertEntry) {
  ::p4::config::Table p4_table = DefaultP4Table();
  // Create & initialize the ACL table.
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);

  // Set up an entry.
  ::p4::TableEntry entry;
  entry.set_table_id(p4_table.preamble().id());
  entry.add_match()->set_field_id(p4_table.match_fields(0).id());

  // Insall, delete, install, and delete the entry.
  ASSERT_OK(table.InsertEntry(entry));
  ASSERT_OK(table.DeleteEntry(entry));
  ASSERT_OK(table.InsertEntry(entry));
  ASSERT_OK(table.DeleteEntry(entry));
}

// Verify that duplicate entries are rejected from an AclTable. Delete should
// delete the existing entry.
TEST(AclTableTest, InsertDuplicateEntry) {
  ::p4::config::Table p4_table = DefaultP4Table();
  // Create & initialize the ACL table.
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);

  // Set up an entry.
  ::p4::TableEntry entry;
  entry.set_table_id(p4_table.preamble().id());
  entry.add_match()->set_field_id(p4_table.match_fields(0).id());

  // Install the entry twice.
  ASSERT_OK(table.InsertEntry(entry));
  EXPECT_THAT(table.InsertEntry(entry),
              StatusIs(StratumErrorSpace(), ERR_ENTRY_EXISTS,
                       HasSubstr(entry.ShortDebugString())));
  EXPECT_EQ(table.EntryCount(), 1);
  EXPECT_TRUE(table.HasEntry(entry));
  EXPECT_OK(table.DeleteEntry(entry));
  EXPECT_TRUE(table.Empty());
}

// Verify that an entry with a match field that does not match the table's match
// fields is rejected.
TEST(AclTableTest, InsertEntryWithBadMatchField) {
  ::p4::config::Table p4_table = DefaultP4Table();
  // Create & initialize the ACL table.
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);

  // Set up an entry with a bad match field.
  ::p4::TableEntry entry;
  entry.set_table_id(p4_table.preamble().id());
  entry.add_match()->set_field_id(9);

  EXPECT_THAT(table.InsertEntry(entry),
              StatusIs(StratumErrorSpace(), ERR_INVALID_PARAM,
                       HasSubstr(entry.ShortDebugString())));
  EXPECT_TRUE(table.Empty());
}

// Verify that adding an entry past the table size is rejected.
TEST(AclTableTest, InsertEntryToCapacity) {
  ::p4::config::Table p4_table = DefaultP4Table();
  // Create & initialize the ACL table.
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);

  // Fill the table.
  ::p4::TableEntry entry;
  entry.set_table_id(p4_table.preamble().id());
  entry.add_match()->set_field_id(p4_table.match_fields(0).id());
  for (int i = 0; i < table.Size(); ++i) {
    entry.mutable_match(0)->mutable_exact()->set_value(absl::StrCat(i));
    ASSERT_OK(table.InsertEntry(entry));
  }
  // Attempt to add another entry.
  entry.mutable_match(0)->mutable_exact()->set_value("test");
  EXPECT_THAT(table.InsertEntry(entry),
              StatusIs(StratumErrorSpace(), ERR_TABLE_FULL, _));
  EXPECT_EQ(table.EntryCount(), table.Size());
  // Clear the table. Perform the clear in reverse to cover this case.
  for (int i = table.Size() - 1; i >= 0; --i) {
    entry.mutable_match(0)->mutable_exact()->set_value(absl::StrCat(i));
    ASSERT_OK(table.DeleteEntry(entry));
  }
  EXPECT_TRUE(table.Empty());
}

// Make sure that entry insertion with a Bcm ACL ID stores the ID and otherwise
// behaves the same as inserting just an entry.
TEST(AclTableTest, InsertEntryWithBcmAclId) {
  ::p4::config::Table p4_table = DefaultP4Table();
  // Create & initialize the ACL table.
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);

  // Generate entries and associated IDs.
  struct EntryAndId {
    ::p4::TableEntry entry;
    int bcm_acl_id;
  };
  std::vector<EntryAndId> entries;
  int id = 1;
  for (const auto& match_field : p4_table.match_fields()) {
    EntryAndId entry_and_id;
    entry_and_id.entry.set_table_id(p4_table.preamble().id());
    entry_and_id.entry.add_match()->set_field_id(match_field.id());
    entry_and_id.bcm_acl_id = id++;
    entries.push_back(entry_and_id);
  }

  // Insert the entries.
  for (const EntryAndId& entry : entries) {
    ASSERT_OK(table.InsertEntry(entry.entry, entry.bcm_acl_id));
  }
  EXPECT_EQ(table.EntryCount(), entries.size());
  // Lookup the BcmAclIds.
  for (const EntryAndId& entry : entries) {
    EXPECT_THAT(table.BcmAclId(entry.entry), IsOkAndHolds(entry.bcm_acl_id))
        << "Entry: " << entry.entry.ShortDebugString();
  }
  // Clear the entries.
  for (const EntryAndId& entry : entries) {
    ASSERT_OK(table.DeleteEntry(entry.entry));
  }
  EXPECT_TRUE(table.Empty());
  // Lookup the BcmAclIds. These should all fail now.
  for (const EntryAndId& entry : entries) {
    EXPECT_THAT(table.BcmAclId(entry.entry).status(),
                StatusIs(StratumErrorSpace(), ERR_ENTRY_NOT_FOUND, _))
        << "Entry: " << entry.entry.ShortDebugString();
  }
}

// Make sure that SetBcmAclId sets Bcm ACL ID.
TEST(AclTableTest, SetBcmAclId) {
  ::p4::config::Table p4_table = DefaultP4Table();
  // Create & initialize the ACL table.
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);

  // Set up an entry.
  ::p4::TableEntry entry;
  entry.set_table_id(p4_table.preamble().id());
  entry.add_match()->set_field_id(p4_table.match_fields(0).id());

  // Install the entry and modify the Bcm ACL ID.
  ASSERT_OK(table.InsertEntry(entry));
  ASSERT_THAT(table.BcmAclId(entry).status(),
              StatusIs(StratumErrorSpace(), ERR_NOT_INITIALIZED, _));
  ASSERT_OK(table.SetBcmAclId(entry, 12));
  EXPECT_THAT(table.BcmAclId(entry), IsOkAndHolds(12));

  // Clear the entry.
  ASSERT_OK(table.DeleteEntry(entry));
  EXPECT_TRUE(table.Empty());
  EXPECT_THAT(table.BcmAclId(entry).status(),
              StatusIs(StratumErrorSpace(), ERR_ENTRY_NOT_FOUND, _));
}

// Verify that valid entries can be modified in the AclTable.
TEST(AclTableTest, ModifyEntry) {
  ::p4::config::Table p4_table = DefaultP4Table();
  // Create & initialize the ACL table.
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);

  // Set up the expected entry. For each match field supported by the table, the
  // entry will appear as:
  //   table_id: <table id>
  //   match {
  //     field_id: <match_field id>
  //     exact { value: "<match_field id>" }
  //   }
  std::vector<p4::TableEntry> entries;
  int unique_id = 0;
  for (const auto& match_field : p4_table.match_fields()) {
    ::p4::TableEntry entry;
    entry.set_table_id(p4_table.preamble().id());
    auto* match = entry.add_match();
    match->set_field_id(match_field.id());
    match->mutable_exact()->set_value(absl::StrCat(match_field.id()));
    entry.mutable_action()->set_action_profile_member_id(++unique_id);
    entries.push_back(entry);
    ASSERT_OK(table.InsertEntry(entry, unique_id));
  }

  // Modify the action.
  unique_id = 100;
  std::vector<p4::TableEntry> modified_entries;
  for (const auto& entry : entries) {
    ::p4::TableEntry modified_entry = entry;
    modified_entry.mutable_action()->set_action_profile_member_id(++unique_id);
    modified_entries.push_back(modified_entry);
    ASSERT_THAT(table.ModifyEntry(modified_entry),
                IsOkAndHolds(EqualsProto(entry)));
  }

  // Inspect the table.
  unique_id = 0;
  for (int index = 0; index < entries.size(); ++index) {
    EXPECT_THAT(table.Lookup(entries[index]),
                IsOkAndHolds(EqualsProto(modified_entries[index])));
    EXPECT_THAT(table.BcmAclId(modified_entries[index]),
                IsOkAndHolds(++unique_id));
    EXPECT_THAT(table.BcmAclId(entries[index]), IsOkAndHolds(unique_id));
  }

  // Clear the table.
  for (const auto& entry : entries) {
    ASSERT_OK(table.DeleteEntry(entry));
  }
  EXPECT_TRUE(table.Empty());
}

// Make sure that MarkUdfMatchField marks the UDF match field and sets the UDF
// set ID.
TEST(AclTableTest, SetUdfSetId) {
  ::p4::config::Table p4_table = DefaultP4Table();
  // Create & initialize the ACL table.
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);

  ASSERT_OK(table.MarkUdfMatchField(p4_table.match_fields(0).id(), 999));

  EXPECT_TRUE(table.IsUdfField(p4_table.match_fields(0).id()));
  EXPECT_FALSE(table.IsUdfField(p4_table.match_fields(1).id()));
  EXPECT_FALSE(table.IsUdfField(p4_table.match_fields(2).id()));
  EXPECT_EQ(table.UdfSetId(), 999);
  EXPECT_TRUE(table.HasUdf());
}

// Make sure that MarkUdfMatchField marks the UDF match field fails if the match
// field is unknown.
TEST(AclTableTest, SetUdfSetId_FieldLookupFailure) {
  ::p4::config::Table p4_table = DefaultP4Table();
  // Create & initialize the ACL table.
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);

  ASSERT_FALSE(table.MarkUdfMatchField(99, 999).ok());
  EXPECT_FALSE(table.HasUdf());
  EXPECT_FALSE(table.IsUdfField(99));
}

// Make sure that MarkUdfMatchField marks the UDF match field fails if the UDF
// set ID changes.
TEST(AclTableTest, SetUdfSetId_UdfSetIdOverwriteFailure) {
  ::p4::config::Table p4_table = DefaultP4Table();
  // Create & initialize the ACL table.
  AclTable table(p4_table, P4Annotation::INGRESS_ACL, 12);

  ASSERT_OK(table.MarkUdfMatchField(p4_table.match_fields(0).id(), 999));
  ASSERT_FALSE(table.MarkUdfMatchField(p4_table.match_fields(1).id(), 9).ok());

  EXPECT_TRUE(table.HasUdf());
  EXPECT_EQ(table.UdfSetId(), 999);
  EXPECT_TRUE(table.IsUdfField(p4_table.match_fields(0).id()));
  EXPECT_FALSE(table.IsUdfField(p4_table.match_fields(1).id()));
}

}  // namespace
}  // namespace bcm
}  // namespace hal
}  // namespace stratum
