// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_YANG_YANG_PARSE_TREE_COMPONENT_H_
#define STRATUM_HAL_LIB_YANG_YANG_PARSE_TREE_COMPONENT_H_

#include "stratum/hal/lib/yang/yang_parse_tree.h"

namespace stratum {
namespace hal {
namespace yang {
namespace component {

// /components/component[name=<name>]/config/name
void SetUpComponentsComponentConfigName(
    const std::string& name, TreeNode* node);

// /components/component[name=<name>]/name
void SetUpComponentsComponentName(const std::string& name, TreeNode* node);

// /components/component[name=<name>]/state/type
void SetUpComponentsComponentStateType(
    const std::string& type, TreeNode* node);

// /components/component[name=<name>]/state/description
void SetUpComponentsComponentStateDescription(
    const std::string& description, TreeNode* node);

// /components/component[name=<name>]/state/part-no
void SetUpComponentsComponentStatePartNo(
    uint64 node_id, TreeNode* node, YangParseTree* tree);

// /components/component[name=<name>]/state/mfg-name
void SetUpComponentsComponentStateMfgName(
    uint64 node_id, TreeNode* node, YangParseTree* tree);
 
} // namespace component
} // namespace yang
} // namespace hal
} // namespace stratum

#endif  // STRATUM_HAL_LIB_YANG_YANG_PARSE_TREE_COMPONENT_H_
