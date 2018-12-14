// This file declares a TunnelOptimizerInterface mock for unit tests.

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TUNNEL_OPTIMIZER_MOCK_H_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TUNNEL_OPTIMIZER_MOCK_H_

#include "platforms/networking/hercules/p4c_backend/switch/tunnel_optimizer_interface.h"
#include "testing/base/public/gmock.h"

namespace google {
namespace hercules {
namespace p4c_backend {

class TunnelOptimizerMock : public TunnelOptimizerInterface {
 public:
  MOCK_METHOD2(Optimize,
               bool(const hal::P4ActionDescriptor& input_action,
                    hal::P4ActionDescriptor* optimized_action));
  MOCK_METHOD3(MergeAndOptimize,
               bool(const hal::P4ActionDescriptor& input_action1,
                    const hal::P4ActionDescriptor& input_action2,
                    hal::P4ActionDescriptor* optimized_action));
};

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TUNNEL_OPTIMIZER_MOCK_H_
