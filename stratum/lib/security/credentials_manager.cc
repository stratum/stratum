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

#include "stratum/lib/security/credentials_manager.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "gflags/gflags.h"
#include "grpcpp/security/server_credentials.h"
#include "grpcpp/security/server_credentials_impl.h"
#include "grpcpp/security/tls_credentials_options.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
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

std::unique_ptr<CredentialsManager> CredentialsManager::CreateInstance() {
  auto instance_ = absl::WrapUnique(new CredentialsManager());
  auto status = instance_->Initialize();
  if (!status.ok()) {
    LOG(ERROR) << "Failed to initialize the CredentialsManager instance: "
               << status.error_message();
    return nullptr;
  }
  return instance_;
}

::util::Status CredentialsManager::Initialize() {
  if (FLAGS_ca_cert.empty() || FLAGS_server_key.empty() ||
      FLAGS_server_cert.empty()) {
    LOG(WARNING) << "Using insecure server credentials";
    server_credentials_ = ::grpc::InsecureServerCredentials();
  } else {
    // Load default credentials
    ::util::Status status;
    std::string pem_root_certs_;
    std::string server_private_key_;
    std::string server_cert_;
    status.Update(ReadFileToString(FLAGS_ca_cert, &pem_root_certs_));
    status.Update(ReadFileToString(FLAGS_server_key, &server_private_key_));
    status.Update(ReadFileToString(FLAGS_server_cert, &server_cert_));
    if (!status.ok()) {
      RETURN_ERROR().without_logging() << "Unable to load credentials.";
    }
    auto credentials_reload_interface_ =
        std::make_shared<CredentialsReloadInterface>(pem_root_certs_,
                                                     server_private_key_,
                                                     server_cert_);
    auto credential_reload_config_ =
        std::make_shared<TlsCredentialReloadConfig>(
            credentials_reload_interface_);

    tls_opts_ = std::make_shared<TlsCredentialsOptions>(
        GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE,
        GRPC_TLS_SERVER_VERIFICATION, nullptr,
        credential_reload_config_, nullptr);
    server_credentials_ = TlsServerCredentials(*tls_opts_);
  }
  return ::util::OkStatus();
}

::util::Status CredentialsManager::LoadNewCredential(
    const std::string root_certs, const std::string cert_chain,
    const std::string private_key) {
  return credentials_reload_interface_->LoadNewCredential(
      root_certs, cert_chain, private_key);
}

CredentialsReloadInterface::CredentialsReloadInterface(
    std::string pem_root_certs, std::string server_private_key,
    std::string server_cert)
    : reload_credential_(true),
      pem_root_certs_(pem_root_certs),
      server_private_key_(server_private_key),
      server_cert_(server_cert) {}

int CredentialsReloadInterface::Schedule(TlsCredentialReloadArg *arg) {
  absl::WriterMutexLock l(&credential_lock_);
  if (arg == nullptr) {
    arg->set_status(GRPC_SSL_CERTIFICATE_CONFIG_RELOAD_FAIL);
    return 1;
  }
  if (!reload_credential_) {
    arg->set_status(GRPC_SSL_CERTIFICATE_CONFIG_RELOAD_UNCHANGED);
    return 0;
  }

  TlsKeyMaterialsConfig::PemKeyCertPair pem_key_cert_pair_ = {
      server_private_key_, server_cert_};

  arg->set_pem_root_certs(pem_root_certs_);
  arg->add_pem_key_cert_pair(pem_key_cert_pair_);
  arg->set_status(GRPC_SSL_CERTIFICATE_CONFIG_RELOAD_NEW);
  reload_credential_ = false;
  return 0;
}

void CredentialsReloadInterface::Cancel(TlsCredentialReloadArg *arg) {
  if (arg == nullptr) return;
  arg->set_status(GRPC_SSL_CERTIFICATE_CONFIG_RELOAD_FAIL);
  arg->set_error_details("Cancelled.");
}

::util::Status CredentialsReloadInterface::LoadNewCredential(
    const std::string root_certs, const std::string cert_chain,
    const std::string private_key) {
  absl::WriterMutexLock l(&credential_lock_);
  // TODO(Yi): verify if key and cert are valid format
  CHECK_RETURN_IF_FALSE(!root_certs.empty());
  CHECK_RETURN_IF_FALSE(!cert_chain.empty());
  CHECK_RETURN_IF_FALSE(!private_key.empty());
  pem_root_certs_ = root_certs;
  server_cert_ = cert_chain;
  server_private_key_ = private_key;
  reload_credential_ = true;
  return ::util::OkStatus();
}

}  // namespace stratum
