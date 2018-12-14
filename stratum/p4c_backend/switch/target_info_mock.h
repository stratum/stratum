// This file defines the TargetInfoMock class.

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TARGET_INFO_MOCK_H_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TARGET_INFO_MOCK_H_

#include "platforms/networking/hercules/p4c_backend/switch/target_info.h"

#include "testing/base/public/gmock.h"

namespace google {
namespace hercules {
namespace p4c_backend {

class TargetInfoMock : public TargetInfo {
 public:
  MOCK_CONST_METHOD1(IsPipelineStageFixed,
                     bool(P4Annotation::PipelineStage stage));
};

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TARGET_INFO_MOCK_H_
