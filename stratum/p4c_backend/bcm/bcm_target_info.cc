// The BcmTargetInfo implementation is in this file.

#include "platforms/networking/hercules/p4c_backend/bcm/bcm_target_info.h"

#include "platforms/networking/hercules/public/proto/p4_annotation.host.pb.h"

namespace google {
namespace hercules {
namespace p4c_backend {

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

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
