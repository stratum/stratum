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

// This file contains P4WriteRequestDiffer unit tests.

#include "stratum/hal/lib/p4/p4_write_request_differ.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "sandblaze/p4lang/p4/p4runtime.pb.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/utils.h"

using ::testing::HasSubstr;

namespace stratum {
namespace hal {

// This unnamed namespace hides the text strings that parse into static table
// update entries for testing.  The test strings are pasted from static entry
// output in the generated P4PipelineConfig for Hercules tor.p4.
namespace {

const char* kTestUpdate1 = R"(
  type: INSERT
  entity {
    table_entry {
      table_id: 33619021
      match {
        field_id: 1
        exact {
          value: "\003"
        }
      }
      action {
        action {
          action_id: 16804726
        }
      }
    }
  }
)";

const char* kTestUpdate2 = R"(
  type: INSERT
  entity {
    table_entry {
      table_id: 33576594
      match {
        field_id: 1
        exact {
          value: "\010\000"
        }
      }
      match {
        field_id: 2
        ternary {
          value: "\000"
          mask: "\340"
        }
      }
      action {
        action {
          action_id: 16781933
        }
      }
      priority: 5
    }
  }
)";

const char* kTestUpdate3 = R"(
  type: INSERT
  entity {
    table_entry {
      table_id: 33580322
      match {
        field_id: 1
        exact {
          value: "\003"
        }
      }
      action {
        action {
          action_id: 16789619
        }
      }
    }
  }
)";

}  // namespace

// This class is the P4WriteRequestDiffer test fixture.
class P4WriteRequestDifferTest : public testing::Test {
 protected:
  P4WriteRequestDifferTest()
      : three_text_updates_({kTestUpdate1, kTestUpdate2, kTestUpdate3}) {}

  // Adds update entries to the output WriteRequest by parsing each string in
  // updates_text.
  static void SetUpTestRequest(const std::vector<const char*>& updates_text,
                               p4::WriteRequest* request);

  // Tests can use these WriteRequests to set up inputs for testing
  // P4WriteRequestDiffer.
  p4::WriteRequest old_request_;
  p4::WriteRequest new_request_;

  // The P4WriteRequestDifferTest constructor sets up this vector for
  // common use as a SetUpTestRequest input.
  const std::vector<const char*> three_text_updates_;

  // Tests can use these WriteRequests to store output from
  // P4WriteRequestDiffer::Compare.
  p4::WriteRequest additions_;
  p4::WriteRequest deletions_;
  p4::WriteRequest modified_;
  p4::WriteRequest unchanged_;

  // Tests can use this MessageDifferencer to verify test expectations.
  ::google::protobuf::util::MessageDifferencer msg_differencer_;
};

void P4WriteRequestDifferTest::SetUpTestRequest(
    const std::vector<const char*>& updates_text, p4::WriteRequest* request) {
  request->Clear();
  for (auto update_text : updates_text) {
    p4::Update update;
    ASSERT_OK(ParseProtoFromString(update_text, &update));
    *request->add_updates() = update;
  }
}

TEST_F(P4WriteRequestDifferTest, TestEmptyRequests) {
  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  EXPECT_EQ(0, deletions_.updates_size());
  EXPECT_EQ(0, additions_.updates_size());
  EXPECT_EQ(0, modified_.updates_size());
  EXPECT_EQ(0, unchanged_.updates_size());
}

// Tests no change between old_request_ and new_request_.
TEST_F(P4WriteRequestDifferTest, TestNoChange) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  new_request_ = old_request_;

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  EXPECT_EQ(0, deletions_.updates_size());
  EXPECT_EQ(0, additions_.updates_size());
  EXPECT_EQ(0, modified_.updates_size());
  EXPECT_EQ(old_request_.updates_size(), unchanged_.updates_size());
  EXPECT_TRUE(msg_differencer_.Compare(old_request_, unchanged_));
}

// Tests reordering of entries between old_request_ and new_request_ without
// any changes to content.
TEST_F(P4WriteRequestDifferTest, TestReorder) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_EQ(3, old_request_.updates_size());
  *new_request_.add_updates() = old_request_.updates(2);
  *new_request_.add_updates() = old_request_.updates(1);
  *new_request_.add_updates() = old_request_.updates(0);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  EXPECT_EQ(0, deletions_.updates_size());
  EXPECT_EQ(0, additions_.updates_size());
  EXPECT_EQ(0, modified_.updates_size());
  EXPECT_EQ(old_request_.updates_size(), unchanged_.updates_size());
  EXPECT_TRUE(msg_differencer_.Compare(old_request_, unchanged_));
}

