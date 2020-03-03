// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/hal/lib/phal/attribute_group_mock.h"

#include <memory>
#include "absl/memory/memory.h"

namespace stratum {
namespace hal {
namespace phal {

std::unique_ptr<MutableAttributeGroup> MakeMockGroup(
    AttributeGroupMock* group) {  // NOLINT
  return absl::make_unique<LockedAttributeGroupMock>(group);
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
