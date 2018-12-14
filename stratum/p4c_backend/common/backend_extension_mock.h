// This file declares a BackendExtensionInterface mock for unit tests.

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_COMMON_BACKEND_EXTENSION_MOCK_H_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_COMMON_BACKEND_EXTENSION_MOCK_H_

#include "platforms/networking/hercules/p4c_backend/common/backend_extension_interface.h"
#include "testing/base/public/gmock.h"

namespace google {
namespace hercules {
namespace p4c_backend {

class BackendExtensionMock : public BackendExtensionInterface {
 public:
  MOCK_METHOD5(Compile, void(const IR::ToplevelBlock& top_level,
                             const ::p4::v1::WriteRequest& static_table_entries,
                             const ::p4::config::v1::P4Info& p4_info,
                             P4::ReferenceMap* refMap, P4::TypeMap* typeMap));
};

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_COMMON_BACKEND_EXTENSION_MOCK_H_
