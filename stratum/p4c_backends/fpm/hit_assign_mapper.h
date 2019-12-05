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

// The HitAssignMapper inspects IR::AssignmentStatements in P4Control logic for
// table-hit status assignments to temporary variables.  Upon finding such
// statements, it transforms them into an IR::TableHitStatement node for
// subsequent backend processing.  The transformed node is an IR::Statement
// subclass that contains the names of the temporary hit variable and the
// applied table.

#ifndef STRATUM_P4C_BACKENDS_FPM_HIT_ASSIGN_MAPPER_H_
#define STRATUM_P4C_BACKENDS_FPM_HIT_ASSIGN_MAPPER_H_

#include <string>

#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

class HitAssignMapper : public Transform {
 public:
  // The constructor requires p4c's ReferenceMap and TypeMap.
  HitAssignMapper(P4::ReferenceMap* ref_map, P4::TypeMap* type_map);
  ~HitAssignMapper() override {}

  // Applies the HitAssignMapper transform to the input control.  If any
  // transforms occur, Apply returns a pointer to a transformed control.
  // If no transforms occur, Apply returns the original control.  Apply
  // can be called multiple times to process separate IR::P4Control blocks.
  // There is no clear ownership of the returned P4Control pointer.  Instead
  // of establishing object ownership rules, p4c depends on a garbage collector
  // to free memory that is no longer used.  The Stratum p4c binary does
  // not enable this garbage collector.
  virtual const IR::P4Control* Apply(const IR::P4Control& control);

  // These methods override the IR::Transform base class to visit the nodes
  // under the inspected IR::P4Control.  Per p4c convention, they return
  // the input pointer when no transform occurs, or they return a new
  // IR::Node representing the transformed object.
  const IR::Node* preorder(IR::AssignmentStatement* statement) override;
  const IR::Node* preorder(IR::Expression* expression) override;

  // RunPreTestTransform typically runs during test setup
  // from IRTestHelperJson::TransformP4Control to prepare an IR for
  // testing other classes that depend on HitAssignMapper transforms.
  static const IR::P4Control* RunPreTestTransform(const IR::P4Control& control,
                                                  P4::ReferenceMap* ref_map,
                                                  P4::TypeMap* type_map);

  // HitAssignMapper is neither copyable nor movable.
  HitAssignMapper(const HitAssignMapper&) = delete;
  HitAssignMapper& operator=(const HitAssignMapper&) = delete;

 private:
  // These members store the injected parameters.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_HIT_ASSIGN_MAPPER_H_
