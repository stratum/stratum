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

#include "stratum/hal/lib/common/file_service.h"

#include <grpc++/grpc++.h>
#include <memory>
#include <string>

#include "gflags/gflags.h"
#include "stratum/glue/net_util/ports.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/hal/lib/common/switch_mock.h"
#include "stratum/lib/security/auth_policy_checker_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"

using ::testing::IsEmpty;

namespace stratum {
namespace hal {

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

class FileServiceTest : public ::testing::TestWithParam<OperationMode> {
 protected:
  void SetUp() override {
    mode_ = GetParam();
    switch_mock_ = absl::make_unique<SwitchMock>();
    auth_policy_checker_mock_ = absl::make_unique<AuthPolicyCheckerMock>();
    error_buffer_ = absl::make_unique<ErrorBuffer>();
    file_service_ = absl::make_unique<FileService>(
        mode_, switch_mock_.get(), auth_policy_checker_mock_.get(),
        error_buffer_.get());
    // Create a mock service to test the functionality.
    std::string url =
        "localhost:" + std::to_string(stratum::PickUnusedPortOrDie());
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(url, ::grpc::InsecureServerCredentials());
    builder.RegisterService(file_service_.get());
    server_ = builder.BuildAndStart();
    ASSERT_NE(server_, nullptr);
    stub_ = ::gnoi::file::File::NewStub(
        ::grpc::CreateChannel(url, ::grpc::InsecureChannelCredentials()));
    ASSERT_NE(stub_, nullptr);
  }

  void TearDown() override { server_->Shutdown(); }

  OperationMode mode_;
  std::unique_ptr<FileService> file_service_;
  std::unique_ptr<SwitchMock> switch_mock_;
  std::unique_ptr<AuthPolicyCheckerMock> auth_policy_checker_mock_;
  std::unique_ptr<ErrorBuffer> error_buffer_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<::gnoi::file::File::Stub> stub_;
};

TEST_P(FileServiceTest, ColdbootSetupSuccess) {
  ASSERT_OK(file_service_->Setup(false));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_THAT(errors, IsEmpty());

  // cleanup
  ASSERT_OK(file_service_->Teardown());
}

TEST_P(FileServiceTest, WarmbootSetupSuccess) {
  ASSERT_OK(file_service_->Setup(true));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_THAT(errors, IsEmpty());

  // cleanup
  ASSERT_OK(file_service_->Teardown());
}

TEST_P(FileServiceTest, GetSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::file::GetRequest req;
  ::gnoi::file::GetResponse resp;

  // Invoke the RPC and validate the results.
  std::unique_ptr<::grpc::ClientReader<::gnoi::file::GetResponse>> reader =
      stub_->Get(&context, req);
  ASSERT_FALSE(reader->Read(&resp));
  ::grpc::Status status = reader->Finish();
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(file_service_->Teardown());
}

TEST_P(FileServiceTest, PutSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::file::PutRequest req;
  ::gnoi::file::PutResponse resp;

  // Invoke the RPC and validate the results.
  std::unique_ptr<::grpc::ClientWriter<::gnoi::file::PutRequest>> writer =
      stub_->Put(&context, &resp);

  ASSERT_TRUE(writer->Write(req));
  ASSERT_TRUE(writer->WritesDone());
  ::grpc::Status status = writer->Finish();
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(file_service_->Teardown());
}

TEST_P(FileServiceTest, StatSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::file::StatRequest req;
  ::gnoi::file::StatResponse resp;

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Stat(&context, req, &resp);
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(file_service_->Teardown());
}

TEST_P(FileServiceTest, RemoveSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::file::RemoveRequest req;
  ::gnoi::file::RemoveResponse resp;

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Remove(&context, req, &resp);
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(file_service_->Teardown());
}

INSTANTIATE_TEST_SUITE_P(FileServiceTestWithMode, FileServiceTest,
                        ::testing::Values(OPERATION_MODE_STANDALONE,
                                          OPERATION_MODE_COUPLED,
                                          OPERATION_MODE_SIM));

}  // namespace hal
}  // namespace stratum
