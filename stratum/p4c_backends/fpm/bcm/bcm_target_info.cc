// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// The BcmTargetInfo implementation is in this file.

#include "stratum/p4c_backends/fpm/bcm/bcm_target_info.h"

#include "stratum/public/proto/p4_annotation.pb.h"

namespace stratum {
namespace p4c_backends {

bool BcmTargetInfo::IsPipelineStageFixed(
    P4Annotation::PipelineStage stage) const {
  bool is_fixed = false;
  switch (stage) {
    case P4Annotation::L2:
    case P4Annotation::L3_LPM:
    case P4Annotation::L3_MPLS:
    case P4Annotation::ENCAP:
    case P4Annotation::DECAP:
      is_fixed = true;
      break;
    default:
      is_fixed = false;
      break;
  }
  return is_fixed;
}

}  // namespace p4c_backends
}  // namespace stratum
