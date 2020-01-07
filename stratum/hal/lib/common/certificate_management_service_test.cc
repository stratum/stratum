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

#include "stratum/hal/lib/common/certificate_management_service.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
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

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

using RotateCertificateClientReaderWriter =
    ::grpc::ClientReaderWriter<::gnoi::certificate::RotateCertificateRequest,
                               ::gnoi::certificate::RotateCertificateResponse>;
using InstallCertificateClientReaderWriter =
    ::grpc::ClientReaderWriter<::gnoi::certificate::InstallCertificateRequest,
                               ::gnoi::certificate::InstallCertificateResponse>;

class CertificateManagementServiceTest
    : public ::testing::TestWithParam<OperationMode> {
 protected:
  void SetUp() override {
    mode_ = GetParam();
    switch_mock_ = absl::make_unique<SwitchMock>();
    auth_policy_checker_mock_ = absl::make_unique<AuthPolicyCheckerMock>();
    error_buffer_ = absl::make_unique<ErrorBuffer>();
    certificate_management_service_ =
        absl::make_unique<CertificateManagementService>(
            mode_, switch_mock_.get(), auth_policy_checker_mock_.get(),
            error_buffer_.get());
    // Create a mock service to test the functionality.
    std::string url =
        "localhost:" + std::to_string(stratum::PickUnusedPortOrDie());
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(url, ::grpc::InsecureServerCredentials());
    builder.RegisterService(certificate_management_service_.get());
    server_ = builder.BuildAndStart();
    ASSERT_NE(server_, nullptr);
    stub_ = ::gnoi::certificate::CertificateManagement::NewStub(
        ::grpc::CreateChannel(url, ::grpc::InsecureChannelCredentials()));
    ASSERT_NE(stub_, nullptr);
  }

  void TearDown() override { server_->Shutdown(); }

  OperationMode mode_;
  std::unique_ptr<CertificateManagementService> certificate_management_service_;
  std::unique_ptr<SwitchMock> switch_mock_;
  std::unique_ptr<AuthPolicyCheckerMock> auth_policy_checker_mock_;
  std::unique_ptr<ErrorBuffer> error_buffer_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<::gnoi::certificate::CertificateManagement::Stub> stub_;
};

TEST_P(CertificateManagementServiceTest, ColdbootSetupSuccess) {
  ASSERT_OK(certificate_management_service_->Setup(false));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());

  // cleanup
  ASSERT_OK(certificate_management_service_->Teardown());
}

TEST_P(CertificateManagementServiceTest, WarmbootSetupSuccess) {
  ASSERT_OK(certificate_management_service_->Setup(true));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());

  // cleanup
  ASSERT_OK(certificate_management_service_->Teardown());
}

TEST_P(CertificateManagementServiceTest, RotateSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::certificate::RotateCertificateRequest req;
  ::gnoi::certificate::RotateCertificateResponse resp;

  // Invoke the RPC and validate the results.
  std::unique_ptr<RotateCertificateClientReaderWriter> stream =
      stub_->Rotate(&context);

  ASSERT_TRUE(stream->Write(req));
  ASSERT_TRUE(stream->WritesDone());
  ASSERT_FALSE(stream->Read(&resp));
  ::grpc::Status status = stream->Finish();
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(certificate_management_service_->Teardown());
}

TEST_P(CertificateManagementServiceTest, InstallSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::certificate::InstallCertificateRequest req;
  ::gnoi::certificate::InstallCertificateResponse resp;

  // Invoke the RPC and validate the results.
  std::unique_ptr<InstallCertificateClientReaderWriter> stream =
      stub_->Install(&context);

  ASSERT_TRUE(stream->Write(req));
  ASSERT_TRUE(stream->WritesDone());
  ASSERT_FALSE(stream->Read(&resp));
  ::grpc::Status status = stream->Finish();
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(certificate_management_service_->Teardown());
}

TEST_P(CertificateManagementServiceTest, GetCertificatesSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::certificate::GetCertificatesRequest req;
  ::gnoi::certificate::GetCertificatesResponse resp;

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->GetCertificates(&context, req, &resp);
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(certificate_management_service_->Teardown());
}

TEST_P(CertificateManagementServiceTest, RevokeCertificatesSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::certificate::RevokeCertificatesRequest req;
  ::gnoi::certificate::RevokeCertificatesResponse resp;

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->RevokeCertificates(&context, req, &resp);
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(certificate_management_service_->Teardown());
}

TEST_P(CertificateManagementServiceTest, CanGenerateCSRSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::certificate::CanGenerateCSRRequest req;
  ::gnoi::certificate::CanGenerateCSRResponse resp;

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->CanGenerateCSR(&context, req, &resp);
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(certificate_management_service_->Teardown());
}

INSTANTIATE_TEST_SUITE_P(CertificateManagementServiceTestWithMode,
                        CertificateManagementServiceTest,
                        ::testing::Values(OPERATION_MODE_STANDALONE,
                                          OPERATION_MODE_COUPLED,
                                          OPERATION_MODE_SIM));

}  // namespace hal
}  // namespace stratum
