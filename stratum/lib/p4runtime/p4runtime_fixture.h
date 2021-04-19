// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_P4RUNTIME_P4RUNTIME_FIXTURE_H_
#define STRATUM_LIB_P4RUNTIME_P4RUNTIME_FIXTURE_H_

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "p4/v1/p4runtime.pb.h"
// #include "stratum/hal/lib/common/common.pb.h"
// #include "stratum/hal/lib/p4/p4_table_mapper.h"
// #include "stratum/hal/lib/phal/phal_sim.h"
#include "stratum/lib/p4runtime/p4runtime_session.h"
#include "stratum/lib/utils.h"

DEFINE_string(grpc_addr, "127.0.0.1:9339", "P4Runtime server address.");
DEFINE_string(p4_info_file, "",
              "Path to an optional P4Info text proto file. If specified, file "
              "content will be serialized into the p4info field in "
              "ForwardingPipelineConfig proto and pushed to the switch.");
DEFINE_string(p4_pipeline_config_file, "",
              "Path to an optional P4PipelineConfig bin proto file. If "
              "specified, file content will be serialized into the "
              "p4_device_config field in ForwardingPipelineConfig proto "
              "and pushed to the switch.");
DEFINE_uint64(device_id, 1, "P4Runtime device ID.");

namespace stratum {
namespace p4runtime {

//
using p4runtime::P4RuntimeSession;

class P4RuntimeFixture : public testing::Test {
 protected:
  static void SetUpTestSuite() { LOG(WARNING) << "SetUpTestSuite"; }
  static void TearDownTestSuite() { LOG(WARNING) << "TearDownTestSuite"; }
  P4RuntimeFixture() {}
  ~P4RuntimeFixture() override {}

  void SetUp() override {
    // Initialize the connection.
    ASSERT_OK_AND_ASSIGN(
        sut_p4rt_session_,
        P4RuntimeSession::Create(FLAGS_grpc_addr,
                                 ::grpc::InsecureChannelCredentials(),
                                 FLAGS_device_id));

    ASSERT_FALSE(FLAGS_p4_info_file.empty());
    ASSERT_FALSE(FLAGS_p4_pipeline_config_file.empty());
    ASSERT_OK(ReadProtoFromTextFile(FLAGS_p4_info_file, &p4info_));
    std::string p4_device_config;
    ASSERT_OK(
        ReadFileToString(FLAGS_p4_pipeline_config_file, &p4_device_config));
    LOG(ERROR) << "Pushing pipeline";
    ASSERT_OK(SetForwardingPipelineConfig(sut_p4rt_session_.get(), p4info_,
                                          p4_device_config));

    // Clear entries here in case the previous test did not (e.g. because it
    // crashed).
    ASSERT_OK(ClearTableEntries(sut_p4rt_session_.get()));
    // Check that switch is in a clean state.
    ASSERT_OK_AND_ASSIGN(auto read_back_entries,
                         ReadTableEntries(sut_p4rt_session_.get()));
    ASSERT_EQ(read_back_entries.size(), 0);
  }

  void TearDown() override {
    if (SutP4RuntimeSession() != nullptr) {
      // Clear all table entries to leave the switch in a clean state.
      EXPECT_OK(ClearTableEntries(SutP4RuntimeSession()));
    }
  }

  P4RuntimeSession* SutP4RuntimeSession() const {
    return sut_p4rt_session_.get();
  }

  const ::p4::config::v1::P4Info& P4Info() const { return p4info_; }

 private:
  // The fix node Id and unit for the node tested by this class. This class
  // only tests one node with ID 1 and device id 0.
  static constexpr uint64 kNodeId = 1;
  static constexpr int kDevice = 0;

  // ChassisConfig chassis_config_;
  ::p4::v1::ForwardingPipelineConfig forwarding_pipeline_config_;
  // ::p4::v1::WriteRequest write_request_;
  std::unique_ptr<P4RuntimeSession> sut_p4rt_session_;
  ::p4::config::v1::P4Info p4info_;
};

}  // namespace p4runtime
}  // namespace stratum

#endif  // STRATUM_LIB_P4RUNTIME_P4RUNTIME_FIXTURE_H_
