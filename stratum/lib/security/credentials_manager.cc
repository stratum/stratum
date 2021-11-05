// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/security/credentials_manager.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

DEFINE_string(ca_cert, "", "CA certificate path");
DEFINE_string(server_key, "", "gRPC Server pricate key path");
DEFINE_string(server_cert, "", "gRPC Server certificate path");

namespace stratum {

CredentialsManager::~CredentialsManager() {}
CredentialsManager::CredentialsManager() {}

std::shared_ptr<::grpc::ServerCredentials>
CredentialsManager::GenerateExternalFacingServerCredentials() const {
  return server_credentials_;
}

::util::StatusOr<std::unique_ptr<CredentialsManager>>
CredentialsManager::CreateInstance() {
  auto instance_ = absl::WrapUnique(new CredentialsManager());
  RETURN_IF_ERROR(instance_->Initialize());
  return std::move(instance_);
}

::util::Status CredentialsManager::Initialize() {
  if (FLAGS_ca_cert.empty() && FLAGS_server_key.empty() &&
      FLAGS_server_cert.empty()) {
    LOG(WARNING) << "Using insecure server credentials";
    server_credentials_ = ::grpc::InsecureServerCredentials();
  } else {
    certificate_provider_ = std::make_shared<FileWatcherCertificateProvider>(
        FLAGS_server_key, FLAGS_server_cert, FLAGS_ca_cert, 1);

    tls_opts_ =
        std::make_shared<TlsServerCredentialsOptions>(certificate_provider_);
    tls_opts_->set_cert_request_type(GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE);
    tls_opts_->watch_root_certs();
    tls_opts_->watch_identity_key_cert_pairs();

    server_credentials_ = TlsServerCredentials(*tls_opts_);
  }
  return ::util::OkStatus();
}

::util::Status CredentialsManager::LoadNewCredential(
    const std::string root_certs, const std::string cert_chain,
    const std::string private_key) {
  ::util::Status status;
  // TODO: Validate the provided key material if possible
  status.Update(WriteStringToFile(root_certs, FLAGS_ca_cert));
  status.Update(WriteStringToFile(cert_chain, FLAGS_server_cert));
  status.Update(WriteStringToFile(private_key, FLAGS_server_key));
  return status;
}

}  // namespace stratum
