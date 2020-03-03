// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_MOCK_H_
#define STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_MOCK_H_

#include <memory>

#include "stratum/lib/security/credentials_manager.h"
#include "gmock/gmock.h"

namespace stratum {

class CredentialsManagerMock : public CredentialsManager {
 public:
  MOCK_CONST_METHOD0(GenerateExternalFacingServerCredentials,
                     std::shared_ptr<::grpc::ServerCredentials>());
};

}  // namespace stratum

#endif  // STRATUM_LIB_SECURITY_CREDENTIALS_MANAGER_MOCK_H_
