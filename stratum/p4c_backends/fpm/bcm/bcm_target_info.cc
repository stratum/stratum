// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
