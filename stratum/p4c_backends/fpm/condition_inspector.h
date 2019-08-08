/*
 * Copyright 2019 Google LLC
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

// The ConditionsInspector is a p4c Inspector subclass that visits the node
// hierarchy under an IR::Expression that defines an IfStatement condition in
// a P4 program.  The current output is a descriptive string for the condition.
// TODO(unknown): Add more detailed output as needed; the current role
// of this class is limited to describing unsupported conditions in the
// ControlInspector's P4Control output.

#ifndef STRATUM_P4C_BACKENDS_FPM_CONDITION_INSPECTOR_H_
#define STRATUM_P4C_BACKENDS_FPM_CONDITION_INSPECTOR_H_

#include <string>

#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"

namespace stratum {
namespace p4c_backends {

// A ConditionInspector instance operates on one IR::Expression to generate
// a description of the condition.  Typical usage is to construct a
// ConditionInspector, call the Inspect method with the condition of interest,
// and then use the output available from the description() accessor.
class ConditionInspector : public Inspector {
 public:
  ConditionInspector();
  ~ConditionInspector() override {}

  // The Inspect method visits the IR node hierarchy underneath the input
  // condition and produces a text string that is available through the
  // description() accessor upon return.  Inspect should only be called
  // once per ControlInspector instance.  If Inspect is unable to interpret
  // the input condition, the description() method contains "Unrecognized
  // condition".
  void Inspect(const IR::Expression& condition);

  // These methods override the IR::Inspector base class to visit the nodes
  // under the inspected IR::Expression.  Per p4c convention, they return true
  // to visit deeper nodes in the IR, or false if the ConditionInspector does
  // not need to visit any deeper nodes.
  bool preorder(const IR::Equ* expression) override;
  bool preorder(const IR::Neq* expression) override;
  bool preorder(const IR::Operation_Binary* expression) override;

  // Accesses the description formed by the Inspect method.
  const std::string& description() const { return description_; }

  // ConditionInspector is neither copyable nor movable.
  ConditionInspector(const ConditionInspector&) = delete;
  ConditionInspector& operator=(const ConditionInspector&) = delete;

 private:
  // Handles conditions for preorder methods that compare two values.
  bool Compare(const IR::Operation_Relation& compare_op);

  std::string description_;  // Contains the output string.
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_CONDITION_INSPECTOR_H_
