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
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/lib/utils.h"

DEFINE_string(ca_cert, "", "CA certificate");
DEFINE_string(server_key, "", "gRPC Server pricate key");
DEFINE_string(server_cert, "", "gRPC Server certificate");

namespace stratum {

CredentialsManager::CredentialsManager() {}

CredentialsManager::~CredentialsManager() {}

std::shared_ptr<::grpc::ServerCredentials>
CredentialsManager::GenerateExternalFacingServerCredentials() const {
  if (FLAGS_ca_cert.empty() || FLAGS_server_key.empty() ||
      FLAGS_server_cert.empty()) {
    LOG(INFO) << "Using insecure server credentials";
    return ::grpc::InsecureServerCredentials();
  }
  ::grpc::SslServerCredentialsOptions ssl_opts;
  ::grpc::SslServerCredentialsOptions::PemKeyCertPair key_cert_pair;
  ::util::Status status;
  status.Update(
      ::stratum::ReadFileToString(FLAGS_ca_cert, &ssl_opts.pem_root_certs));
  status.Update(::stratum::ReadFileToString(FLAGS_server_key,
                                            &key_cert_pair.private_key));
  status.Update(::stratum::ReadFileToString(FLAGS_server_cert,
                                            &key_cert_pair.cert_chain));
  ssl_opts.pem_key_cert_pairs.push_back(key_cert_pair);

  if (!status.ok()) {
    LOG(WARNING) << "Invalid server credential, use insecure credential.";
    return ::grpc::InsecureServerCredentials();
  }

  return ::grpc::SslServerCredentials(ssl_opts);
}

std::unique_ptr<CredentialsManager> CredentialsManager::CreateInstance() {
  return absl::WrapUnique(new CredentialsManager());
}

}  // namespace stratum