// Tests addition of a static entry in new_request_.
TEST_F(P4WriteRequestDifferTest, TestAddTableEntry) {
  SetUpTestRequest(three_text_updates_, &new_request_);
  ASSERT_EQ(3, new_request_.updates_size());
  *old_request_.add_updates() = new_request_.updates(0);
  *old_request_.add_updates() = new_request_.updates(2);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  EXPECT_EQ(0, deletions_.updates_size());
  ASSERT_EQ(1, additions_.updates_size());
  EXPECT_TRUE(
      msg_differencer_.Compare(new_request_.updates(1), additions_.updates(0)));
  EXPECT_EQ(0, modified_.updates_size());
  EXPECT_EQ(2, unchanged_.updates_size());
  EXPECT_TRUE(msg_differencer_.Compare(old_request_, unchanged_));
}

// Tests addition of multiple static entries in new_request_.
TEST_F(P4WriteRequestDifferTest, TestAddMultipleTableEntries) {
  SetUpTestRequest(three_text_updates_, &new_request_);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  EXPECT_EQ(0, deletions_.updates_size());
  ASSERT_EQ(3, additions_.updates_size());
  msg_differencer_.set_repeated_field_comparison(
      ::google::protobuf::util::MessageDifferencer::AS_SET);
  EXPECT_TRUE(msg_differencer_.Compare(new_request_, additions_));
  EXPECT_EQ(0, modified_.updates_size());
  EXPECT_EQ(0, unchanged_.updates_size());
}

// Tests deletion of a static entry in new_request_.
TEST_F(P4WriteRequestDifferTest, TestDeleteTableEntry) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_EQ(3, old_request_.updates_size());
  *new_request_.add_updates() = old_request_.updates(0);
  *new_request_.add_updates() = old_request_.updates(2);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  ASSERT_EQ(1, deletions_.updates_size());
  EXPECT_EQ(0, additions_.updates_size());
  p4::Update expected_update = old_request_.updates(1);
  expected_update.set_type(p4::Update::DELETE);
  EXPECT_TRUE(msg_differencer_.Compare(expected_update, deletions_.updates(0)));
  EXPECT_EQ(0, modified_.updates_size());
  EXPECT_EQ(2, unchanged_.updates_size());
  EXPECT_TRUE(msg_differencer_.Compare(new_request_, unchanged_));
}

// Tests deletion of multiple static entries in new_request_.
TEST_F(P4WriteRequestDifferTest, TestDeleteMultipleTableEntries) {
  SetUpTestRequest(three_text_updates_, &old_request_);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  ASSERT_EQ(3, deletions_.updates_size());
  EXPECT_EQ(0, additions_.updates_size());
  msg_differencer_.set_repeated_field_comparison(
      ::google::protobuf::util::MessageDifferencer::AS_SET);
  p4::WriteRequest expected_deletes = old_request_;
  expected_deletes.mutable_updates(0)->set_type(p4::Update::DELETE);
  expected_deletes.mutable_updates(1)->set_type(p4::Update::DELETE);
  expected_deletes.mutable_updates(2)->set_type(p4::Update::DELETE);
  EXPECT_TRUE(msg_differencer_.Compare(expected_deletes, deletions_));
  EXPECT_EQ(0, modified_.updates_size());
  EXPECT_EQ(0, unchanged_.updates_size());
}

// Tests changing an action ID in one of the new_request_ entries.
TEST_F(P4WriteRequestDifferTest, TestModifyActionID) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_LE(1, old_request_.updates_size());
  new_request_ = old_request_;
  auto modify_entity = new_request_.mutable_updates(0)->mutable_entity();
  auto modify_action = modify_entity->mutable_table_entry()->mutable_action();
  modify_action->mutable_action()->set_action_id(
      modify_action->action().action_id() + 1);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  ASSERT_EQ(0, deletions_.updates_size());
  EXPECT_EQ(0, additions_.updates_size());
  ASSERT_EQ(1, modified_.updates_size());
  p4::Update expected_mod = new_request_.updates(0);
  expected_mod.set_type(p4::Update::MODIFY);
  EXPECT_TRUE(msg_differencer_.Compare(expected_mod, modified_.updates(0)));
  ASSERT_EQ(2, unchanged_.updates_size());
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(1), unchanged_.updates(0)));
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(2), unchanged_.updates(1)));
}

