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

#ifndef STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_MOCK_H_
#define STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_MOCK_H_

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "stratum/lib/security/credentials_manager.h"

namespace stratum {

class CredentialsManagerMock : public CredentialsManager {
 public:
  MOCK_CONST_METHOD0(GenerateExternalFacingServerCredentials,
                     std::shared_ptr<::grpc::ServerCredentials>());
  MOCK_CONST_METHOD3(
      LoadNewCredential,
      ::util::Status<const std::string ca_cert, const std::string cert,
                     const std::string key>);
};

}  // namespace stratum

#endif  // STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_MOCK_H_
