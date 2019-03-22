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

// The MeterColorMapper inspects IR::IfStatements in P4Control logic for
// conditions that act based on the meter color.  Upon finding such statements,
// it transforms them into an IR::MeterColorStatement node for subsequent
// backend processing.  The transformed node is an IR::IfStatement subclass
// with additional information linking it to a P4MeterColorAction message.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_METER_COLOR_MAPPER_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_METER_COLOR_MAPPER_H_

#include <string>

#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

class MeterColorMapper : public Transform {
 public:
  // The constructor requires p4c's ReferenceMap and TypeMap as well as
  // a TableMapGenerator for its internal use.
  MeterColorMapper(P4::ReferenceMap* ref_map, P4::TypeMap* type_map,
                   TableMapGenerator* table_mapper);
  ~MeterColorMapper() override {}

  // Applies the MeterColorMapper transform to the input control.  If any
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
  // IR::Node representing the transformed statement.
  const IR::Node* preorder(IR::BlockStatement* statement) override;
  const IR::Node* preorder(IR::IfStatement* statement) override;
  const IR::Node* preorder(IR::MethodCallStatement* statement) override;
  const IR::Node* preorder(IR::Statement* statement) override;

  // RunPreTestTransform typically runs during test setup
  // from IRTestHelperJson::TransformP4Control to prepare an IR for
  // testing other classes that depend on MeterColorMapper transforms.
  // It expects the test to call SetP4ModelNames first to establish
  // proper P4 model settings relative to the tested P4Control.
  static const IR::P4Control* RunPreTestTransform(
      const IR::P4Control& control, const std::string& color_field_name,
      P4::ReferenceMap* ref_map, P4::TypeMap* type_map);

  // MeterColorMapper is neither copyable nor movable.
  MeterColorMapper(const MeterColorMapper&) = delete;
  MeterColorMapper& operator=(const MeterColorMapper&) = delete;

 private:
  // Reinitializes all members related to the state of the most recent
  // Apply run on a P4Control.
  void ClearControlState();

  // Decodes the conditional expression within an IR::IfStatement, returning
  // true if the condition can be realized on Stratum switches.
  bool DecodeCondition(const IR::IfStatement& statement);

  // Sets the color condition flags based on the input enum member value.
  void SetColorConditions(const std::string& color_value);

  // Performs a logical NOT operation on current color conditions.
  void InvertColorConditions();

  // These members store the injected parameters.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;
  TableMapGenerator* table_mapper_;

  // This message contains P4MeterColorAction data that MeterColorMapper
  // accumulates while transforming the IR.
  hal::P4ActionDescriptor color_actions_;

  // These members track the decoded state of the current control.
  bool condition_equal_;   // True when a condition compares for equality.
  bool green_condition_;   // True when a condition affects green behavior.
  bool yellow_condition_;  // True when a condition affects yellow behavior.
  bool red_condition_;     // True when a condition affects red behavior.
  bool transforming_if_;   // True when a transform of an IfStatement with a
                           // meter condition is in progress.
};

// This is a p4c Inspector subclass that decides whether a single
// IR::IfStatement can transform into an IR::MeterColorStatement.  It exists
// primarily as a helper class for MeterColorMapper.  This class does not
// perform any transform, it just makes the transform decision.
class IfStatementColorInspector : public Inspector {
 public:
  IfStatementColorInspector();
  ~IfStatementColorInspector() override {}

  // Inspects the input IfStatement to decide if it can be transformed into
  // an IR::MeterColorStatement.  A transform can occur when the input
  // IfStatement's condition evaluates the meter color.  When no transform
  // is possible, Transform returns false.  When a transform is possible,
  // Transform returns true.  The transform decision does not consider
  // whether an IR::MeterColorStatement is valid in its current context.
  // TODO: Consider how much validation is useful to do here.
  bool CanTransform(const IR::IfStatement& statement);

  // These methods override the IR::Inspector base class to visit the nodes
  // under the inspected IfStatement.  Per p4c convention, the preorder
  // functions return true to visit deeper nodes in the IR, or false if the
  // IfStatementColorInspector does not need to visit any deeper nodes.
  bool preorder(const IR::Equ* condition) override;
  bool preorder(const IR::Neq* condition) override;
  bool preorder(const IR::Operation_Relation* condition) override;
  bool preorder(const IR::Expression* expression) override;
  bool preorder(const IR::Member* member) override;

  // Accessors.
  bool negate() const { return negate_; }
  const std::string& color_value() const { return color_value_; }
  const std::string& color_field() const { return color_field_; }

  // IfStatementColorInspector is neither copyable nor movable.
  IfStatementColorInspector(const IfStatementColorInspector&) = delete;
  IfStatementColorInspector& operator=(const IfStatementColorInspector&) =
      delete;

 private:
  // Evaluates whether the given IR::Member node represents the meter
  // color enum.
  static bool IsMemberColorEnum(const IR::Member& member);

  // These two members are false until IfStatementColorInspector encounters
  // an IR::Equ or an IR::Neq node, after which they are set as follows:
  //              IR::Equ  IR::Neq
  //  equ_found_    true     true
  //  negate_       false    true
  bool equ_found_;
  bool negate_;

  // This member counts the number of relational operators that appear in
  // an IfStatement's condition.
  int relational_operators_;

  // These strings store the condition operands for color-based decisions.
  // The color_value_ identifies a color type enum value, such as "GREEN"
  // or "RED".  The color_field_ identifies the field name being compared,
  // such as "local_metadata.color".
  std::string color_value_;
  std::string color_field_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_METER_COLOR_MAPPER_H_
