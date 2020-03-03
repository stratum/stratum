// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/lib/security/credentials_manager.h"

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "absl/memory/memory.h"

namespace stratum {

CredentialsManager::CredentialsManager() {}

CredentialsManager::~CredentialsManager() {}

std::shared_ptr<::grpc::ServerCredentials>
CredentialsManager::GenerateExternalFacingServerCredentials() const {
  return ::grpc::InsecureServerCredentials();
}

std::unique_ptr<CredentialsManager> CredentialsManager::CreateInstance() {
  return absl::WrapUnique(new CredentialsManager());
}

}  // namespace stratum
