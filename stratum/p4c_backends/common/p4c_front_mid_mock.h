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

// This file declares a p4cFrontMidInterface mock for unit tests.

#ifndef STRATUM_P4C_BACKENDS_COMMON_P4C_FRONT_MID_MOCK_H_
#define STRATUM_P4C_BACKENDS_COMMON_P4C_FRONT_MID_MOCK_H_

#include "gmock/gmock.h"
#include "stratum/p4c_backends/common/p4c_front_mid_interface.h"

namespace stratum {
namespace p4c_backends {

class P4cFrontMidMock : public P4cFrontMidInterface {
 public:
  MOCK_METHOD0(Initialize, void());
  MOCK_METHOD2(ProcessCommandLineOptions, int(int argc, char* const argv[]));
  MOCK_METHOD0(ParseP4File, const IR::P4Program*());
  MOCK_METHOD0(RunFrontEndPass, const IR::P4Program*());
  MOCK_METHOD0(RunMidEndPass, IR::ToplevelBlock*());
  MOCK_METHOD2(GenerateP4Runtime, void(std::ostream* p4info_out,
                                       std::ostream* static_table_entries_out));
  MOCK_METHOD0(GetErrorCount, unsigned());
  MOCK_METHOD0(GetMidEndReferenceMap, P4::ReferenceMap*());
  MOCK_METHOD0(GetMidEndTypeMap, P4::TypeMap*());
  MOCK_CONST_METHOD0(IsV1Program, bool());
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_COMMON_P4C_FRONT_MID_MOCK_H_
