// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// BcmTargetInfo is a TargetInfo subclass for BCM switch chips.

#ifndef STRATUM_P4C_BACKENDS_FPM_BCM_BCM_TARGET_INFO_H_
#define STRATUM_P4C_BACKENDS_FPM_BCM_BCM_TARGET_INFO_H_

#include "stratum/p4c_backends/fpm/target_info.h"

namespace stratum {
namespace p4c_backends {

class BcmTargetInfo : public TargetInfo {
 public:
  BcmTargetInfo() {}
  ~BcmTargetInfo() override {}

  // This override returns true for BCM pipeline stages with fixed logic.
  bool IsPipelineStageFixed(P4Annotation::PipelineStage stage) const override;

  // BcmTargetInfo is neither copyable nor movable.
  BcmTargetInfo(const BcmTargetInfo&) = delete;
  BcmTargetInfo& operator=(const BcmTargetInfo&) = delete;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_BCM_BCM_TARGET_INFO_H_
