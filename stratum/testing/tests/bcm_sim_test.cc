// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/testing/tests/bcm_sim_test_fixture.h"

namespace stratum {

namespace hal {
namespace bcm {

class BcmSimTest : public BcmSimTestFixture {
 protected:
  // A buffer class for collecting forwarding entries from the device.
  class ReadResponseBuffer : public WriterInterface<::p4::v1::ReadResponse> {
   public:
    explicit ReadResponseBuffer(std::vector<::p4::v1::ReadResponse>* responses)
        : responses_(ABSL_DIE_IF_NULL(responses)) {
      responses_->clear();
    }
    bool Write(const ::p4::v1::ReadResponse& response) override {
      if (responses_) {
        responses_->push_back(response);
        return true;
      }
      return false;
    }

   private:
    std::vector<::p4::v1::ReadResponse>* responses_;  // not owned by the class.
  };

  BcmSimTest() {}
  ~BcmSimTest() override {}
};

TEST_F(BcmSimTest, TestBasicFunctionality) {
  // Push the FowardingPipelineConfig proto.
  ASSERT_OK(bcm_switch_->PushForwardingPipelineConfig(
      kNodeId, forwarding_pipeline_config_))
      << "Failed to push forwarding_pipeline_config:\n"
      << forwarding_pipeline_config_.DebugString();

  // Program the forwarding entries.
  std::vector<::util::Status> results;
  ::util::Status status =
      bcm_switch_->WriteForwardingEntries(write_request_, &results);
  if (!status.ok()) {
    std::string msg = "Failed to write forwarding entries. Results:";
    for (const auto& r : results) {
      absl::StrAppend(&msg, "\n>>> ",
                      r.error_message().empty() ? "None" : r.error_message());
    }
    FAIL() << msg;
  }

  // Read the forwarding entries back.
  ::p4::v1::ReadRequest request;
  std::vector<::p4::v1::ReadResponse> responses;
  std::vector<::util::Status> details = {};
  ReadResponseBuffer buffer(&responses);
  request.set_device_id(kNodeId);
  request.add_entities()->mutable_table_entry();
  request.add_entities()->mutable_action_profile_group();
  request.add_entities()->mutable_action_profile_member();
  status = bcm_switch_->ReadForwardingEntries(request, &buffer, &details);
  if (!status.ok()) {
    std::string msg = "Failed to read forwarding entries. Details:";
    for (const auto& d : details) {
      absl::StrAppend(&msg, "\n>>> ",
                      d.error_message().empty() ? "None" : d.error_message());
    }
    FAIL() << msg;
  }
  if (!responses.empty()) {
    std::string msg = "Read the following forwarding entries:";
    for (const auto& r : responses) {
      absl::StrAppend(&msg, "\n>>> ", r.ShortDebugString());
    }
    LOG(INFO) << msg;
  } else {
    FAIL() << "No forwarding entry read from the node.";
  }
}

}  // namespace bcm
}  // namespace hal

}  // namespace stratum
