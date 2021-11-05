// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
#include "stratum/lib/security/credentials_manager.h"

#include <fstream>
#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "absl/strings/substitute.h"
#include "gmock/gmock.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "stratum/glue/net_util/ports.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/utils.h"
#include "stratum/lib/security/test.grpc.pb.h"

DECLARE_string(ca_cert);
DECLARE_string(server_key);
DECLARE_string(server_cert);
DECLARE_string(test_tmpdir);

namespace stratum {
namespace {

// Ken's contribution to ONF
class TestServiceImpl final : public ::testing::TestService::Service {
 public:
    ::grpc::Status Test(::grpc::ServerContext* /*context*/,
                        const ::testing::Empty* /*request*/,
                        ::testing::Empty* /*response*/) override {
      return ::grpc::Status::OK;
    }
};

enum class CertFile {
  kCaCert = 0,
  kServerCert = 1,
  kServerKey = 2,
};

constexpr char kCertDirPrefix[] = "stratum/lib/security/testdata/certs";
constexpr char kCaCertFile[] = "ca.crt";
constexpr char kServerCertFile[] = "stratum.crt";
constexpr char kServerKeyFile[] = "stratum.key";

std::string GetFilename(CertFile f, int i) {
  const char* file;
  switch(f) {
    case CertFile::kCaCert:
      file = kCaCertFile;
      break;
    case CertFile::kServerCert:
      file = kServerCertFile;
      break;
    case CertFile::kServerKey:
      file = kServerKeyFile;
      break;
  }
  return absl::StrFormat("%s%d/%s", kCertDirPrefix, i, file);
}

void CopyFile(std::string from, std::string to) {
  std::ifstream src(from, std::ios::binary);
  std::ofstream dst(to, std::ios::binary);
  ASSERT_TRUE(src.is_open());
  ASSERT_TRUE(dst.is_open());
  dst << src.rdbuf();
}

void SetCerts(int i) {
  CopyFile(GetFilename(CertFile::kCaCert, i), FLAGS_ca_cert);
  CopyFile(GetFilename(CertFile::kServerCert, i), FLAGS_server_cert);
  CopyFile(GetFilename(CertFile::kServerKey, i), FLAGS_server_key);
}

class CredentialsManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FLAGS_ca_cert = absl::StrFormat("%s/%s", FLAGS_test_tmpdir, kCaCertFile);
    FLAGS_server_cert = absl::StrFormat("%s/%s", FLAGS_test_tmpdir, kServerCertFile);
    FLAGS_server_key = absl::StrFormat("%s/%s", FLAGS_test_tmpdir, kServerKeyFile);
    SetCerts(1);  // Set certs 1 as the default
    credentials_manager_ = CredentialsManager::CreateInstance().ConsumeValueOrDie();
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

  void Connect(int i, bool expectSuccess = true) {
    std::string root_certs;
    ASSERT_OK(ReadFileToString(GetFilename(CertFile::kCaCert, i), &root_certs));

    auto cert_provider = std::make_shared<::grpc::experimental::StaticDataCertificateProvider>(root_certs);
    auto tls_opts = std::make_shared<::grpc::experimental::TlsChannelCredentialsOptions>();
    tls_opts->watch_root_certs();
    tls_opts->set_certificate_provider(cert_provider);
    auto channel_creds = ::grpc::experimental::TlsCredentials(*tls_opts);

    ::grpc::ChannelArguments args;
    args.SetSslTargetNameOverride("stratum.local");
    auto channel = ::grpc::CreateCustomChannel(url_, channel_creds, args);
    auto stub = ::testing::TestService::NewStub(channel);
    
    ::grpc::ClientContext context;
    ::testing::Empty request;
    ::testing::Empty response;
    ::grpc::Status status = stub->Test(&context, request, &response);
    ASSERT_TRUE(status.ok() == expectSuccess);
  }

  void TearDown() override { server_->Shutdown(); }

  std::string url_;
  std::unique_ptr<CredentialsManager> credentials_manager_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<TestServiceImpl> test_service_;
};

TEST_F(CredentialsManagerTest, ConnectSuccess) {
  Connect(1);
}

TEST_F(CredentialsManagerTest, ConnectFailWrongCert) {
  Connect(2, false);
}

TEST_F(CredentialsManagerTest, ConnectAfterUpdate) {
  SetCerts(2);
  sleep(2);  // Wait for file watcher to update certs...
  Connect(2);
  Connect(1, false);
}

TEST_F(CredentialsManagerTest, ChangeCertFileCheck) {
  std::string ca_cert;
  std::string cert;
  std::string key;
  ASSERT_OK(ReadFileToString(GetFilename(CertFile::kCaCert, 2), &ca_cert));
  ASSERT_OK(ReadFileToString(GetFilename(CertFile::kServerCert, 2), &cert));
  ASSERT_OK(ReadFileToString(GetFilename(CertFile::kServerKey, 2), &key));
  EXPECT_OK(credentials_manager_->LoadNewCredential(ca_cert, cert, key));

  // Read and verify the active key material files
  std::string ca_cert_actual;
  std::string cert_actual;
  std::string key_actual;
  ASSERT_OK(ReadFileToString(FLAGS_ca_cert, &ca_cert_actual));
  ASSERT_OK(ReadFileToString(FLAGS_server_cert, &cert_actual));
  ASSERT_OK(ReadFileToString(FLAGS_server_key, &key_actual));
  EXPECT_EQ(ca_cert_actual, ca_cert);
  EXPECT_EQ(cert_actual, cert);
  EXPECT_EQ(key_actual, key);
  
  // Make sure connections work
  sleep(2);  // Wait for file watcher to update certs...
  Connect(2);
  Connect(1, false);
}

}  // namespace
}  // namespace stratum