// Tests adding an additional match field in one of the new_request_ entries.
TEST_F(P4WriteRequestDifferTest, TestModifyAddMatchField) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_EQ(3, old_request_.updates_size());
  new_request_ = old_request_;
  auto modify_entity = new_request_.mutable_updates(2)->mutable_entity();
  auto new_match = modify_entity->mutable_table_entry()->add_match();
  new_match->set_field_id(2);
  new_match->mutable_exact()->set_value("new-value");

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  ASSERT_EQ(1, deletions_.updates_size());
  ASSERT_EQ(1, additions_.updates_size());
  p4::Update expected_update = old_request_.updates(2);
  expected_update.set_type(p4::Update::DELETE);
  EXPECT_TRUE(msg_differencer_.Compare(expected_update, deletions_.updates(0)));
  expected_update = new_request_.updates(2);
  expected_update.set_type(p4::Update::INSERT);
  EXPECT_TRUE(msg_differencer_.Compare(expected_update, additions_.updates(0)));
  EXPECT_EQ(0, modified_.updates_size());
  ASSERT_EQ(2, unchanged_.updates_size());
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(0), unchanged_.updates(0)));
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(1), unchanged_.updates(1)));
}

// Tests removal of a match field in one of the new_request_ entries.
TEST_F(P4WriteRequestDifferTest, TestModifyDeleteMatchField) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_EQ(3, old_request_.updates_size());
  new_request_ = old_request_;
  auto modify_entity = old_request_.mutable_updates(2)->mutable_entity();
  modify_entity->mutable_table_entry()->add_match()->set_field_id(2);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  ASSERT_EQ(1, deletions_.updates_size());
  ASSERT_EQ(1, additions_.updates_size());
  p4::Update expected_update = old_request_.updates(2);
  expected_update.set_type(p4::Update::DELETE);
  EXPECT_TRUE(msg_differencer_.Compare(expected_update, deletions_.updates(0)));
  expected_update = new_request_.updates(2);
  expected_update.set_type(p4::Update::INSERT);
  EXPECT_TRUE(msg_differencer_.Compare(expected_update, additions_.updates(0)));
  EXPECT_EQ(0, modified_.updates_size());
  ASSERT_EQ(2, unchanged_.updates_size());
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(0), unchanged_.updates(0)));
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(1), unchanged_.updates(1)));
}

// Tests changing an action ID in one of the new_request_ entries while
// reordering all the entries at the same time.
TEST_F(P4WriteRequestDifferTest, TestModifyActionIDAndReorder) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_EQ(3, old_request_.updates_size());
  *new_request_.add_updates() = old_request_.updates(2);
  *new_request_.add_updates() = old_request_.updates(1);
  *new_request_.add_updates() = old_request_.updates(0);
  auto modify_entity = new_request_.mutable_updates(0)->mutable_entity();
  auto modify_action = modify_entity->mutable_table_entry()->mutable_action();
  modify_action->mutable_action()->set_action_id(
      modify_action->action().action_id() + 1);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  EXPECT_EQ(0, deletions_.updates_size());
  EXPECT_EQ(0, additions_.updates_size());
  ASSERT_EQ(1, modified_.updates_size());
  p4::Update expected_update = new_request_.updates(0);
  expected_update.set_type(p4::Update::MODIFY);
  EXPECT_TRUE(msg_differencer_.Compare(expected_update, modified_.updates(0)));
  ASSERT_EQ(2, unchanged_.updates_size());
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(0), unchanged_.updates(0)));
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(1), unchanged_.updates(1)));
}

