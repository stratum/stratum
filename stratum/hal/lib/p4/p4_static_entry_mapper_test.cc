// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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


// This file contains unit tests for P4StaticEntryMapper.

#include "stratum/hal/lib/p4/p4_static_entry_mapper.h"

#include <memory>
#include <vector>

#include "gflags/gflags.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/p4/p4_table_mapper_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "p4/v1/p4runtime.pb.h"

DECLARE_bool(remap_hidden_table_const_entries);

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::HasSubstr;
using ::testing::Return;

namespace stratum {
namespace hal {

// This unnamed namespace hides the text strings that parse into static table
// update entries for testing.  The tested updates don't need to have complete
// content, they just use the table ID to distinguish one update from another.
namespace {

const char* kTestUpdatePhysical1 = R"(  # Static physical table.
  type: INSERT
  entity {
    table_entry {
      table_id: 0x001
    }
  }
)";

const char* kTestUpdatePhysical2 = R"(  # Static physical table.
  type: INSERT
  entity {
    table_entry {
      table_id: 0x002
    }
  }
)";

const char* kTestUpdateHidden1 = R"(  # Static hidden table.
  type: INSERT
  entity {
    table_entry {
      table_id: 0x101
    }
  }
)";

const char* kTestUpdateHidden2 = R"(  # Static hidden table.
  type: INSERT
  entity {
    table_entry {
      table_id: 0x102
    }
  }
)";

}  // namespace

// This class is the P4StaticEntryMapper test fixture.
class P4StaticEntryMapperTest : public testing::Test {
 protected:
  P4StaticEntryMapperTest() {
    test_mapper_ =
        absl::make_unique<P4StaticEntryMapper>(&mock_p4_table_mapper_);
  }

  // Adds update entries to pipeline_static_entries_ by parsing each string in
  // updates_text.
  void SetUpTestRequest(const std::vector<const char*>& updates_text);

  // Sets up mock_p4_table_mapper_ expectations to distinguish between
  // hidden and physical tables.
  void SetUpHiddenTableExpectations();

  // Puts test_mapper_ into a state where the initial pipeline config was
  // pushed with static entries parsed from updates_text.
  void SetUpFirstPipelinePush(const std::vector<const char*>& updates_text);

  // This P4TableMapperMock is injected into test_mapper_.
  P4TableMapperMock mock_p4_table_mapper_;

  // This is a common P4StaticEntryMapper for test use.
  std::unique_ptr<P4StaticEntryMapper> test_mapper_;

  // Messages for test use.
  ::p4::v1::WriteRequest pipeline_static_entries_;
  ::p4::v1::WriteRequest test_request_out_;
};

void P4StaticEntryMapperTest::SetUpTestRequest(
    const std::vector<const char*>& updates_text) {
  pipeline_static_entries_.Clear();
  for (auto update_text : updates_text) {
    ::p4::v1::Update update;
    ASSERT_OK(ParseProtoFromString(
        update_text, pipeline_static_entries_.add_updates()));
  }
}

// Table IDs for IsTableStageHidden match the table entries defined by the
// test update strings near the beginning of this file.
void P4StaticEntryMapperTest::SetUpHiddenTableExpectations() {
  EXPECT_CALL(mock_p4_table_mapper_, IsTableStageHidden(0x101))
      .Times(AnyNumber())
      .WillRepeatedly(Return(TRI_STATE_TRUE));
  EXPECT_CALL(mock_p4_table_mapper_, IsTableStageHidden(0x102))
      .Times(AnyNumber())
      .WillRepeatedly(Return(TRI_STATE_TRUE));
  EXPECT_CALL(mock_p4_table_mapper_, IsTableStageHidden(0x001))
      .Times(AnyNumber())
      .WillRepeatedly(Return(TRI_STATE_FALSE));
  EXPECT_CALL(mock_p4_table_mapper_, IsTableStageHidden(0x002))
      .Times(AnyNumber())
      .WillRepeatedly(Return(TRI_STATE_FALSE));
}

