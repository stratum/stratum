// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file defines the TargetInfoMock class.

#ifndef STRATUM_P4C_BACKENDS_FPM_TARGET_INFO_MOCK_H_
#define STRATUM_P4C_BACKENDS_FPM_TARGET_INFO_MOCK_H_

#include "gmock/gmock.h"
#include "stratum/p4c_backends/fpm/target_info.h"

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
