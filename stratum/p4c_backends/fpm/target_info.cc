// This file implements TargetInfo static methods that manage the
// singleton instance.

#include "platforms/networking/hercules/p4c_backend/switch/target_info.h"

#include "base/logging.h"

namespace google {
namespace hercules {
namespace p4c_backend {

TargetInfo* TargetInfo::singleton_ = nullptr;

void TargetInfo::InjectSingleton(TargetInfo* target_info) {
  singleton_ = target_info;
}

TargetInfo* TargetInfo::GetSingleton() {
  CHECK(singleton_ != nullptr)
      << "The TargetInfo singleton has not been injected";
  return singleton_;
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
