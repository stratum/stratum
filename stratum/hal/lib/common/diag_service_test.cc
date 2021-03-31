// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/common/diag_service.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "gflags/gflags.h"
#include "gmock/gmock.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "stratum/glue/net_util/ports.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/hal/lib/common/switch_mock.h"
#include "stratum/lib/security/auth_policy_checker_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {

using ::testing::IsEmpty;

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

class DiagServiceTest : public ::testing::TestWithParam<OperationMode> {
 protected:
  void SetUp() override {
    mode_ = GetParam();
    switch_mock_ = absl::make_unique<SwitchMock>();
    auth_policy_checker_mock_ = absl::make_unique<AuthPolicyCheckerMock>();
    error_buffer_ = absl::make_unique<ErrorBuffer>();
    diag_service_ = absl::make_unique<DiagService>(
        mode_, switch_mock_.get(), auth_policy_checker_mock_.get(),
        error_buffer_.get());
    // Create a mock service to test the functionality.
    std::string url =
        "localhost:" + std::to_string(stratum::PickUnusedPortOrDie());
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(url, ::grpc::InsecureServerCredentials());
    builder.RegisterService(diag_service_.get());
    server_ = builder.BuildAndStart();
    ASSERT_NE(server_, nullptr);
    stub_ = ::gnoi::diag::Diag::NewStub(
        ::grpc::CreateChannel(url, ::grpc::InsecureChannelCredentials()));
    ASSERT_NE(stub_, nullptr);
  }

  void TearDown() override { server_->Shutdown(); }

  OperationMode mode_;
  std::unique_ptr<DiagService> diag_service_;
  std::unique_ptr<SwitchMock> switch_mock_;
  std::unique_ptr<AuthPolicyCheckerMock> auth_policy_checker_mock_;
  std::unique_ptr<ErrorBuffer> error_buffer_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<::gnoi::diag::Diag::Stub> stub_;
};

TEST_P(DiagServiceTest, ColdbootSetupSuccess) {
  ASSERT_OK(diag_service_->Setup(false));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_THAT(errors, IsEmpty());

  // cleanup
  ASSERT_OK(diag_service_->Teardown());
}

TEST_P(DiagServiceTest, WarmbootSetupSuccess) {
  ASSERT_OK(diag_service_->Setup(true));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_THAT(errors, IsEmpty());

  // cleanup
  ASSERT_OK(diag_service_->Teardown());
}

TEST_P(DiagServiceTest, StartBERTSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::diag::StartBERTRequest req;
  ::gnoi::diag::StartBERTResponse resp;

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->StartBERT(&context, req, &resp);
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(diag_service_->Teardown());
}

TEST_P(DiagServiceTest, StopBERTSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::diag::StopBERTRequest req;
  ::gnoi::diag::StopBERTResponse resp;

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->StopBERT(&context, req, &resp);
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(diag_service_->Teardown());
}

TEST_P(DiagServiceTest, GetBERTResultSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::diag::GetBERTResultRequest req;
  ::gnoi::diag::GetBERTResultResponse resp;

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->GetBERTResult(&context, req, &resp);
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(diag_service_->Teardown());
}

INSTANTIATE_TEST_SUITE_P(DiagServiceTestWithMode, DiagServiceTest,
                         ::testing::Values(OPERATION_MODE_STANDALONE,
                                           OPERATION_MODE_COUPLED,
                                           OPERATION_MODE_SIM));

}  // namespace hal
}  // namespace stratum
