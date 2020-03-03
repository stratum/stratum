// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file implements TargetInfo static methods that manage the
// singleton instance.

#include "stratum/p4c_backends/fpm/target_info.h"

#include "stratum/glue/logging.h"

namespace stratum {
namespace p4c_backends {

TargetInfo* TargetInfo::singleton_ = nullptr;

void TargetInfo::InjectSingleton(TargetInfo* target_info) {
  singleton_ = target_info;
}

TargetInfo* TargetInfo::GetSingleton() {
  CHECK(singleton_ != nullptr)
      << "The TargetInfo singleton has not been injected";
  return singleton_;
}

}  // namespace p4c_backends
}  // namespace stratum
