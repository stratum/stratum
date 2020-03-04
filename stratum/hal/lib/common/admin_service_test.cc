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

#include "stratum/hal/lib/common/admin_service.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "gflags/gflags.h"
#include "gmock/gmock.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "stratum/glue/net_util/ports.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/admin_utils_mock.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/hal/lib/common/switch_mock.h"
#include "stratum/lib/security/auth_policy_checker_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/timer_daemon.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

class AdminServiceTest : public ::testing::TestWithParam<OperationMode> {
 protected:
  void SetUp() override {
    mode_ = GetParam();
    switch_mock_ = absl::make_unique<SwitchMock>();
    auth_policy_checker_mock_ = absl::make_unique<AuthPolicyCheckerMock>();
    error_buffer_ = absl::make_unique<ErrorBuffer>();
    admin_service_ = absl::make_unique<AdminService>(
        mode_, switch_mock_.get(), auth_policy_checker_mock_.get(),
        error_buffer_.get(),
        [this](int sigval) {
          this->hal_reset_triggered_ = true;
        });

    admin_service_->helper_ =
        absl::make_unique<AdminServiceUtilsInterfaceMock>();

    admin_utils_ = dynamic_cast<AdminServiceUtilsInterfaceMock*>
                                (admin_service_->helper_.get());

    // Create FileSystemHelperMock object
    fs_helper_ = std::make_shared<FileSystemHelperMock>();

    // Get AdminServiceUtilsInterface from admin_service_
    auto helper_ = admin_service_->helper_.get();
    // Set FileSystemHelperMock object as default return value
    // for admin_service_->helper_->GetFileSystemHelper() call
    ON_CALL(*(dynamic_cast<AdminServiceUtilsInterfaceMock*>(helper_)),
            GetFileSystemHelper())
        .WillByDefault(::testing::Return(fs_helper_));

    // Create a mock service to test the functionality.
    std::string url =
        "localhost:" + std::to_string(stratum::PickUnusedPortOrDie());
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(url, ::grpc::InsecureServerCredentials());
    builder.RegisterService(admin_service_.get());
    server_ = builder.BuildAndStart();
    ASSERT_NE(server_, nullptr);
    stub_ = ::gnoi::system::System::NewStub(
        ::grpc::CreateChannel(url, ::grpc::InsecureChannelCredentials()));
    ASSERT_NE(stub_, nullptr);
    hal_reset_triggered_ = false;
  }

  void TearDown() override { server_->Shutdown(); }

  OperationMode mode_;

  // Not-owning pointer, owned by the AdminService
  AdminServiceUtilsInterfaceMock* admin_utils_;
  std::unique_ptr<AdminService> admin_service_;
  std::unique_ptr<SwitchMock> switch_mock_;
  std::unique_ptr<AuthPolicyCheckerMock> auth_policy_checker_mock_;
  std::unique_ptr<ErrorBuffer> error_buffer_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<::gnoi::system::System::Stub> stub_;
  std::shared_ptr<FileSystemHelperMock> fs_helper_;
  bool hal_reset_triggered_;
};

