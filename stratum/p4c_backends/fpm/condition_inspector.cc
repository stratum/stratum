// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file implements the Hercules p4c backend's ConditionInspector.

#include "stratum/p4c_backends/fpm/condition_inspector.h"

#include "base/logging.h"
#include "base/stringprintf.h"

namespace stratum {
namespace p4c_backends {

ConditionInspector::ConditionInspector() {
  description_ = "";
}

void ConditionInspector::Inspect(const IR::Expression& condition) {
  if (!description_.empty()) {
    LOG(ERROR) << "ConditionInspector does not inspect multiple conditions";
    return;
  }
  condition.apply(*this);
}

// Allows (a == b).
bool ConditionInspector::preorder(const IR::Equ* expression) {
  return Compare(*expression);
}

// Allows (a != b).
bool ConditionInspector::preorder(const IR::Neq* expression) {
  return Compare(*expression);
}

// Reports a compile error for any condition with complexity exceeding the
// expressions allowed above.
bool ConditionInspector::preorder(const IR::Operation_Binary* expression) {
  description_ = "Unrecognized condition";
  ::error("Condition expression %s is too complex for Hercules", expression);
  return false;  // No interest in visiting more IR nodes.
}

bool ConditionInspector::Compare(const IR::Operation_Relation& compare_op) {
  DCHECK_NE(nullptr, compare_op.left) << "Compare operation is missing LHS";
  DCHECK_NE(nullptr, compare_op.right) << "Compare operation is missing RHS";
  description_ = StringPrintf("%s %s %s", compare_op.left->toString().c_str(),
                              compare_op.getStringOp().c_str(),
                              compare_op.right->toString().c_str());

  // Deeper node visits are needed to make sure LHS and RHS aren't complex
  // expressions such as (field1 == (field2 + field3)).
  return true;
}

}  // namespace p4c_backends
}  // namespace stratum
