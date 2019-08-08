/*
 * Copyright 2019 Google LLC
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

// This file defines the TargetInfoMock class.

#ifndef STRATUM_P4C_BACKENDS_FPM_TARGET_INFO_MOCK_H_
#define STRATUM_P4C_BACKENDS_FPM_TARGET_INFO_MOCK_H_

#include "stratum/p4c_backends/fpm/target_info.h"

#include "gmock/gmock.h"

namespace stratum {
namespace p4c_backends {

class TargetInfoMock : public TargetInfo {
 public:
  MOCK_CONST_METHOD1(IsPipelineStageFixed,
                     bool(P4Annotation::PipelineStage stage));
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_TARGET_INFO_MOCK_H_
