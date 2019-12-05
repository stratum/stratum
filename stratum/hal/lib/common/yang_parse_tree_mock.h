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

#ifndef STRATUM_HAL_LIB_COMMON_YANG_PARSE_TREE_MOCK_H_
#define STRATUM_HAL_LIB_COMMON_YANG_PARSE_TREE_MOCK_H_

#include "gmock/gmock.h"
#include "stratum/hal/lib/common/yang_parse_tree.h"

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

#endif  // STRATUM_HAL_LIB_COMMON_YANG_PARSE_TREE_MOCK_H_
