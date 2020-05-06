// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


// This is a mock implementation of P4StaticEntryMapper.

#ifndef STRATUM_HAL_LIB_P4_P4_STATIC_ENTRY_MAPPER_MOCK_H_
#define STRATUM_HAL_LIB_P4_P4_STATIC_ENTRY_MAPPER_MOCK_H_

#include "stratum/hal/lib/p4/p4_static_entry_mapper.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {

class P4StaticEntryMapperMock : public P4StaticEntryMapper {
 public:
  MOCK_METHOD2(HandlePrePushChanges,
               ::util::Status(const ::p4::v1::WriteRequest& new_static_config,
                              ::p4::v1::WriteRequest* out_request));
  MOCK_METHOD2(HandlePostPushChanges,
               ::util::Status(const ::p4::v1::WriteRequest& new_static_config,
                              ::p4::v1::WriteRequest* out_request));
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_STATIC_ENTRY_MAPPER_MOCK_H_