TEST_P(AdminServiceTest, ColdbootSetupSuccess) {
  ASSERT_OK(admin_service_->Setup(false));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());

  // cleanup
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, WarmbootSetupSuccess) {
  ASSERT_OK(admin_service_->Setup(true));
  const auto& errors = error_buffer_->GetErrors();
  EXPECT_TRUE(errors.empty());

  // cleanup
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, TimeSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::system::TimeRequest req;
  ::gnoi::system::TimeResponse resp;
  ASSERT_OK(admin_service_->Setup(false));

  // Invoke the RPC and validate the results.
  EXPECT_CALL(*admin_utils_, GetTime())
    .WillOnce(::testing::Return(0));
  ::grpc::Status status = stub_->Time(&context, req, &resp);
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, RebootColdSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::system::RebootRequest req;
  ::gnoi::system::RebootResponse resp;
  ASSERT_OK(admin_service_->Setup(false));

  req.set_delay(1000000);  // 1ms
  req.set_method(gnoi::system::RebootMethod::COLD);

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Reboot(&context, req, &resp);
  EXPECT_TRUE(status.ok());

  absl::SleepFor(absl::Milliseconds(2));
  TimerDaemon::Execute();
  EXPECT_TRUE(hal_reset_triggered_);

  // we expected that reboot method invoked when admin service teardown
  EXPECT_CALL(*admin_utils_, Reboot())
    .WillOnce(::testing::Return(::util::OkStatus()));
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, CancelReboot) {
  ::grpc::ClientContext context1;
  ::gnoi::system::RebootRequest req;
  ::gnoi::system::RebootResponse resp;
  ASSERT_OK(admin_service_->Setup(false));

  req.set_delay(5000000);  // 5ms
  req.set_method(gnoi::system::RebootMethod::COLD);

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Reboot(&context1, req, &resp);
  EXPECT_TRUE(status.ok());

  // Cancel the reboot
  ::grpc::ClientContext context2;
  ::gnoi::system::CancelRebootRequest cancel_req;
  ::gnoi::system::CancelRebootResponse cancel_resp;
  status = stub_->CancelReboot(&context2, cancel_req, &cancel_resp);
  EXPECT_TRUE(status.ok());

  absl::SleepFor(absl::Milliseconds(6));
  TimerDaemon::Execute();
  EXPECT_FALSE(hal_reset_triggered_);

  // we expected that reboot method not invoked when admin service teardown
  EXPECT_CALL(*admin_utils_, Reboot()).Times(0);
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, RebootUnknownFail) {
  ::grpc::ClientContext context;
  ::gnoi::system::RebootRequest req;
  ::gnoi::system::RebootResponse resp;
  ASSERT_OK(admin_service_->Setup(false));

  // Set unimplemented mode
  req.set_method(gnoi::system::RebootMethod::UNKNOWN);

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->Reboot(&context, req, &resp);
  EXPECT_TRUE(status.error_code() == ::grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_P(AdminServiceTest, RebootStatusInactiveSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::system::RebootStatusRequest req;
  ::gnoi::system::RebootStatusResponse resp;
  ASSERT_OK(admin_service_->Setup(false));

  stub_->RebootStatus(&context, req, &resp);
  EXPECT_TRUE(!resp.active());
  EXPECT_EQ(resp.wait(), 0);
  EXPECT_EQ(resp.when(), 0);
  EXPECT_TRUE(resp.reason().empty());

  // cleanup
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, CancelRebootSuccess) {
  ::grpc::ClientContext context;
  ::gnoi::system::CancelRebootRequest req;
  ::gnoi::system::CancelRebootResponse resp;
  ASSERT_OK(admin_service_->Setup(false));

  // Invoke the RPC and validate the results.
  ::grpc::Status status = stub_->CancelReboot(&context, req, &resp);
  EXPECT_TRUE(status.ok());

  // cleanup
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, SetPackageFirstMeesageNotPackage) {
  ::grpc::ClientContext context;
  ::gnoi::system::SetPackageResponse resp;
  ::gnoi::system::SetPackageRequest req;
  ASSERT_OK(admin_service_->Setup(false));

  std::unique_ptr<::grpc::ClientWriter<::gnoi::system::SetPackageRequest>>
      writer = stub_->SetPackage(&context, &resp);

  req.set_contents("some fake contents");
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }
  writer->WritesDone();
  ::grpc::Status status = writer->Finish();
  EXPECT_TRUE(status.error_code() == ::grpc::StatusCode::INVALID_ARGUMENT);
  // cleanup
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, SetPackageRemoteOptionSFTP) {
  ::grpc::ClientContext context;
  ::gnoi::system::SetPackageResponse resp;
  ::gnoi::system::SetPackageRequest req;
  ASSERT_OK(admin_service_->Setup(false));

  auto package = new ::gnoi::system::Package();
  auto remoteDownload = new ::gnoi::common::RemoteDownload();
  auto hash_type = new ::gnoi::types::HashType();

  // Configure expected calls
  std::unique_ptr<::grpc::ClientWriter<::gnoi::system::SetPackageRequest> >
      writer = stub_->SetPackage(&context, &resp);

  remoteDownload->set_protocol(::gnoi::common::RemoteDownload_Protocol_SFTP);
  package->set_filename(std::string("/home/user/filename"));
  package->set_allocated_remote_download(remoteDownload);
  req.set_allocated_package(package);
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }
  hash_type->set_method(::gnoi::types::HashType_HashMethod_SHA256);
  hash_type->set_hash("Incorrect Hash");
  req.set_allocated_hash(hash_type);
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }
  writer->WritesDone();
  ::grpc::Status status = writer->Finish();
  EXPECT_TRUE(status.error_code() == ::grpc::StatusCode::UNIMPLEMENTED);
  // cleanup
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, SetPackageLastNotHash) {
  ::grpc::ClientContext context;
  ::gnoi::system::SetPackageResponse resp;
  ::gnoi::system::SetPackageRequest req;
  ASSERT_OK(admin_service_->Setup(false));

  auto package = new ::gnoi::system::Package();

  // Configure expected calls
  EXPECT_CALL(*(fs_helper_.get()), PathExists("/home/user"))
      .WillOnce(::testing::Return(true));

  EXPECT_CALL(*(fs_helper_.get()), CreateTempDir())
      .WillOnce(::testing::Return("tmpdir"));

  EXPECT_CALL(*(fs_helper_.get()), TempFileName("tmpdir"))
      .WillOnce(::testing::Return("tmpfile"));

  EXPECT_CALL(*(fs_helper_.get()),
              StringToFile("Some data", "tmpfile", true))
      .Times(1);

  EXPECT_CALL(*(fs_helper_.get()), RemoveDir("tmpdir"))
      .Times(1);

  EXPECT_CALL(*(fs_helper_.get()), RemoveFile("tmpfile"))
      .Times(1);

  std::unique_ptr<::grpc::ClientWriter<::gnoi::system::SetPackageRequest> >
      writer = stub_->SetPackage(&context, &resp);

  package->set_filename("/home/user/filename");
  req.set_allocated_package(package);
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }

  req.set_contents(std::string("Some data"));
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }
  writer->WritesDone();
  ::grpc::Status status = writer->Finish();
  EXPECT_TRUE(status.error_code() == ::grpc::StatusCode::INVALID_ARGUMENT);
  // cleanup
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, SetPackageUNCPECIFIEDHash) {
  ::grpc::ClientContext context;
  ::gnoi::system::SetPackageResponse resp;
  ::gnoi::system::SetPackageRequest req;
  auto package = new ::gnoi::system::Package();
  auto hash_type = new ::gnoi::types::HashType();
  ASSERT_OK(admin_service_->Setup(false));

  // Configure expected calls
  EXPECT_CALL(*(fs_helper_.get()), PathExists("/home/user"))
      .WillOnce(::testing::Return(true));

  EXPECT_CALL(*(fs_helper_.get()), CreateTempDir())
      .WillOnce(::testing::Return("tmpdir"));

  EXPECT_CALL(*(fs_helper_.get()), TempFileName("tmpdir"))
      .WillOnce(::testing::Return("tmpfile"));

  EXPECT_CALL(*(fs_helper_.get()),
              StringToFile("Some data", "tmpfile", true))
      .Times(1);

  EXPECT_CALL(*(fs_helper_.get()), RemoveDir("tmpdir"))
      .Times(1);

  EXPECT_CALL(*(fs_helper_.get()), RemoveFile("tmpfile"))
      .Times(1);

  std::unique_ptr<::grpc::ClientWriter<::gnoi::system::SetPackageRequest> >
      writer = stub_->SetPackage(&context, &resp);

  package->set_filename("/home/user/filename");
  req.set_allocated_package(package);
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }

  req.set_contents(std::string("Some data"));
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }

  hash_type->set_method(::gnoi::types::HashType_HashMethod_UNSPECIFIED);
  req.set_allocated_hash(hash_type);
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }
  writer->WritesDone();
  ::grpc::Status status = writer->Finish();
  EXPECT_TRUE(status.error_code() == ::grpc::StatusCode::INVALID_ARGUMENT);

  //  cleanup
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, SetPackageIncorrectHash) {
  ::grpc::ClientContext context;
  ::gnoi::system::SetPackageResponse resp;
  ::gnoi::system::SetPackageRequest req;
  auto package = new ::gnoi::system::Package();
  auto hash_type = new ::gnoi::types::HashType();
  ASSERT_OK(admin_service_->Setup(false));

  // Configure expected calls
  EXPECT_CALL(*(fs_helper_.get()), PathExists("/home/user"))
      .WillOnce(::testing::Return(true));

  EXPECT_CALL(*(fs_helper_.get()), CreateTempDir())
      .WillOnce(::testing::Return("tmpdir"));

  EXPECT_CALL(*(fs_helper_.get()), TempFileName("tmpdir"))
      .WillOnce(::testing::Return("tmpfile"));

  EXPECT_CALL(*(fs_helper_.get()),
              StringToFile("Some data", "tmpfile", true))
      .Times(1);

  EXPECT_CALL(*(fs_helper_.get()), RemoveDir("tmpdir"))
      .Times(1);

  EXPECT_CALL(*(fs_helper_.get()), RemoveFile("tmpfile"))
      .Times(1);

  EXPECT_CALL(*(fs_helper_.get()),
              CheckHashSumFile("tmpfile",
                               "Incorrect Hash",
                               ::gnoi::types::HashType_HashMethod_SHA256))
      .WillOnce(::testing::Return(false));

  std::unique_ptr<::grpc::ClientWriter<::gnoi::system::SetPackageRequest> >
      writer = stub_->SetPackage(&context, &resp);

  package->set_filename("/home/user/filename");
  req.set_allocated_package(package);
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }

  req.set_contents(std::string("Some data"));
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }

  hash_type->set_method(::gnoi::types::HashType_HashMethod_SHA256);
  hash_type->set_hash("Incorrect Hash");
  req.set_allocated_hash(hash_type);
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }
  writer->WritesDone();
  ::grpc::Status status = writer->Finish();
  EXPECT_TRUE(status.error_code() == ::grpc::StatusCode::DATA_LOSS);

  //  cleanup
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, SetPackageSHA256Success) {
  ::grpc::ClientContext context;
  ::gnoi::system::SetPackageResponse resp;
  ::gnoi::system::SetPackageRequest req;
  auto package = new ::gnoi::system::Package();
  auto hash_type = new ::gnoi::types::HashType();
  ASSERT_OK(admin_service_->Setup(false));

  // Configure expected calls
  EXPECT_CALL(*(fs_helper_.get()), PathExists("/home/user"))
      .WillOnce(::testing::Return(true));

  EXPECT_CALL(*(fs_helper_.get()), CreateTempDir())
      .WillOnce(::testing::Return("tmpdir"));

  EXPECT_CALL(*(fs_helper_.get()), TempFileName("tmpdir"))
      .WillOnce(::testing::Return("tmpfile"));

  EXPECT_CALL(*(fs_helper_.get()),
              StringToFile("Some data", "tmpfile", true))
      .Times(1);

  EXPECT_CALL(*(fs_helper_.get()), RemoveDir("tmpdir"))
      .Times(1);

  EXPECT_CALL(*(fs_helper_.get()), RemoveFile("tmpfile"))
      .Times(1);

  EXPECT_CALL(*(fs_helper_.get()),
              CheckHashSumFile("tmpfile",
                               "correct hash",
                               ::gnoi::types::HashType_HashMethod_SHA256))
      .WillOnce(::testing::Return(true));

  std::unique_ptr<::grpc::ClientWriter<::gnoi::system::SetPackageRequest> >
      writer = stub_->SetPackage(&context, &resp);

  package->set_filename("/home/user/somefile");
  req.set_allocated_package(package);
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }

  req.set_contents(std::string("Some data"));
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }

  std::istringstream isstream("Some data");
  hash_type->set_method(::gnoi::types::HashType_HashMethod_SHA256);
  hash_type->set_hash("correct hash");
  req.set_allocated_hash(hash_type);
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }
  writer->WritesDone();
  ::grpc::Status status = writer->Finish();
  EXPECT_TRUE(status.ok());

  //  cleanup
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, SetPackageEmptyFilename) {
  ::grpc::ClientContext context;
  ::gnoi::system::SetPackageResponse resp;
  ::gnoi::system::SetPackageRequest req;
  auto package = new ::gnoi::system::Package();
  ASSERT_OK(admin_service_->Setup(false));

  std::unique_ptr<::grpc::ClientWriter<::gnoi::system::SetPackageRequest> >
  writer = stub_->SetPackage(&context, &resp);

  package->set_filename("");
  req.set_allocated_package(package);
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }

  writer->WritesDone();
  ::grpc::Status status = writer->Finish();
  EXPECT_TRUE(status.error_code() == ::grpc::StatusCode::INVALID_ARGUMENT);

  // cleanup
  ASSERT_OK(admin_service_->Teardown());
}

