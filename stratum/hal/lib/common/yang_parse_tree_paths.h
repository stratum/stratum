/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_COMMON_YANG_PARSE_TREE_PATHS_H_
#define STRATUM_HAL_LIB_COMMON_YANG_PARSE_TREE_PATHS_H_

#include <string>

#include "stratum/hal/lib/common/yang_parse_tree.h"

namespace stratum {
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
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_YANG_PARSE_TREE_PATHS_H_
