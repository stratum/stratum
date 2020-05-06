// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

#include "stratum/p4c_backends/fpm/parser_decoder.h"

#include "stratum/glue/logging.h"
#include "stratum/p4c_backends/fpm/field_name_inspector.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "absl/debugging/leak_check.h"
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/frontends/p4/methodInstance.h"

namespace stratum {
namespace p4c_backends {

ParserDecoder::ParserDecoder()
    : ref_map_(nullptr),
      type_map_(nullptr) {
}

// TODO(unknown): The Stratum p4c backend needs a consistent approach for
// handling errors.  Errors can occur in several ways:
// - Bad input from prior p4c passes due to undetected compiler bugs.
// - Unrecognized input from prior passes due to new p4c features or changes.
// - P4 programs using features that this backend does not support.
//    a) Features that will never be supported.
//    b) Features planned but not yet implemented.
// - P4 programs using features that the target platform does not support.
bool ParserDecoder::DecodeParser(const IR::P4Parser& p4_parser,
                                 P4::ReferenceMap* ref_map,
                                 P4::TypeMap* type_map) {
  VLOG(2) << __PRETTY_FUNCTION__;
  if (!parser_states_.parser_states().empty()) {
    LOG(ERROR) << "Multiple attempts to decode the P4Parser";
    return false;
  }

  if (VLOG_IS_ON(2)) ::dump(&p4_parser);
  ref_map_ = ABSL_DIE_IF_NULL(ref_map);
  type_map_ = ABSL_DIE_IF_NULL(type_map);
  const IR::ParserState* start_state = nullptr;

  // This loop iterates the parser locals to find all the value sets.
  // It is modeled after bmv2's parser.cpp code.
  for (const auto& local : p4_parser.parserLocals) {
    auto value_set = local->to<IR::P4ValueSet>();
    if (value_set == nullptr) continue;
    value_sets_[local->controlPlaneName().c_str()] =
        value_set->elementType->width_bits();
  }

  // This loop iterates the states in the P4Parser.  It creates a parser_states_
  // map entry with the decoded output for each encountered state.
  for (const auto ir_parser_state : p4_parser.states) {
    // We can't use externalName() here. See pull request #182
    const std::string state_name =
                            std::string(ir_parser_state->getName().toString());
    VLOG(2) << "ParserState: " << state_name;
    ParserState* decoded_state =
        &(*parser_states_.mutable_parser_states())[state_name];
    if (!decoded_state->name().empty()) {
      // TODO(unknown): Should this be handled as a compiler bug?
      LOG(FATAL) << "Multiple P4Parser states have name " << state_name;
      return false;
    }
    decoded_state->set_name(state_name);

    // A single "start" state must exist.  All other reserved states get
    // marked in their decoded_state.
    if (ir_parser_state->name == IR::ParserState::start) {
      DCHECK(start_state == nullptr);
      VLOG(2) << state_name << " is the parser start state";
      start_state = ir_parser_state;
      decoded_state->set_reserved_state(ParserState::P4_PARSER_STATE_START);
    } else if (ir_parser_state->name == IR::ParserState::accept) {
      decoded_state->set_reserved_state(ParserState::P4_PARSER_STATE_ACCEPT);
    } else if (ir_parser_state->name == IR::ParserState::reject) {
      decoded_state->set_reserved_state(ParserState::P4_PARSER_STATE_REJECT);
    }

    // The components at the top level of each state node represent statements,
    // such as "extract".
    DecodeStatements(ir_parser_state->components, decoded_state);

    // The state's selectExpression is an IR::SelectExpression when the state
    // contains a select statement to choose the next state.  Otherwise, an
    // IR::PathExpression unconditionally sets the next state.
    if (ir_parser_state->selectExpression != nullptr) {
      if (ir_parser_state->selectExpression->is<IR::SelectExpression>()) {
        DecodeSelectExpression(
            *(ir_parser_state->selectExpression->to<IR::SelectExpression>()),
            decoded_state);
      } else if (ir_parser_state->selectExpression->is<IR::PathExpression>()) {
        DecodePathExpression(
            *(ir_parser_state->selectExpression->to<IR::PathExpression>()),
            decoded_state);
      } else {
        LOG(ERROR) << "Unexpected selectExpression type in parser state"
                   << state_name;
        return false;
      }
    }
  }

  if (start_state == nullptr) {
    // TODO(unknown): Is this a compiler bug?  Promote to FATAL?
    LOG(ERROR) << "P4Parser has no start state";
    return false;
  }

  return true;
}

// Looks at the input components for a MethodCallStatement or an
// AssignmentStatement.  MethodCallStatements represent statements to
// parse a specific header type.  AssignmentStatements can provide clues
// about metadata fields based on the type of the right-hand side of the
// expression.
bool ParserDecoder::DecodeStatements(
    const IR::Vector<IR::StatOrDecl>& components, ParserState* decoded_state) {
  VLOG(1) << decoded_state->name() << " components count is "
          << components.size();
  for (const auto component : components) {
    auto method_call = component->to<IR::MethodCallStatement>();
    if (method_call != nullptr) {
      absl::LeakCheckDisabler disable_ir_method_call_leak_checks;
      const std::string header = ExtractHeaderType(*method_call);
      if (!header.empty()) {
        // The fields within this type are extracted by other parts of the
        // Stratum backend, which will append them to the decoded state later.
        decoded_state->mutable_extracted_header()->set_name(header);
        FieldNameInspector path_inspector;
        path_inspector.ExtractName(
            *method_call->methodCall->arguments->at(0)->expression);
        if (path_inspector.stacked_header_names().empty()) {
          decoded_state->mutable_extracted_header()->add_header_paths(
              path_inspector.field_name());
        } else {
          for (const auto& stacked : path_inspector.stacked_header_names()) {
            decoded_state->mutable_extracted_header()->
                add_header_paths(stacked);
          }
        }
      } else {
        LOG(WARNING) << "MethodCallStatement is not an extract statement";
      }
    } else if (component->is<IR::AssignmentStatement>()) {
      auto assignment = component->to<IR::AssignmentStatement>();
      VLOG(1) << "AssignmentStatement: " << assignment->toString();
      // TODO(unknown): Add implementation.
    } else {
      LOG(WARNING) << "Ignoring unknown component " << component->toString();
    }
  }

  return true;
}

bool ParserDecoder::DecodeSelectExpression(
    const IR::SelectExpression& expression, ParserState* decoded_state) {
  CHECK(expression.select != nullptr)
      << "Compiler bug: SelectExpression has no selector";

  // This loop decodes all the cases in the input expression.
  ParserSelectExpression* decoded_select =
      decoded_state->mutable_transition()->mutable_select();
  for (auto ir_select_case : expression.selectCases) {
    auto key_set = ir_select_case->keyset;
    auto decoded_case = decoded_select->add_cases();

    // This code is adapted from the BMV2 JsonConverter::combine method.  The
    // key_set may indicate this case is the default, it may have a simple
    // one-value expression if the select argument is one field, or it may
    // have a complex multi-value expression when the select uses multiple
    // arguments.
    if (key_set->is<IR::DefaultExpression>()) {
      decoded_case->set_is_default(true);
    } else if (key_set->is<IR::ListExpression>()) {
      auto key_list_expression = key_set->to<IR::ListExpression>();
      DecodeComplexSelectKeySet(*key_list_expression, *expression.select,
                                decoded_case);
    } else if (expression.select->components.size() == 1) {
      DecodeSimpleSelectKeySet(*key_set, decoded_case);
    } else {
      LOG(ERROR) << "Unexpected key set in select case for parser state"
                 << decoded_state->name();
      return false;
    }

    decoded_case->set_next_state(ir_select_case->state->path->name.toString());
  }

  // The select components identify the select statement's argument fields.
  for (const auto component : expression.select->components) {
    if (component->is<IR::Member>()) {
      // The field_name for the selector is relative to the header type.  It
      // is not a fully-qualified name in "header.field" format.  This avoids
      // difficulties with the way the parser IR encodes stack headers, but
      // it means a later step needs to deal with rolling the selector fields
      // up into their respective headers.
      decoded_select->add_selector_fields(
          component->to<IR::Member>()->member.name);
    } else if (component->is<IR::Concat>()) {
      DecodeConcatOperator(*component->to<IR::Concat>(), decoded_select);
    } else if (component->is<IR::Slice>()) {
      // If the compiler assigns a temporary variable to evaluate the select,
      // the temporary variable is of no interest in P4 table mapping, so it
      // just gets a name placeholder.
      VLOG(1) << "Found select slice " << component->toString()
              << " in parser state " << decoded_state->name();
      decoded_select->add_selector_fields("temporary-field-slice");
    } else if (component->is<IR::PathExpression>()) {
      decoded_select->add_selector_fields(
          component->to<IR::PathExpression>()->path->name.name.c_str());
    } else {
      LOG(ERROR) << "Unrecognized selector expression " << component->toString()
                 << " in parser state " << decoded_state->name();
    }
  }

  return true;
}

// The PathExpression applies to parser states that unconditionally set the
// next state without any select expression.
bool ParserDecoder::DecodePathExpression(const IR::PathExpression& expression,
                                         ParserState* decoded_state) {
  decoded_state->mutable_transition()->set_next_state(
      expression.path->name.name);
  return true;
}

// This code is adapted from p4c's bmv2 backend JsonConverter::convertSimpleKey.
// It figures out the value and mask for the input key_set and stores them
// in the decoded_case output.
void ParserDecoder::DecodeSimpleSelectKeySet(
    const IR::Expression& key_set, ParserSelectCase* decoded_case) {
  mpz_class value;
  mpz_class mask;
  if (key_set.is<IR::Mask>()) {
    auto mk = key_set.to<IR::Mask>();
    if (!mk->left->is<IR::Constant>()) {
      LOG(ERROR) << mk->left << " must evaluate to a compile-time constant";
      return;
    }
    if (!mk->right->is<IR::Constant>()) {
      LOG(ERROR) << mk->right << " must evaluate to a compile-time constant";
      return;
    }
    value = mk->left->to<IR::Constant>()->value;
    mask = mk->right->to<IR::Constant>()->value;
  } else if (key_set.is<IR::Constant>()) {
    value = key_set.to<IR::Constant>()->value;
    mask = -1;
  } else if (key_set.is<IR::BoolLiteral>()) {
    value = key_set.to<IR::BoolLiteral>()->value ? 1 : 0;
    mask = -1;
  } else if (DecodeValueSetSelectKeySet(key_set, decoded_case)) {
    return;
  } else {
    LOG(ERROR) << key_set << " must evaluate to a compile-time constant "
               << "or a parser value set";
    value = 0;
    mask = 0;
  }

  ParserKeySetValue* case_value = decoded_case->add_keyset_values();
  case_value->mutable_constant()->set_value(value.get_ui());
  case_value->mutable_constant()->set_mask(mask.get_ui());
}

// A ListExpression means the select key uses a combination of multiple
// fields, and the expression lists the key values for each field.  The
// size of the key_set list must match the number of fields in the
// select component list.
void ParserDecoder::DecodeComplexSelectKeySet(
    const IR::ListExpression& key_set, const IR::ListExpression& select,
    ParserSelectCase* decoded_case) {
  if (key_set.components.size() != select.components.size()) {
    // TODO(unknown): Should the compiler catch this?
    LOG(ERROR) << "Number of values in select case key set does not match "
               << "the number of select arguments";
    return;
  }

  for (auto key_element : key_set.components) {
    DecodeSimpleSelectKeySet(*key_element, decoded_case);
  }
}

bool ParserDecoder::DecodeValueSetSelectKeySet(
    const IR::Expression& key_set, ParserSelectCase* decoded_case) {
  if (!key_set.is<IR::PathExpression>()) return false;
  if (!key_set.type->is<IR::Type_Set>()) return false;
  auto path_expression = key_set.to<IR::PathExpression>();
  auto declaration = ref_map_->getDeclaration(path_expression->path, true);
  const auto& iter = value_sets_.find(declaration->controlPlaneName().c_str());
  DCHECK(iter != value_sets_.end())
      << "Possible compiler bug: unrecognized value set name "
      << path_expression->path->name.name;
  ParserKeySetValue* case_value = decoded_case->add_keyset_values();
  case_value->mutable_value_set()->set_value_set_name(iter->first);

  return true;
}

void ParserDecoder::DecodeConcatOperator(
    const IR::Concat& concat, ParserSelectExpression* decoded_select) {
  if (!concat.type->is<IR::Type_Bits>()) {
    LOG(ERROR) << "Expected P4 concat operator Type_Bits, found "
               << concat.node_type_name();
    return;
  }
  if (!concat.left->is<IR::Member>()) {
    LOG(ERROR) << "Expected P4 concat operator left side type Member, found "
               << concat.left->node_type_name();
    return;
  }
  if (!concat.right->is<IR::Member>()) {
    LOG(ERROR) << "Expected P4 concat operator right side type Member, found "
               << concat.right->node_type_name();
    return;
  }
  const auto left = concat.left->to<IR::Member>();
  if (!left->type->is<IR::Type_Bits>()) {
    LOG(ERROR) << "Expected concat operator left side to be Type_Bits";
    return;
  }
  const auto right = concat.right->to<IR::Member>();
  if (!right->type->is<IR::Type_Bits>()) {
    LOG(ERROR) << "Expected concat operator right side to be Type_Bits";
    return;
  }

  // P4's concat operator forms two fields into one, with the left field
  // in the higher bits, and the right field in the lower bits.
  const size_t concat_bit_size = concat.type->to<IR::Type_Bits>()->size;
  const size_t left_bit_size = left->type->to<IR::Type_Bits>()->size;
  const size_t right_bit_size = right->type->to<IR::Type_Bits>()->size;
  if (concat_bit_size != left_bit_size + right_bit_size) {
    LOG(ERROR) << "Compiler bug: concatenated field bit size "
               << concat_bit_size << " does not equal the sum of the left("
               << left_bit_size << ") and right(" << right_bit_size
               << ") field sizes";
    return;
  }

  // The ParserDecoder deals with concatenated select fields by splitting them,
  // as if select(field1 ++ field2) was coded as select(field1, field2).  This
  // means that all case key set values, which should currently be a single
  // value per case, must split into two values according to field widths.
  decoded_select->add_selector_fields(left->member.name);
  decoded_select->add_selector_fields(right->member.name);
  for (auto& decoded_case : *decoded_select->mutable_cases()) {
    if (decoded_case.is_default())
      continue;
    if (decoded_case.keyset_values_size() != 1) {
      LOG(ERROR) << "Compiler bug: expected keyset values of size 1 in select "
                 << "expression with concat operator, found keyset size "
                 << decoded_case.keyset_values_size();
      return;
    }
    ParserKeySetValue* field1_keyset = decoded_case.mutable_keyset_values(0);
    ParserKeySetValue* field2_keyset = decoded_case.add_keyset_values();
    uint64 value_left = field1_keyset->constant().value() >> right_bit_size;
    uint64 value_right =
        field1_keyset->constant().value() & ((1 << right_bit_size) - 1);
    field1_keyset->mutable_constant()->set_value(value_left);
    field2_keyset->mutable_constant()->set_value(value_right);
    uint64 mask_left = field1_keyset->constant().mask() >> right_bit_size;
    uint64 mask_right =
        field1_keyset->constant().mask() & ((1 << right_bit_size) - 1);
    field1_keyset->mutable_constant()->set_mask(mask_left);
    field2_keyset->mutable_constant()->set_mask(mask_right);
  }
}

std::string ParserDecoder::ExtractHeaderType(
    const IR::MethodCallStatement& statement) {
  std::string header_type_name;  // Returned value.

  // To be an "extract" statement, the input value must first resolve to
  // a P4 ExternMethod.  This code is derived from the bmv2 backend
  // implementation in //p4lang_p4c/backends/bmv2's
  // JsonConverter::convertParserStatement method.
  auto method_call = statement.methodCall;
  auto method_instance =
      P4::MethodInstance::resolve(method_call, ref_map_, type_map_);
  if (method_instance->is<P4::ExternMethod>()) {
    P4::P4CoreLibrary& corelib = P4::P4CoreLibrary::instance;
    auto extern_method = method_instance->to<P4::ExternMethod>();

    // Extract methods must have one argument that identifies the header type.
    if (extern_method->method->name.name == corelib.packetIn.extract.name) {
      if (method_call->arguments->size() == 1) {
        auto arg = method_call->arguments->at(0);
        auto arg_type = type_map_->getType(arg, true);
        if (!arg_type->is<IR::Type_Header>()) {
          // TODO(unknown): Should the compiler catch this earlier?
          LOG(ERROR) << "extract expects arg type to be Type_Header";
          return header_type_name;
        }
        header_type_name = std::string(
            arg_type->to<IR::Type_Header>()->name.toString());
      } else {
        LOG(WARNING) << "Unexpected argument count "
                     << method_call->arguments->size() << " in extract";
      }
    } else {
      LOG(WARNING) << "MethodCallStatement is not an extract statement";
    }
  } else {
    LOG(WARNING) << "MethodCallStatement is not an ExternMethod";
  }

  return header_type_name;
}

}  // namespace p4c_backends
}  // namespace stratum
