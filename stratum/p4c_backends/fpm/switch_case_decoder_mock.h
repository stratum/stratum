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

// This is a mock implementation of SwitchCaseDecoder.

#ifndef STRATUM_P4C_BACKENDS_FPM_SWITCH_CASE_DECODER_MOCK_H_
#define STRATUM_P4C_BACKENDS_FPM_SWITCH_CASE_DECODER_MOCK_H_

#include <string>
#include <set>
#include <map>

#include "stratum/p4c_backends/fpm/switch_case_decoder.h"
#include "gmock/gmock.h"

namespace stratum {
namespace p4c_backends {

class SwitchCaseDecoderMock : public SwitchCaseDecoder {
 public:
  SwitchCaseDecoderMock() : SwitchCaseDecoder(empty_action_name_map_) {}

  MOCK_METHOD1(Decode, void(const IR::SwitchStatement& switch_statement));
  MOCK_CONST_METHOD0(applied_table, const IR::P4Table*());

 private:
  // Dummy objects to satisfy base class reference members.
  std::map<std::string, std::string> empty_action_name_map_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_SWITCH_CASE_DECODER_MOCK_H_
