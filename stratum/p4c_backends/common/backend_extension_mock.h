// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file declares a BackendExtensionInterface mock for unit tests.

#ifndef STRATUM_P4C_BACKENDS_COMMON_BACKEND_EXTENSION_MOCK_H_
#define STRATUM_P4C_BACKENDS_COMMON_BACKEND_EXTENSION_MOCK_H_

#include "stratum/p4c_backends/common/backend_extension_interface.h"
#include "gmock/gmock.h"

namespace stratum {
namespace p4c_backends {

class BackendExtensionMock : public BackendExtensionInterface {
 public:
  MOCK_METHOD5(Compile, void(const IR::ToplevelBlock& top_level,
                             const ::p4::v1::WriteRequest& static_table_entries,
                             const ::p4::config::v1::P4Info& p4_info,
                             P4::ReferenceMap* refMap, P4::TypeMap* typeMap));
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_COMMON_BACKEND_EXTENSION_MOCK_H_
