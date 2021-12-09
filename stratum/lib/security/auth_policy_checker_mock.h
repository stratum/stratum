// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_SECURITY_AUTH_POLICY_CHECKER_MOCK_H_
#define STRATUM_LIB_SECURITY_AUTH_POLICY_CHECKER_MOCK_H_

#include <string>

#include "gmock/gmock.h"
#include "stratum/lib/security/auth_policy_checker.h"

namespace stratum {

class AuthPolicyCheckerMock : public AuthPolicyChecker {
 public:
  MOCK_CONST_METHOD3(Authorize,
                     ::util::Status(const std::string& service_name,
                                    const std::string& rpc_name,
                                    const ::grpc::AuthContext& auth_context));
  MOCK_METHOD0(RefreshPolicies, ::util::Status());
  MOCK_METHOD0(Shutdown, ::util::Status());
};

}  // namespace stratum

#endif  // STRATUM_LIB_SECURITY_AUTH_POLICY_CHECKER_MOCK_H_
