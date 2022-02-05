// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This is a mock implementation of SwitchCaseDecoder.

#ifndef STRATUM_P4C_BACKENDS_FPM_SWITCH_CASE_DECODER_MOCK_H_
#define STRATUM_P4C_BACKENDS_FPM_SWITCH_CASE_DECODER_MOCK_H_

#include <map>
#include <set>
#include <string>

#include "gmock/gmock.h"
#include "stratum/p4c_backends/fpm/switch_case_decoder.h"

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
