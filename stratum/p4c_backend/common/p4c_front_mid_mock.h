// This file declares a p4cFrontMidInterface mock for unit tests.

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_COMMON_P4C_FRONT_MID_MOCK_H_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_COMMON_P4C_FRONT_MID_MOCK_H_

#include "platforms/networking/hercules/p4c_backend/common/p4c_front_mid_interface.h"
#include "testing/base/public/gmock.h"

namespace google {
namespace hercules {
namespace p4c_backend {

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

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_COMMON_P4C_FRONT_MID_MOCK_H_
