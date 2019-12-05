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

#include "absl/memory/memory.h"
#include "gflags/gflags.h"
#include "stratum/glue/logging.h"

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