// Tests changing a table ID in one of the new_request_ entries.
TEST_F(P4WriteRequestDifferTest, TestModifyTableID) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_EQ(3, old_request_.updates_size());
  new_request_ = old_request_;
  auto modify_entity = new_request_.mutable_updates(2)->mutable_entity();
  modify_entity->mutable_table_entry()->set_table_id(
      modify_entity->table_entry().table_id() + 1);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  ASSERT_EQ(1, deletions_.updates_size());
  ASSERT_EQ(1, additions_.updates_size());
  p4::Update expected_update = old_request_.updates(2);
  expected_update.set_type(p4::Update::DELETE);
  EXPECT_TRUE(msg_differencer_.Compare(expected_update, deletions_.updates(0)));
  expected_update = new_request_.updates(2);
  expected_update.set_type(p4::Update::INSERT);
  EXPECT_TRUE(msg_differencer_.Compare(expected_update, additions_.updates(0)));
  EXPECT_EQ(0, modified_.updates_size());
  ASSERT_EQ(2, unchanged_.updates_size());
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(0), unchanged_.updates(0)));
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(1), unchanged_.updates(1)));
}

// Tests changing the mask value of a match in one of the new_request_ entries.
TEST_F(P4WriteRequestDifferTest, TestModifyMatchValue) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_EQ(3, old_request_.updates_size());
  new_request_ = old_request_;
  auto modify_entity = new_request_.mutable_updates(1)->mutable_entity();
  auto modify_match = modify_entity->mutable_table_entry()->mutable_match(1);
  modify_match->mutable_ternary()->set_mask("modified-mask");

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  ASSERT_EQ(1, deletions_.updates_size());
  ASSERT_EQ(1, additions_.updates_size());
  p4::Update expected_update = old_request_.updates(1);
  expected_update.set_type(p4::Update::DELETE);
  EXPECT_TRUE(msg_differencer_.Compare(expected_update, deletions_.updates(0)));
  expected_update = new_request_.updates(1);
  expected_update.set_type(p4::Update::INSERT);
  EXPECT_TRUE(msg_differencer_.Compare(expected_update, additions_.updates(0)));
  EXPECT_EQ(0, modified_.updates_size());
  ASSERT_EQ(2, unchanged_.updates_size());
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(0), unchanged_.updates(0)));
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(2), unchanged_.updates(1)));
}

// Tests reordering the match fields in one entry update.
TEST_F(P4WriteRequestDifferTest, TestReorderMatchFields) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_EQ(3, old_request_.updates_size());
  new_request_ = old_request_;
  auto modify_entry =
      new_request_.mutable_updates(1)->mutable_entity()->mutable_table_entry();
  ASSERT_EQ(2, modify_entry->match_size());
  p4::FieldMatch swap_match = modify_entry->match(0);
  *modify_entry->mutable_match(0) = modify_entry->match(1);
  *modify_entry->mutable_match(1) = swap_match;

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  EXPECT_EQ(0, deletions_.updates_size());
  EXPECT_EQ(0, additions_.updates_size());
  EXPECT_EQ(0, modified_.updates_size());
  EXPECT_EQ(3, unchanged_.updates_size());
}

// Tests nullptr input for the deletions request.
TEST_F(P4WriteRequestDifferTest, TestNullDeletions) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_EQ(3, old_request_.updates_size());

  // One table_id is modified and one priority is adjusted so all output
  // requests have entries.
  new_request_ = old_request_;
  auto modify_entity = new_request_.mutable_updates(2)->mutable_entity();
  modify_entity->mutable_table_entry()->set_table_id(
      modify_entity->table_entry().table_id() + 1);
  modify_entity = new_request_.mutable_updates(1)->mutable_entity();
  modify_entity->mutable_table_entry()->set_priority(0xabc);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(test_differ.Compare(nullptr, &additions_, &modified_, &unchanged_));
  EXPECT_EQ(1, additions_.updates_size());
  EXPECT_EQ(1, modified_.updates_size());
  ASSERT_EQ(1, unchanged_.updates_size());
}

// Tests nullptr input for the additions request.
TEST_F(P4WriteRequestDifferTest, TestNullAdditions) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_EQ(3, old_request_.updates_size());

  // One table_id is modified and one priority is adjusted so all output
  // requests have entries.
  new_request_ = old_request_;
  auto modify_entity = new_request_.mutable_updates(2)->mutable_entity();
  modify_entity->mutable_table_entry()->set_table_id(
      modify_entity->table_entry().table_id() + 1);
  modify_entity = new_request_.mutable_updates(1)->mutable_entity();
  modify_entity->mutable_table_entry()->set_priority(0xabc);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(test_differ.Compare(&deletions_, nullptr, &modified_, &unchanged_));
  EXPECT_EQ(1, deletions_.updates_size());
  EXPECT_EQ(1, modified_.updates_size());
  ASSERT_EQ(1, unchanged_.updates_size());
}

