// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

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
  MOCK_CONST_METHOD3(LoadNewCredential,
      ::util::Status(const std::string ca_cert, const std::string cert,
                     const std::string key));
};

}  // namespace stratum

#endif  // STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_MOCK_H_
