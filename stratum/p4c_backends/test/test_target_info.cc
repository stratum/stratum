// The TestTargetInfo implementation is in this file.

#include "platforms/networking/hercules/p4c_backend/test/test_target_info.h"

namespace google {
namespace hercules {
namespace p4c_backend {

TestTargetInfo* TestTargetInfo::test_singleton_ = nullptr;

void TestTargetInfo::SetUpTestTargetInfo() {
  test_singleton_ = new TestTargetInfo;
  TargetInfo::InjectSingleton(test_singleton_);
}

void TestTargetInfo::TearDownTestTargetInfo() {
  TargetInfo::InjectSingleton(nullptr);
  delete test_singleton_;
  test_singleton_ = nullptr;
}

bool TestTargetInfo::IsPipelineStageFixed(
    P4Annotation::PipelineStage stage) const {
  if (stage == P4Annotation::L2 || stage == P4Annotation::L3_LPM) {
    return true;
  }
  return false;
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