// Tests nullptr input for the modified request.
TEST_F(P4WriteRequestDifferTest, TestNullModified) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_EQ(3, old_request_.updates_size());

  // One table_id is modified and one priority is adjusted so all output
  // requests have entries.
  new_request_ = old_request_;
  auto modify_entity = new_request_.mutable_updates(2)->mutable_entity();
  modify_entity->mutable_table_entry()->set_table_id(
      modify_entity->table_entry().table_id() + 1);
  modify_entity = new_request_.mutable_updates(1)->mutable_entity();
  modify_entity->mutable_table_entry()->set_priority(0xabc);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, nullptr, &unchanged_));
  EXPECT_EQ(1, deletions_.updates_size());
  EXPECT_EQ(1, additions_.updates_size());
  ASSERT_EQ(1, unchanged_.updates_size());
}

// Tests nullptr input for the unchanged request.
TEST_F(P4WriteRequestDifferTest, TestNullUnchanged) {
  SetUpTestRequest(three_text_updates_, &old_request_);
  ASSERT_EQ(3, old_request_.updates_size());

  // One table_id is modified and one priority is adjusted so all output
  // requests have entries.
  new_request_ = old_request_;
  auto modify_entity = new_request_.mutable_updates(2)->mutable_entity();
  modify_entity->mutable_table_entry()->set_table_id(
      modify_entity->table_entry().table_id() + 1);
  modify_entity = new_request_.mutable_updates(1)->mutable_entity();
  modify_entity->mutable_table_entry()->set_priority(0xabc);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(test_differ.Compare(&deletions_, &additions_, &modified_, nullptr));
  EXPECT_EQ(1, deletions_.updates_size());
  EXPECT_EQ(1, modified_.updates_size());
  EXPECT_EQ(1, additions_.updates_size());
}

// Tests new_request with one add and one modification at the same time.
TEST_F(P4WriteRequestDifferTest, TestModifyAndAdd) {
  SetUpTestRequest(three_text_updates_, &new_request_);
  ASSERT_EQ(3, new_request_.updates_size());

  // The code below makes new_request_[0] unchanged, new_request_[1] an
  // addition, and new_request_[2] a modification.
  *old_request_.add_updates() = new_request_.updates(0);
  *old_request_.add_updates() = new_request_.updates(2);
  auto modify_entity = new_request_.mutable_updates(2)->mutable_entity();
  auto modify_action = modify_entity->mutable_table_entry()->mutable_action();
  modify_action->mutable_action()->set_action_id(
      modify_action->action().action_id() + 1);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  ASSERT_EQ(0, deletions_.updates_size());
  ASSERT_EQ(1, additions_.updates_size());
  p4::Update expected_update = new_request_.updates(1);
  expected_update.set_type(p4::Update::INSERT);
  EXPECT_TRUE(msg_differencer_.Compare(expected_update, additions_.updates(0)));
  ASSERT_EQ(1, modified_.updates_size());
  expected_update = new_request_.updates(2);
  expected_update.set_type(p4::Update::MODIFY);
  EXPECT_TRUE(msg_differencer_.Compare(expected_update, modified_.updates(0)));
  ASSERT_EQ(1, unchanged_.updates_size());
  EXPECT_TRUE(
      msg_differencer_.Compare(old_request_.updates(0), unchanged_.updates(0)));
}

// Verifies that the update type is ignored during comparisons.
TEST_F(P4WriteRequestDifferTest, TestIgnoreUpdateType) {
  SetUpTestRequest(three_text_updates_, &new_request_);
  ASSERT_EQ(3, new_request_.updates_size());
  old_request_ = new_request_;
  new_request_.mutable_updates(0)->set_type(p4::Update::MODIFY);
  new_request_.mutable_updates(1)->set_type(p4::Update::DELETE);
  new_request_.mutable_updates(2)->set_type(p4::Update::UNSPECIFIED);

  P4WriteRequestDiffer test_differ(old_request_, new_request_);
  EXPECT_OK(
      test_differ.Compare(&deletions_, &additions_, &modified_, &unchanged_));
  EXPECT_EQ(0, deletions_.updates_size());
  EXPECT_EQ(0, additions_.updates_size());
  EXPECT_EQ(0, modified_.updates_size());
  EXPECT_EQ(3, unchanged_.updates_size());
}

}  // namespace hal
}  // namespace stratum
