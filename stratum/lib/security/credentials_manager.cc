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

#include "absl/memory/memory.h"
#include "gflags/gflags.h"
#include "grpcpp/security/server_credentials.h"
#include "grpcpp/security/server_credentials_impl.h"
#include "grpcpp/security/tls_credentials_options.h"

#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/lib/utils.h"

DEFINE_string(ca_cert, "", "CA certificate path");
DEFINE_string(server_key, "", "gRPC Server pricate key path");
DEFINE_string(server_cert, "", "gRPC Server certificate path");

namespace stratum {

using TlsKeyMaterialsConfig = ::grpc_impl::experimental::TlsKeyMaterialsConfig;
using TlsCredentialReloadConfig =
    ::grpc_impl::experimental::TlsCredentialReloadConfig;
using TlsCredentialsOptions = ::grpc_impl::experimental::TlsCredentialsOptions;
using TlsKeyMaterialsConfig = ::grpc_impl::experimental::TlsKeyMaterialsConfig;
using TlsCredentialReloadArg =
    ::grpc_impl::experimental::TlsCredentialReloadArg;
using ::grpc_impl::experimental::TlsServerCredentials;

int CredentialReloadManager::Schedule(TlsCredentialReloadArg *arg) {
  if (arg == nullptr) {
    return 1;
  }
  ::grpc::string pem_root_certs;
  TlsKeyMaterialsConfig::PemKeyCertPair pem_key_cert_pair;
  ::util::Status status;
  status.Update(
      ::stratum::ReadFileToString(FLAGS_ca_cert, &pem_root_certs));
  status.Update(::stratum::ReadFileToString(FLAGS_server_key,
                                            &pem_key_cert_pair.private_key));
  status.Update(::stratum::ReadFileToString(FLAGS_server_cert,
                                            &pem_key_cert_pair.cert_chain));
  arg->set_pem_root_certs(pem_root_certs);
  arg->add_pem_key_cert_pair(pem_key_cert_pair);
  arg->set_status(GRPC_SSL_CERTIFICATE_CONFIG_RELOAD_NEW);
  return 0;
}

void CredentialReloadManager::Cancel(TlsCredentialReloadArg *arg) {
  if (arg == nullptr) return;
  arg->set_status(GRPC_SSL_CERTIFICATE_CONFIG_RELOAD_FAIL);
  arg->set_error_details("Cancelled.");
}

CredentialsManager::~CredentialsManager() {}

CredentialsManager::CredentialsManager() {
  if (FLAGS_ca_cert.empty() || FLAGS_server_key.empty() ||
      FLAGS_server_cert.empty()) {
    LOG(INFO) << "Using insecure server credentials";
    server_credentials_ = ::grpc::InsecureServerCredentials();
  } else {
    credential_reload_ =
        std::shared_ptr<CredentialReloadManager>(new CredentialReloadManager());
    credential_reload_config_ = std::shared_ptr<TlsCredentialReloadConfig>(
        new TlsCredentialReloadConfig(credential_reload_));

    TlsCredentialsOptions tls_opts(
      GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE,
      GRPC_TLS_SERVER_VERIFICATION,
      nullptr,
      credential_reload_config_,
      nullptr
    );

    server_credentials_ = TlsServerCredentials(tls_opts);
  }
}

std::shared_ptr<::grpc::ServerCredentials>
CredentialsManager::GenerateExternalFacingServerCredentials() const {
  return server_credentials_;
}

std::unique_ptr<CredentialsManager> CredentialsManager::CreateInstance() {
  return absl::WrapUnique(new CredentialsManager());
}


}  // namespace stratum
