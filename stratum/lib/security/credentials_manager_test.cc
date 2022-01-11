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

DECLARE_string(server_ca_cert_file);
DECLARE_string(server_key_file);
DECLARE_string(server_cert_file);
DECLARE_string(client_ca_cert_file);
DECLARE_string(client_key_file);
DECLARE_string(client_cert_file);
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

constexpr char kServerCaCertFile[] = "ca_server.crt";
constexpr char kClientCaCertFile[] = "ca_client.crt";
constexpr char kServerCertFile[] = "stratum_server.crt";
constexpr char kServerKeyFile[] = "stratum_server.key";
constexpr char kClientCertFile[] = "stratum_client.crt";
constexpr char kClientKeyFile[] = "stratum_client.key";
constexpr char kCertCommonName[] = "stratum.local";

util::Status GenerateCerts(std::string* ca_crt, std::string* server_crt,
                           std::string* server_key, std::string* client_crt,
                           std::string* client_key) {
  absl::Time valid_after = absl::Now();
  absl::Time valid_until = valid_after + absl::Hours(24);
  Certificate ca("Stratum CA", 1);
  EXPECT_OK(ca.GenerateKeyPair(1024));
  EXPECT_OK(ca.SignCertificate(ca, valid_after, valid_until));

  Certificate stratum_server(kCertCommonName, 1);
  EXPECT_OK(stratum_server.GenerateKeyPair(1024));
  EXPECT_OK(stratum_server.SignCertificate(ca, valid_after, valid_until));

  Certificate stratum_client(kCertCommonName, 1);
  EXPECT_OK(stratum_client.GenerateKeyPair(1024));
  EXPECT_OK(stratum_client.SignCertificate(ca, valid_after, valid_until));

  ASSIGN_OR_RETURN(*ca_crt, ca.GetCertificate());
  ASSIGN_OR_RETURN(*server_crt, stratum_server.GetCertificate());
  ASSIGN_OR_RETURN(*server_key, stratum_server.GetPrivateKey());
  ASSIGN_OR_RETURN(*client_crt, stratum_client.GetCertificate());
  ASSIGN_OR_RETURN(*client_key, stratum_client.GetPrivateKey());

  return util::OkStatus();
}

void WriteServerCredentialsToDisk(const std::string& server_ca_crt,
                                  const std::string& server_crt,
                                  const std::string& server_key) {
  ASSERT_OK(WriteStringToFile(server_ca_crt, FLAGS_server_ca_cert_file));
  ASSERT_OK(WriteStringToFile(server_crt, FLAGS_server_cert_file));
  ASSERT_OK(WriteStringToFile(server_key, FLAGS_server_key_file));
  absl::SleepFor(absl::Seconds(2));  // Wait for file watcher to update certs...
}

void WriteClientCredentialsToDisk(const std::string& client_ca_crt,
                                  const std::string& client_crt,
                                  const std::string& client_key) {
  ASSERT_OK(WriteStringToFile(client_ca_crt, FLAGS_client_ca_cert_file));
  ASSERT_OK(WriteStringToFile(client_crt, FLAGS_client_cert_file));
  ASSERT_OK(WriteStringToFile(client_key, FLAGS_client_key_file));
  absl::SleepFor(absl::Seconds(2));  // Wait for file watcher to update certs...
}

class CredentialsManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FLAGS_server_ca_cert_file =
        absl::StrFormat("%s/%s", FLAGS_test_tmpdir, kServerCaCertFile);
    FLAGS_server_cert_file =
        absl::StrFormat("%s/%s", FLAGS_test_tmpdir, kServerCertFile);
    FLAGS_server_key_file =
        absl::StrFormat("%s/%s", FLAGS_test_tmpdir, kServerKeyFile);
    FLAGS_client_ca_cert_file =
        absl::StrFormat("%s/%s", FLAGS_test_tmpdir, kClientCaCertFile);
    FLAGS_client_cert_file =
        absl::StrFormat("%s/%s", FLAGS_test_tmpdir, kClientCertFile);
    FLAGS_client_key_file =
        absl::StrFormat("%s/%s", FLAGS_test_tmpdir, kClientKeyFile);

    std::string ca_crt, server_crt, server_key, client_crt, client_key;
    EXPECT_OK(GenerateCerts(&ca_crt, &server_crt, &server_key, &client_crt,
                            &client_key));
    WriteServerCredentialsToDisk(ca_crt, server_crt, server_key);
    WriteClientCredentialsToDisk(ca_crt, client_crt, client_key);
    credentials_manager_ =
        CredentialsManager::CreateInstance().ConsumeValueOrDie();
    std::shared_ptr<::grpc::ServerCredentials> server_credentials =
        credentials_manager_->GenerateExternalFacingServerCredentials();

    std::string url =
        "localhost:" + std::to_string(stratum::PickUnusedPortOrDie());
    test_service_ = absl::make_unique<TestServiceImpl>();
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(url, server_credentials);
    builder.RegisterService(test_service_.get());
    server_ = builder.BuildAndStart();
    ASSERT_NE(server_, nullptr);

    ::grpc::ChannelArguments args;
    args.SetSslTargetNameOverride(kCertCommonName);
    auto channel = ::grpc::CreateCustomChannel(
        url, credentials_manager_->GenerateExternalFacingClientCredentials(),
        args);
    stub_ = ::testing::TestService::NewStub(channel);
    ASSERT_NE(stub_, nullptr);
  }

  ::util::Status Connect() {
    ::grpc::ClientContext context;
    context.set_wait_for_ready(false);  // fail fast
    ::testing::Empty request;
    ::testing::Empty response;
    auto status = stub_->Test(&context, request, &response);

    return ::util::Status(static_cast<::util::error::Code>(status.error_code()),
                          status.error_message());
  }

  void TearDown() override { server_->Shutdown(); }

  std::unique_ptr<CredentialsManager> credentials_manager_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<TestServiceImpl> test_service_;
  std::unique_ptr<::testing::TestService::Stub> stub_;
};

TEST_F(CredentialsManagerTest, ConnectSuccess) { EXPECT_OK(Connect()); }

TEST_F(CredentialsManagerTest, ConnectFailWrongCert) {
  std::string ca_crt, server_crt, server_key, client_crt, client_key;
  EXPECT_OK(GenerateCerts(&ca_crt, &server_crt, &server_key, &client_crt,
                          &client_key));
  WriteClientCredentialsToDisk(ca_crt, client_crt, client_key);
  ::util::Status status = Connect();
  EXPECT_FALSE(status.ok());
}

TEST_F(CredentialsManagerTest, ConnectAfterCertChange) {
  std::string ca_crt, server_crt, server_key, client_crt, client_key;
  EXPECT_OK(GenerateCerts(&ca_crt, &server_crt, &server_key, &client_crt,
                          &client_key));
  // Update server keys. Client connect will now fail because of CA mismatch.
  WriteServerCredentialsToDisk(ca_crt, server_crt, server_key);
  EXPECT_FALSE(Connect().ok());

  // Update client credentials. Connects will work again.
  WriteClientCredentialsToDisk(ca_crt, client_crt, client_key);
  EXPECT_OK(Connect());
}

TEST_F(CredentialsManagerTest, LoadNewServerCredentials) {
  std::string ca_crt, server_crt, server_key, client_crt, client_key;
  EXPECT_OK(GenerateCerts(&ca_crt, &server_crt, &server_key, &client_crt,
                          &client_key));
  ASSERT_FALSE(server_crt.empty());
  ASSERT_FALSE(server_key.empty());
  EXPECT_OK(credentials_manager_->LoadNewServerCredentials(ca_crt, server_crt,
                                                           server_key));

  // Read and verify the active key material files.
  std::string ca_cert_actual;
  std::string cert_actual;
  std::string key_actual;
  ASSERT_OK(ReadFileToString(FLAGS_server_ca_cert_file, &ca_cert_actual));
  ASSERT_OK(ReadFileToString(FLAGS_server_cert_file, &cert_actual));
  ASSERT_OK(ReadFileToString(FLAGS_server_key_file, &key_actual));
  EXPECT_EQ(ca_cert_actual, ca_crt);
  EXPECT_EQ(cert_actual, server_crt);
  EXPECT_EQ(key_actual, server_key);

  // Make sure client connections using the old CA certificates do not work.
  EXPECT_FALSE(Connect().ok());

  // Load new CA and connect.
  EXPECT_OK(credentials_manager_->LoadNewClientCredentials(ca_crt, client_crt,
                                                           client_key));
  EXPECT_OK(Connect());
}

}  // namespace
}  // namespace stratum
