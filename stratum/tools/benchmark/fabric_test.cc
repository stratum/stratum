// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "gflags/gflags.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/tools/benchmark/p4runtime_session.h"

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
namespace tools {
namespace benchmark {
namespace {

class FabricTest : public ::testing::Test {
 protected:
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
  std::unique_ptr<P4RuntimeSession> sut_p4rt_session_;
  ::p4::config::v1::P4Info p4info_;
};

TEST_F(FabricTest, InsertTableEntry) {
  const std::string entry_text = R"PROTO(
      table_id: 39601850
      match {
        field_id: 1
        ternary {
          value: "\001\004"
          mask: "\001\377"
        }
      }
      action {
        action {
          action_id: 21161133
        }
      }
      priority: 10
    )PROTO";
  ::p4::v1::TableEntry entry;
  ASSERT_OK(ParseProtoFromString(entry_text, &entry));
  ASSERT_OK(InstallTableEntry(SutP4RuntimeSession(), entry));
}

}  // namespace
}  // namespace benchmark
}  // namespace tools
}  // namespace stratum
