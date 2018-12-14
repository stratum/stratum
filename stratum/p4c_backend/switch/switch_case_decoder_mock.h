// This is a mock implementation of SwitchCaseDecoder.

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_SWITCH_CASE_DECODER_MOCK_H_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_SWITCH_CASE_DECODER_MOCK_H_

#include "platforms/networking/hercules/p4c_backend/switch/switch_case_decoder.h"
#include "testing/base/public/gmock.h"

namespace google {
namespace hercules {
namespace p4c_backend {

class SwitchCaseDecoderMock : public SwitchCaseDecoder {
 public:
  SwitchCaseDecoderMock() : SwitchCaseDecoder(empty_action_name_map_) {}

  MOCK_METHOD1(Decode, void(const IR::SwitchStatement& switch_statement));
  MOCK_CONST_METHOD0(applied_table, const IR::P4Table*());

 private:
  // Dummy objects to satisfy base class reference members.
  std::map<std::string, std::string> empty_action_name_map_;
};

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_SWITCH_CASE_DECODER_MOCK_H_
