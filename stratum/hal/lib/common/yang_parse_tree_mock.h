#ifndef PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_YANG_PARSE_TREE_MOCK_H_
#define PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_YANG_PARSE_TREE_MOCK_H_

#include "platforms/networking/hercules/hal/lib/common/yang_parse_tree.h"
#include "testing/base/public/gmock.h"

namespace google {
namespace hercules {
namespace hal {

// A mock class for YangParseTree.
class YangParseTreeMock : public YangParseTree {
 public:
  explicit YangParseTreeMock(SwitchInterface* intf) : YangParseTree(intf) {}

  MOCK_METHOD1(SendNotification, void(const GnmiEventPtr& event));
};

}  // namespace hal
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_YANG_PARSE_TREE_MOCK_H_
