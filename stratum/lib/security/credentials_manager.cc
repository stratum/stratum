// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/security/credentials_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

DEFINE_string(ca_cert_file, "", "CA certificate path");
DEFINE_string(server_key_file, "", "gRPC Server private key path");
DEFINE_string(server_cert_file, "", "gRPC Server certificate path");

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
  if (FLAGS_ca_cert_file.empty() && FLAGS_server_key_file.empty() &&
      FLAGS_server_cert_file.empty()) {
    LOG(WARNING) << "No key files provided, using insecure credentials!";
    server_credentials_ = ::grpc::InsecureServerCredentials();
    client_credentials_ = ::grpc::InsecureChannelCredentials();
  } else {
    auto certificate_provider =
        std::make_shared<FileWatcherCertificateProvider>(
            FLAGS_server_key_file, FLAGS_server_cert_file, FLAGS_ca_cert_file,
            kFileRefreshIntervalSeconds);

    // Server credentials.
    {
      auto tls_opts =
          std::make_shared<TlsServerCredentialsOptions>(certificate_provider);
      tls_opts->set_cert_request_type(GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE);
      tls_opts->watch_root_certs();
      tls_opts->watch_identity_key_cert_pairs();
      server_credentials_ = TlsServerCredentials(*tls_opts);
    }

    // Client credentials.
    {
      auto tls_opts =
          std::make_shared<TlsChannelCredentialsOptions>(certificate_provider);
      tls_opts->set_server_verification_option(GRPC_TLS_SERVER_VERIFICATION);
      tls_opts->watch_root_certs();
      if (!FLAGS_ca_cert_file.empty() && !FLAGS_server_key_file.empty()) {
        tls_opts->watch_identity_key_cert_pairs();
      }
      client_credentials_ = ::grpc::experimental::TlsCredentials(*tls_opts);
    }
  }
  return ::util::OkStatus();
}

::util::Status CredentialsManager::LoadNewCredential(
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
  return status;
}

}  // namespace stratum
