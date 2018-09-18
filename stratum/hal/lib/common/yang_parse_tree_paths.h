#ifndef PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_YANG_PARSE_TREE_PATHS_H_
#define PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_YANG_PARSE_TREE_PATHS_H_

#include "platforms/networking/hercules/hal/lib/common/yang_parse_tree.h"

namespace google {
namespace hercules {
namespace hal {

// A companion class to YangParseTree class that contains implementation of all
// suppored YANG paths. Having the actual path implementation here makes the
// code easier to manage and will allow for (future) generation of this part of
// this code.
class YangParseTreePaths {
 public:
  // A helper method handling mundane details of sending a  message that marks
  // the end of series of update messages.
  static ::util::Status SendEndOfSeriesMessage(GnmiSubscribeStream* stream);

  // Adds all supported paths for the specified singleton interface.
  static void AddSubtreeInterfaceFromSingleton(
      const SingletonPort& singleton, const NodeConfigParams& node_config,
      YangParseTree* tree) EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_);

  // Adds all supported paths for the specified trunk interface.
  static void AddSubtreeInterfaceFromTrunk(const std::string& name,
                                           uint64 node_id, uint32 port_id,
                                           const NodeConfigParams& node_config,
                                           YangParseTree* tree)
      EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_);

  // Adds all supported paths for the specified node.
  static void AddSubtreeNode(const Node& node, YangParseTree* tree)
      EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_);

  // Adds all supported paths for the specified chassis.
  static void AddSubtreeChassis(const Chassis& chassis, YangParseTree* tree)
      EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_);

  // Adds all supported wildcard interface-related paths.
  static void AddSubtreeAllInterfaces(YangParseTree* tree)
      EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_);

  // Configure the root element.
  static void AddRoot(YangParseTree* tree)
      EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_);

 private:
  // Adds all supported paths for the specified interface.
  static TreeNode* AddSubtreeInterface(const std::string& name, uint64 node_id,
                                       uint32 port_id,
                                       const NodeConfigParams& node_config,
                                       YangParseTree* tree)
      EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_);
};

}  // namespace hal
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_YANG_PARSE_TREE_PATHS_H_
