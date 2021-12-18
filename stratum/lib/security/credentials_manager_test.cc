// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
#include "stratum/lib/security/credentials_manager.h"

#include <fstream>
#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "absl/strings/substitute.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "stratum/glue/net_util/ports.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/security/certificate.h"
#include "stratum/lib/security/test.grpc.pb.h"
#include "stratum/lib/utils.h"

DECLARE_string(ca_cert);
DECLARE_string(server_key);
DECLARE_string(server_cert);
DECLARE_string(test_tmpdir);

namespace stratum {
namespace {

class TestServiceImpl final : public ::testing::TestService::Service {
 public:
  ::grpc::Status Test(::grpc::ServerContext* /*context*/,
                      const ::testing::Empty* /*request*/,
                      ::testing::Empty* /*response*/) override {
    return ::grpc::Status::OK;
  }
};

constexpr char kCaCertFile[] = "ca.crt";
constexpr char kServerCertFile[] = "stratum.crt";
constexpr char kServerKeyFile[] = "stratum.key";
constexpr char cert_common_name[] = "stratum.local";

util::Status GenerateCerts(std::string* ca_crt, std::string* server_crt,
                           std::string* server_key) {
  absl::Time valid_until = absl::Now() + absl::Hours(24);
  Certificate ca("Stratum CA", 1);
  EXPECT_OK(ca.GenerateKeyPair(1024));
  EXPECT_OK(ca.SignCertificate(ca, valid_until));

  Certificate stratum(cert_common_name, 1);
  EXPECT_OK(stratum.GenerateKeyPair(1024));
  EXPECT_OK(stratum.SignCertificate(ca, valid_until));

  ASSIGN_OR_RETURN(*ca_crt, ca.GetCertificate());
  ASSIGN_OR_RETURN(*server_crt, stratum.GetCertificate());
  ASSIGN_OR_RETURN(*server_key, stratum.GetPrivateKey());
  return util::OkStatus();
}

void SetCerts(const std::string& ca_crt, const std::string& server_crt,
              const std::string& server_key) {
  ASSERT_OK(WriteStringToFile(ca_crt, FLAGS_ca_cert));
  ASSERT_OK(WriteStringToFile(server_crt, FLAGS_server_cert));
  ASSERT_OK(WriteStringToFile(server_key, FLAGS_server_key));
}

class CredentialsManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FLAGS_ca_cert = absl::StrFormat("%s/%s", FLAGS_test_tmpdir, kCaCertFile);
    FLAGS_server_cert =
        absl::StrFormat("%s/%s", FLAGS_test_tmpdir, kServerCertFile);
    FLAGS_server_key =
        absl::StrFormat("%s/%s", FLAGS_test_tmpdir, kServerKeyFile);

    std::string server_crt, server_key;
    EXPECT_OK(GenerateCerts(&ca_crt_, &server_crt, &server_key));
    SetCerts(ca_crt_, server_crt, server_key);
    credentials_manager_ =
        CredentialsManager::CreateInstance().ConsumeValueOrDie();
    std::shared_ptr<::grpc::ServerCredentials> server_credentials =
        credentials_manager_->GenerateExternalFacingServerCredentials();

    url_ = "localhost:" + std::to_string(stratum::PickUnusedPortOrDie());
    test_service_ = absl::make_unique<TestServiceImpl>();
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(url_, server_credentials);
    builder.RegisterService(test_service_.get());
    server_ = builder.BuildAndStart();
    ASSERT_NE(server_, nullptr);
  }

  void Connect(const std::string& ca_crt, bool expectSuccess = true) {
    auto cert_provider =
        std::make_shared<::grpc::experimental::StaticDataCertificateProvider>(
            ca_crt);
    auto tls_opts =
        std::make_shared<::grpc::experimental::TlsChannelCredentialsOptions>(
            cert_provider);
    tls_opts->watch_root_certs();
    auto channel_creds = ::grpc::experimental::TlsCredentials(*tls_opts);

    ::grpc::ChannelArguments args;
    args.SetSslTargetNameOverride(cert_common_name);
    auto channel = ::grpc::CreateCustomChannel(url_, channel_creds, args);
    auto stub = ::testing::TestService::NewStub(channel);

    ::grpc::ClientContext context;
    ::testing::Empty request;
    ::testing::Empty response;
    ::grpc::Status status = stub->Test(&context, request, &response);
    EXPECT_TRUE(status.ok() == expectSuccess);
  }

  void TearDown() override { server_->Shutdown(); }

  const std::string& GetOriginalCaCert() { return ca_crt_; }

  std::string url_;
  std::string ca_crt_;
  std::unique_ptr<CredentialsManager> credentials_manager_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<TestServiceImpl> test_service_;
};

TEST_F(CredentialsManagerTest, ConnectSuccess) { Connect(GetOriginalCaCert()); }

TEST_F(CredentialsManagerTest, ConnectFailWrongCert) {
  std::string ca_crt, server_crt, server_key;
  EXPECT_OK(GenerateCerts(&ca_crt, &server_crt, &server_key));
  Connect(ca_crt, false);
}

TEST_F(CredentialsManagerTest, ConnectAfterCertChange) {
  std::string ca_crt, server_crt, server_key;
  EXPECT_OK(GenerateCerts(&ca_crt, &server_crt, &server_key));
  SetCerts(ca_crt, server_crt, server_key);
  absl::SleepFor(absl::Seconds(2));  // Wait for file watcher to update certs...
  Connect(ca_crt);
  Connect(GetOriginalCaCert(), false);
}

TEST_F(CredentialsManagerTest, LoadNewCredentials) {
  std::string ca_crt, server_crt, server_key;
  EXPECT_OK(GenerateCerts(&ca_crt, &server_crt, &server_key));
  EXPECT_OK(
      credentials_manager_->LoadNewCredential(ca_crt, server_crt, server_key));

  // Read and verify the active key material files
  std::string ca_cert_actual;
  std::string cert_actual;
  std::string key_actual;
  ASSERT_OK(ReadFileToString(FLAGS_ca_cert, &ca_cert_actual));
  ASSERT_OK(ReadFileToString(FLAGS_server_cert, &cert_actual));
  ASSERT_OK(ReadFileToString(FLAGS_server_key, &key_actual));
  EXPECT_EQ(ca_cert_actual, ca_crt);
  EXPECT_EQ(cert_actual, server_crt);
  EXPECT_EQ(key_actual, server_key);

  // Make sure connections work
  absl::SleepFor(absl::Seconds(2));  // Wait for file watcher to update certs...
  Connect(ca_crt);
  Connect(GetOriginalCaCert(), false);
}

}  // namespace
}  // namespace stratum
