// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_YANG_YANG_PARSE_TREE_MOCK_H_
#define STRATUM_HAL_LIB_YANG_YANG_PARSE_TREE_MOCK_H_

#include "gmock/gmock.h"
#include "stratum/hal/lib/yang/yang_parse_tree.h"

namespace stratum {
namespace hal {

// A mock class for YangParseTree.
class YangParseTreeMock : public YangParseTree {
 public:
  explicit YangParseTreeMock(SwitchInterface* intf) : YangParseTree(intf) {}

  MOCK_METHOD1(SendNotification, void(const GnmiEventPtr& event));
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_YANG_YANG_PARSE_TREE_MOCK_H_
