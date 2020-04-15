/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_H_
#define STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_H_

#include <memory>
#include <string>

#include "absl/synchronization/mutex.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/tls_credentials_options.h"
#include "stratum/glue/status/status.h"

namespace stratum {
using TlsCredentialReloadInterface =
    ::grpc_impl::experimental::TlsCredentialReloadInterface;
using TlsCredentialReloadArg =
    ::grpc_impl::experimental::TlsCredentialReloadArg;
using TlsCredentialReloadConfig =
    grpc_impl::experimental::TlsCredentialReloadConfig;
using TlsKeyMaterialsConfig = ::grpc_impl::experimental::TlsKeyMaterialsConfig;
using TlsCredentialsOptions = ::grpc_impl::experimental::TlsCredentialsOptions;
using ::grpc_impl::experimental::TlsServerCredentials;

// CredentialsManager manages the server credentials for (external facing) gRPC
// servers. It handles starting and shutting down TSI as well as generating the
// server credentials. This class is supposed to be created
// once for each binary.
class CredentialsManager : public TlsCredentialReloadInterface {
 public:
  virtual ~CredentialsManager();

  // Generates server credentials for an external facing gRPC
  // server.
  virtual std::shared_ptr<::grpc::ServerCredentials>
  GenerateExternalFacingServerCredentials() const;

  // Factory function for creating the instance of the class.
  static std::unique_ptr<CredentialsManager> CreateInstance();

  // CredentialsManager is neither copyable nor movable.
  CredentialsManager(const CredentialsManager &) = delete;
  CredentialsManager &operator=(const CredentialsManager &) = delete;

  // Public methods from TlsCredentialReloadInterface
  int Schedule(TlsCredentialReloadArg *arg) override
      LOCKS_EXCLUDED(credential_lock_);
  void Cancel(TlsCredentialReloadArg *arg) override;

  // Loads new credentials
  ::util::Status LoadNewCredential(const std::string ca_cert,
                                   const std::string cert,
                                   const std::string key)
      LOCKS_EXCLUDED(credential_lock_);

 protected:
  // Default constructor. To be called by the Mock class instance as well as
  // CreateInstance().
  CredentialsManager();

 private:
  std::shared_ptr<::grpc::ServerCredentials> server_credentials_;
  std::shared_ptr<TlsCredentialsOptions> tls_opts_;

  absl::Mutex credential_lock_;
  bool reload_credential GUARDED_BY(credential_lock_);
  ::grpc::string pem_root_certs_ GUARDED_BY(credential_lock_);
  ::grpc::string server_private_key_ GUARDED_BY(credential_lock_);
  ::grpc::string server_cert_ GUARDED_BY(credential_lock_);
};
}  // namespace stratum
#endif  // STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_H_