TEST_P(AdminServiceTest, SetPackageUnsupportedOptions) {
  ::grpc::ClientContext context;
  ::gnoi::system::SetPackageResponse resp;
  ::gnoi::system::SetPackageRequest req;
  auto package = new ::gnoi::system::Package();
  ASSERT_OK(admin_service_->Setup(false));

  // Configure unexpected calls
  EXPECT_CALL(*(fs_helper_.get()), CreateTempDir()).Times(0);

  EXPECT_CALL(*(fs_helper_.get()), TempFileName(::testing::_)).Times(0);

  EXPECT_CALL(*(fs_helper_.get()),
              StringToFile(::testing::_, ::testing::_, ::testing::_))
      .Times(0);

  EXPECT_CALL(*(fs_helper_.get()), RemoveDir(::testing::_)).Times(0);

  EXPECT_CALL(*(fs_helper_.get()), RemoveFile(::testing::_)).Times(0);

  EXPECT_CALL(*(fs_helper_.get()),
              CheckHashSumFile(::testing::_, ::testing::_, ::testing::_))
      .Times(0);

  std::unique_ptr<::grpc::ClientWriter<::gnoi::system::SetPackageRequest> >
  writer = stub_->SetPackage(&context, &resp);

  package->set_filename("tmpfile");
  package->set_activate(true);
  package->set_version("10.2.1");
  req.set_allocated_package(package);
  if (!writer->Write(req)) {
    LOG(ERROR) << "Broken stream";
  }

  writer->WritesDone();
  ::grpc::Status status = writer->Finish();
  EXPECT_TRUE(status.error_code() == ::grpc::StatusCode::UNIMPLEMENTED);

  //  cleanup
  ASSERT_OK(admin_service_->Teardown());
}

INSTANTIATE_TEST_SUITE_P(AdminServiceTestWithMode, AdminServiceTest,
                        ::testing::Values(OPERATION_MODE_STANDALONE,
                                          OPERATION_MODE_COUPLED,
                                          OPERATION_MODE_SIM));

}  // namespace hal
}  // namespace stratum
