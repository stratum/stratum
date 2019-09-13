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


#ifndef STRATUM_LIB_SECURITY_AUTH_POLICY_CHECKER_MOCK_H_
#define STRATUM_LIB_SECURITY_AUTH_POLICY_CHECKER_MOCK_H_

#include <string>

#include "stratum/lib/security/auth_policy_checker.h"
#include "gmock/gmock.h"

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
