/*
 * Copyright 2018 Google LLC
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

#ifndef THIRD_PARTY_STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_MOCK_H_
#define THIRD_PARTY_STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_MOCK_H_

#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
#include "testing/base/public/gmock.h"
#include "util/task/status.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class MockOnlpWrapper : public OnlpInterface {
 public:
  ~MockOnlpWrapper() override{};

  MOCK_CONST_METHOD1(GetOidInfo, ::util::StatusOr<OidInfo>(OnlpOid oid));
  MOCK_CONST_METHOD1(GetSfpInfo, ::util::StatusOr<SfpInfo>(OnlpOid oid));
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
#endif  // THIRD_PARTY_STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_MOCK_H_
