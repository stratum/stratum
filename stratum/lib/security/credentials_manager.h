// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_H_
#define STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_H_

#include <memory>
#include <string>

#include "grpcpp/grpcpp.h"
#include "grpcpp/security/server_credentials.h"
#include "grpcpp/security/tls_credentials_options.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
using ::grpc::experimental::FileWatcherCertificateProvider;
using ::grpc::experimental::TlsServerCredentials;
using ::grpc::experimental::TlsServerCredentialsOptions;

// CredentialsManager manages the server credentials for (external facing) gRPC
// servers. It handles starting and shutting down TSI as well as generating the
// server credentials. This class is supposed to be created
// once for each binary.
class CredentialsManager {
 public:
  virtual ~CredentialsManager();

  // Generates server credentials for an external facing gRPC
  // server.
  virtual std::shared_ptr<::grpc::ServerCredentials>
  GenerateExternalFacingServerCredentials() const;

  // Factory functions for creating the instance of the class.
  static ::util::StatusOr<std::unique_ptr<CredentialsManager>> CreateInstance();

  // CredentialsManager is neither copyable nor movable.
  CredentialsManager(const CredentialsManager&) = delete;
  CredentialsManager& operator=(const CredentialsManager&) = delete;

  // Loads new credentials
  ::util::Status LoadNewCredential(const std::string ca_cert,
                                   const std::string cert,
                                   const std::string key);

 protected:
  // Default constructor. To be called by the Mock class instance as well as
  // CreateInstance().
  CredentialsManager();

 private:
  // Function to initialize the credentials manager.
  ::util::Status Initialize();
  std::shared_ptr<::grpc::ServerCredentials> server_credentials_;
  std::shared_ptr<TlsServerCredentialsOptions> tls_opts_;
  std::shared_ptr<FileWatcherCertificateProvider> certificate_provider_;
};

}  // namespace stratum
#endif  // STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_H_
