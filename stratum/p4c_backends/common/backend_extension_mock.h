/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