void P4StaticEntryMapperTest::SetUpFirstPipelinePush(
    const std::vector<const char*>& updates_text) {
  SetUpTestRequest(updates_text);
  SetUpHiddenTableExpectations();
  ASSERT_OK(test_mapper_->HandlePrePushChanges(
      pipeline_static_entries_, &test_request_out_));
  ASSERT_OK(test_mapper_->HandlePostPushChanges(
      pipeline_static_entries_, &test_request_out_));
}

// Verifies that the output P4 WriteRequest is cleared when no static
// entry deletions are needed.
TEST_F(P4StaticEntryMapperTest, TestClearDelete) {
  test_request_out_.add_updates();
  EXPECT_OK(test_mapper_->HandlePrePushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(0, test_request_out_.updates_size());
}

// Verifies that the output P4 WriteRequest is cleared when no static
// entry additions are needed.
TEST_F(P4StaticEntryMapperTest, TestClearAdd) {
  test_request_out_.add_updates();
  EXPECT_OK(test_mapper_->HandlePostPushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(0, test_request_out_.updates_size());
}

// The first push should have no deletions and two additions.  One addition
// is a hidden table that does not appear in the output.
TEST_F(P4StaticEntryMapperTest, TestFirstPipelinePush) {
  SetUpTestRequest(
      {kTestUpdatePhysical1, kTestUpdatePhysical2, kTestUpdateHidden1});

  // For the first push, the table mapper won't be ready to distinguish hidden
  // vs. non-hidden tables.
  EXPECT_CALL(mock_p4_table_mapper_, IsTableStageHidden(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(TRI_STATE_UNKNOWN));
  EXPECT_OK(test_mapper_->HandlePrePushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(0, test_request_out_.updates_size());

  // According to the standard pipeline push sequence, the table mapper should
  // now be ready, so set normal expectations.
  SetUpHiddenTableExpectations();
  EXPECT_OK(test_mapper_->HandlePostPushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(2, test_request_out_.updates_size());
}

// A pipeline push that does not change any static entries should produce
// empty write requests.
TEST_F(P4StaticEntryMapperTest, TestUnchangedPipelinePush) {
  SetUpFirstPipelinePush({kTestUpdatePhysical1, kTestUpdateHidden1});
  EXPECT_OK(test_mapper_->HandlePrePushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(0, test_request_out_.updates_size());
  EXPECT_OK(test_mapper_->HandlePostPushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(0, test_request_out_.updates_size());
}

// A pipeline push that adds a physical static entry should produce
// output from HandlePostPushChanges.
TEST_F(P4StaticEntryMapperTest, TestPipelinePushPhysicalAddition) {
  SetUpFirstPipelinePush({kTestUpdatePhysical2, kTestUpdateHidden2});

  SetUpTestRequest(  // Adds kTestUpdatePhysical1.
      {kTestUpdatePhysical1, kTestUpdatePhysical2, kTestUpdateHidden1});
  EXPECT_OK(test_mapper_->HandlePrePushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(0, test_request_out_.updates_size());
  EXPECT_OK(test_mapper_->HandlePostPushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(1, test_request_out_.updates_size());
}

// A pipeline push that adds a hidden static entry should not
// produce output from HandlePostPushChanges.
TEST_F(P4StaticEntryMapperTest, TestPipelinePushHiddenAddition) {
  SetUpFirstPipelinePush({kTestUpdatePhysical1, kTestUpdateHidden2});

  SetUpTestRequest(  // Adds kTestUpdateHidden1.
      {kTestUpdatePhysical1, kTestUpdateHidden1, kTestUpdateHidden2});
  EXPECT_OK(test_mapper_->HandlePrePushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(0, test_request_out_.updates_size());
  EXPECT_OK(test_mapper_->HandlePostPushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(0, test_request_out_.updates_size());
}

// A pipeline push that deletes a physical static entry should
// produce output from HandlePrePushChanges.
TEST_F(P4StaticEntryMapperTest, TestPipelinePushPhysicalDeletion) {
  SetUpFirstPipelinePush(
      {kTestUpdatePhysical1, kTestUpdatePhysical2, kTestUpdateHidden1});

  // Deletes kTestUpdatePhysical2.
  SetUpTestRequest({kTestUpdatePhysical1, kTestUpdateHidden1});
  EXPECT_OK(test_mapper_->HandlePrePushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(1, test_request_out_.updates_size());
  EXPECT_OK(test_mapper_->HandlePostPushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(0, test_request_out_.updates_size());
}

// A pipeline push that deletes a hidden static entry should not
// produce output from HandlePrePushChanges.
TEST_F(P4StaticEntryMapperTest, TestPipelinePushHiddenDeletion) {
  SetUpFirstPipelinePush(
      {kTestUpdatePhysical1, kTestUpdateHidden1, kTestUpdateHidden2});

  // Deletes kTestUpdateHidden2.
  SetUpTestRequest({kTestUpdatePhysical1, kTestUpdateHidden1});
  EXPECT_OK(test_mapper_->HandlePrePushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(0, test_request_out_.updates_size());
  EXPECT_OK(test_mapper_->HandlePostPushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(0, test_request_out_.updates_size());
}

// A malformed P4 WriteRequest input should produce an error status.
TEST_F(P4StaticEntryMapperTest, TestPipelinePushBadStaticWriteRequest) {
  SetUpFirstPipelinePush(
      {kTestUpdatePhysical1, kTestUpdateHidden1, kTestUpdateHidden2});
  pipeline_static_entries_.add_updates();  // Add update with no table_entry.
  ::util::Status status = test_mapper_->HandlePrePushChanges(
      pipeline_static_entries_, &test_request_out_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("P4 WriteRequest has no table_entry"));
}

// Calling HandlePostPushChanges without HandlePrePushChanges should
// cause an error.
TEST_F(P4StaticEntryMapperTest, TestPipelinePushOutOfOrderPhysical) {
  SetUpFirstPipelinePush(
      {kTestUpdatePhysical1, kTestUpdateHidden1, kTestUpdateHidden2});

  // Deletes kTestUpdatePhysical1.
  SetUpTestRequest({kTestUpdateHidden1, kTestUpdateHidden2});
  ::util::Status status = test_mapper_->HandlePostPushChanges(
      pipeline_static_entries_, &test_request_out_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Unexpected physical static table entry deletions"));
}

// Calling HandlePostPushChanges without HandlePrePushChanges should
// cause an error.
TEST_F(P4StaticEntryMapperTest, TestPipelinePushOutOfOrderHidden) {
  SetUpFirstPipelinePush(
      {kTestUpdatePhysical1, kTestUpdateHidden1, kTestUpdateHidden2});

  // Deletes kTestUpdateHidden1.
  SetUpTestRequest({kTestUpdatePhysical1, kTestUpdateHidden2});
  ::util::Status status = test_mapper_->HandlePostPushChanges(
      pipeline_static_entries_, &test_request_out_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Unexpected hidden static table entry deletions"));
}

// Tests disable of remapping of hidden tables.
TEST_F(P4StaticEntryMapperTest, TestNoHiddenRemap) {
  FLAGS_remap_hidden_table_const_entries = false;
  SetUpFirstPipelinePush({kTestUpdatePhysical1});

  // Adds kTestUpdateHidden2.
  SetUpTestRequest({kTestUpdatePhysical1, kTestUpdateHidden2});
  EXPECT_OK(test_mapper_->HandlePrePushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(0, test_request_out_.updates_size());
  EXPECT_OK(test_mapper_->HandlePostPushChanges(
      pipeline_static_entries_, &test_request_out_));
  EXPECT_EQ(1, test_request_out_.updates_size());
}

}  // namespace hal
}  // namespace stratum
