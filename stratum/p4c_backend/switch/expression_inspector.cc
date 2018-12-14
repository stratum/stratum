// This file implements the p4c backend ExpressionInspector.

#include "platforms/networking/hercules/p4c_backend/switch/expression_inspector.h"

#include "base/logging.h"
#include "platforms/networking/hercules/p4c_backend/switch/field_name_inspector.h"
#include "platforms/networking/hercules/p4c_backend/switch/utils.h"
#include "absl/debugging/leak_check.h"

namespace google {
namespace hercules {
namespace p4c_backend {

ExpressionInspector::ExpressionInspector(P4::ReferenceMap* ref_map,
                                         P4::TypeMap* type_map)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)),
      value_valid_(false),
      inspect_expression_(nullptr) {}

bool ExpressionInspector::Inspect(const IR::Expression& expression) {
  value_.Clear();
  value_valid_ = false;
  inspect_expression_ = &expression;
  absl::LeakCheckDisabler disable_ir_expression_leak_checks;
  expression.apply(*this);
  inspect_expression_ = nullptr;
  return value_valid_;
}

bool ExpressionInspector::preorder(const IR::Member* member) {
  // Most IR::Members should be some kind of field or header name
  // that a FieldNameInspector can interpret.
  FieldNameInspector field_inspector;
  field_inspector.ExtractName(*member);

  if (member->type->is<IR::Type_Bits>()) {
    value_.set_source_field_name(field_inspector.field_name());
    value_valid_ = true;
  } else if (member->type->is<IR::Type_Header>() ||
             member->type->is<IR::Type_Stack>()) {
    // A Type_Stack member will not have the array index here.  It is added
    // by the ArrayIndex preorder below.
    value_.set_source_header_name(field_inspector.field_name());
    value_valid_ = true;
  } else if (member->type->is<IR::Type_Enum>()) {
    // TODO(teverman): Add support here to handle enum values.
    LOG(WARNING) << "Ignoring assignment from IR::Member Type_Enum - "
                 << inspect_expression_->srcInfo.toBriefSourceFragment();
  } else {
    ::error("Unsupported IR::Member type in expression %s", member);
  }
  return false;  // Don't visit deeper nodes.
}

// This preorder expects to be called with the top-level node in the inspection
// hierarchy when Inspect's input IR::Expression is the PathExpression subclass.
// It assumes that the PathExpression represents an action parameter.  Other
// preorders must avoid descending into this preorder when their expression type
// has a nested PathExpression, or the action parameter assumption will be
// violated.
bool ExpressionInspector::preorder(const IR::PathExpression* path) {
  VLOG(1) << "preorder PathExpression " << path->toString();
  // TODO(teverman): The bmv2 backend's ExpressionConverter now does a ref_map_
  // declaration lookup on the path and verifies that it really is an
  // IR::Parameter.
  value_.set_parameter_name(path->toString());
  value_valid_ = true;
  return false;  // Don't visit deeper nodes.
}

bool ExpressionInspector::preorder(const IR::Constant* constant) {
  VLOG(1) << "preorder Constant " << constant->toString();
  value_.set_constant_param(constant->asLong());

  // Bit width constants are IR::Type_Bits.
  auto type_bits = constant->type->to<IR::Type_Bits>();
  if (type_bits != nullptr) {
    value_.set_bit_width(type_bits->size);
    value_valid_ = true;
    return false;  // Don't visit deeper nodes.
  }

  // Slice operator operands and array indices are Type_InfInt.
  auto inf_int = constant->type->to<IR::Type_InfInt>();
  if (inf_int != nullptr) {
    value_valid_ = true;
    return false;  // Don't visit deeper nodes.
  }

  DLOG(FATAL) << "IR::Constant in "
              << inspect_expression_->srcInfo.toBriefSourceFragment()
              << " is not IR::Type_Bits or IR::Type_InfInt";
  return false;  // Don't visit deeper nodes.
}

// The IR::Slice is an IR::Operation_Ternary subclass with three
// sub-expressions:
//  e0 - represents the header field or parameter being sliced.
//  e1 - a constant representing the high-order bit of the slice.
//  e1 - a constant representing the low-order bit of the slice.
bool ExpressionInspector::preorder(const IR::Slice* slice) {
  ExpressionInspector value_inspector(ref_map_, type_map_);
  if (!value_inspector.Inspect(*slice->e0))
    return false;  // Don't visit deeper nodes.
  DCHECK_NE(P4AssignSourceValue::SOURCE_VALUE_NOT_SET,
            value_inspector.value().source_value_case());
  ExpressionInspector high_bit_inspector(ref_map_, type_map_);
  if (!high_bit_inspector.Inspect(*slice->e1))
    return false;  // Don't visit deeper nodes.
  DCHECK_EQ(P4AssignSourceValue::kConstantParam,
            high_bit_inspector.value().source_value_case());
  ExpressionInspector low_bit_inspector(ref_map_, type_map_);
  if (!low_bit_inspector.Inspect(*slice->e2))
    return false;  // Don't visit deeper nodes.
  DCHECK_EQ(P4AssignSourceValue::kConstantParam,
            low_bit_inspector.value().source_value_case());
  DCHECK_GE(high_bit_inspector.value().bit_width(),
            low_bit_inspector.value().bit_width());

  // Consolidate each sub-inspector's value() into this inspector's value_.
  value_ = value_inspector.value();
  value_.set_high_bit(high_bit_inspector.value().constant_param());
  value_.set_bit_width(1 + value_.high_bit() -
                       low_bit_inspector.value().constant_param());
  value_valid_ = true;
  return false;  // Don't visit deeper nodes.
}

// The IR::Add preorder has not been implemented, so it
// does not set value_valid_.
bool ExpressionInspector::preorder(const IR::Add* add) {
  // The only use case is for adjusting header length fields
  // during encap/decap.  BCM should do this without any input.
  LOG(WARNING) << "Ignoring assignment from IR::Add - "
               << inspect_expression_->srcInfo.toBriefSourceFragment();
  return false;  // Don't visit deeper nodes.
}

bool ExpressionInspector::preorder(const IR::ArrayIndex* array_index) {
  // The "right" expression is the array index value.  The Hercules backend
  // requires a constant index (as does the bmv2 backend).
  if (!array_index->right->is<IR::Constant>()) {
    ::error("%1%: all array indices must be constant for Hercules switches",
            array_index->right);
    return false;  // Don't visit deeper nodes.
  }

  // Hercules restricts the "left" expression to a header represented by
  // an IR:Member.  Temporary arrays (represented as IR::PathExpression) are
  // not allowed.
  if (!array_index->left->is<IR::Member>()) {
    ::error("%1%: only stacked headers can be arrays on Hercules switches",
            array_index->left);
    return false;  // Don't visit deeper nodes.
  }

  // Two additional ExpressionInspectors are applied to the left and right
  // expressions, then their outputs are combined to get the overall
  // array_index output value.
  ExpressionInspector header_inspector(ref_map_, type_map_);
  if (!header_inspector.Inspect(*array_index->left))
    return false;  // Don't visit deeper nodes.
  DCHECK_EQ(P4AssignSourceValue::kSourceHeaderName,
            header_inspector.value().source_value_case());
  ExpressionInspector index_inspector(ref_map_, type_map_);
  if (!index_inspector.Inspect(*array_index->right))
    return false;  // Don't visit deeper nodes.
  DCHECK_EQ(P4AssignSourceValue::kConstantParam,
            index_inspector.value().source_value_case());
  value_.set_source_header_name(
      AddHeaderArrayIndex(header_inspector.value().source_header_name(),
                          index_inspector.value().constant_param()));
  value_valid_ = true;
  return false;  // Don't visit deeper nodes.
}

// This preorder handles all IR::Expression subclasses that don't have an
// explicit preorder of their own.  It considers these to be a P4 program
// error, which it reports via p4c's ErrorReporter.
bool ExpressionInspector::preorder(const IR::Expression* unsupported) {
  ::error("Unsupported expression %s", unsupported);
  return false;  // Don't visit deeper nodes.
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
