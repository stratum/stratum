// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/security/credentials_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

DEFINE_string(ca_cert_file, "", "Path to CA certificate file");
DEFINE_string(server_key_file, "", "Path to gRPC server private key file");
DEFINE_string(server_cert_file, "", "Path to gRPC server certificate file");
DEFINE_string(client_key_file, "", "Path to gRPC client key file");
DEFINE_string(client_cert_file, "", "Path to gRPC client certificate file");

namespace stratum {

using ::grpc::experimental::FileWatcherCertificateProvider;
using ::grpc::experimental::TlsChannelCredentialsOptions;
using ::grpc::experimental::TlsServerCredentials;
using ::grpc::experimental::TlsServerCredentialsOptions;

constexpr unsigned int CredentialsManager::kFileRefreshIntervalSeconds;

CredentialsManager::CredentialsManager() {}

CredentialsManager::~CredentialsManager() {}

::util::StatusOr<std::unique_ptr<CredentialsManager>>
CredentialsManager::CreateInstance() {
  auto instance = absl::WrapUnique(new CredentialsManager());
  RETURN_IF_ERROR(instance->Initialize());
  return std::move(instance);
}

std::shared_ptr<::grpc::ServerCredentials>
CredentialsManager::GenerateExternalFacingServerCredentials() const {
  return server_credentials_;
}

std::shared_ptr<::grpc::ChannelCredentials>
CredentialsManager::GenerateExternalFacingClientCredentials() const {
  return client_credentials_;
}

::util::Status CredentialsManager::Initialize() {
  // Server credentials.
  if (FLAGS_ca_cert_file.empty() && FLAGS_server_key_file.empty() &&
      FLAGS_server_cert_file.empty()) {
    LOG(WARNING) << "No key files provided, using insecure server credentials!";
    server_credentials_ = ::grpc::InsecureServerCredentials();
  } else {
    auto certificate_provider =
        std::make_shared<FileWatcherCertificateProvider>(
            FLAGS_server_key_file, FLAGS_server_cert_file, FLAGS_ca_cert_file,
            kFileRefreshIntervalSeconds);
    auto tls_opts =
        std::make_shared<TlsServerCredentialsOptions>(certificate_provider);
    tls_opts->set_cert_request_type(GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE);
    tls_opts->watch_root_certs();
    tls_opts->watch_identity_key_cert_pairs();
    server_credentials_ = TlsServerCredentials(*tls_opts);
  }

  // Client credentials.
  if (FLAGS_ca_cert_file.empty() && FLAGS_client_key_file.empty() &&
      FLAGS_client_cert_file.empty()) {
    client_credentials_ = ::grpc::InsecureChannelCredentials();
    LOG(WARNING) << "No key files provided, using insecure client credentials!";
  } else {
    auto certificate_provider =
        std::make_shared<FileWatcherCertificateProvider>(
            FLAGS_client_key_file, FLAGS_client_cert_file, FLAGS_ca_cert_file,
            kFileRefreshIntervalSeconds);
    auto tls_opts = std::make_shared<TlsChannelCredentialsOptions>();
    tls_opts->set_certificate_provider(certificate_provider);
    tls_opts->set_verify_server_certs(true);
    tls_opts->watch_root_certs();
    if (!FLAGS_ca_cert_file.empty() && !FLAGS_client_key_file.empty()) {
      tls_opts->watch_identity_key_cert_pairs();
    }
    client_credentials_ = ::grpc::experimental::TlsCredentials(*tls_opts);
  }

  return ::util::OkStatus();
}

::util::Status CredentialsManager::LoadNewServerCredentials(
    const std::string& root_certs, const std::string& cert_chain,
    const std::string& private_key) {
  ::util::Status status;
  // TODO(Kevin): Validate the provided key material if possible
  // TODO(max): According to the API of FileWatcherCertificateProvider, any key
  // and certifcate update must happen atomically. The below code does not
  // guarantee that.
  status.Update(WriteStringToFile(root_certs, FLAGS_ca_cert_file));
  status.Update(WriteStringToFile(cert_chain, FLAGS_server_cert_file));
  status.Update(WriteStringToFile(private_key, FLAGS_server_key_file));
  absl::SleepFor(absl::Seconds(kFileRefreshIntervalSeconds + 1));

  return status;
}

::util::Status CredentialsManager::LoadNewClientCredentials(
    const std::string& root_certs, const std::string& cert_chain,
    const std::string& private_key) {
  ::util::Status status;
  // TODO(Kevin): Validate the provided key material if possible
  // TODO(max): According to the API of FileWatcherCertificateProvider, any key
  // and certifcate update must happen atomically. The below code does not
  // guarantee that.
  status.Update(WriteStringToFile(root_certs, FLAGS_ca_cert_file));
  status.Update(WriteStringToFile(cert_chain, FLAGS_client_cert_file));
  status.Update(WriteStringToFile(private_key, FLAGS_client_key_file));
  absl::SleepFor(absl::Seconds(kFileRefreshIntervalSeconds + 1));

  return status;
}

}  // namespace stratum
