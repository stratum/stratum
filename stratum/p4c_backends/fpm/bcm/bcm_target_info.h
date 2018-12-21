// BcmTargetInfo is a TargetInfo subclass for BCM switch chips.

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_BCM_BCM_TARGET_INFO_H_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_BCM_BCM_TARGET_INFO_H_

#include "platforms/networking/hercules/p4c_backend/switch/target_info.h"

namespace google {
namespace hercules {
namespace p4c_backend {

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

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_BCM_BCM_TARGET_INFO_H_
