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

// This file implements the ParserValueSetMapper.

#include "stratum/p4c_backends/fpm/parser_value_set_mapper.h"

#include "base/logging.h"
#include "stratum/hal/lib/p4/p4_table_map.host.pb.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "absl/debugging/leak_check.h"
#include "util/gtl/map_util.h"

namespace stratum {
namespace p4c_backends {

ParserValueSetMapper::ParserValueSetMapper(
    const ParserMap& p4_parser_map, const hal::P4InfoManager& p4_info_manager,
    TableMapGenerator* table_mapper)
    : p4_parser_map_(p4_parser_map),
      p4_info_manager_(p4_info_manager),
      table_mapper_(ABSL_DIE_IF_NULL(table_mapper)),
      visiting_state_(nullptr) {
}

bool ParserValueSetMapper::MapValueSets(const IR::P4Parser& p4_parser) {
  FindValueSetTransitions();
  absl::LeakCheckDisabler disable_ir_parser_leak_checks;
  p4_parser.apply(*this);

  return true;
}

// If the ir_parser_state has a matching entry in value_set_states_, it was
// selected by a value-set transition case, and the return is true to visit
// deeper nodes under the state.  The child nodes of all other parser states
// are uninteresting.
bool ParserValueSetMapper::preorder(const IR::ParserState* ir_parser_state) {
  const std::string ir_state_name = ir_parser_state->controlPlaneName().c_str();
  visiting_state_ = gtl::FindOrNull(value_set_states_, ir_state_name);
  if (visiting_state_ == nullptr) {
    return false;
  }

  return true;
}

// Assignments are the only nodes of interest under an IR::ParserState.
bool ParserValueSetMapper::preorder(const IR::AssignmentStatement* statement) {
  if (visiting_state_ == nullptr) return false;
  if (!ProcessAssignmentRight(*statement->right)) return false;
  if (!ProcessAssignmentLeft(*statement->left)) return false;
  table_mapper_->SetFieldValueSet(statement->left->toString().c_str(),
                                  visiting_state_->value_set_name,
                                  visiting_state_->header_type);
  return false;
}

void ParserValueSetMapper::FindValueSetTransitions() {
  for (const auto& iter : p4_parser_map_.parser_states()) {
    const ParserState& state = iter.second;
    if (!state.transition().has_select()) continue;
    const ParserSelectExpression& select = state.transition().select();

    for (const ParserSelectCase& select_case : select.cases()) {
      for (const ParserKeySetValue& keyset_value :
           select_case.keyset_values()) {
        if (!keyset_value.has_value_set()) continue;
        value_set_states_.emplace(
            select_case.next_state(),
            ValueSetState(keyset_value.value_set().value_set_name()));
      }
    }
  }
}

// Neither ProcessAssignmentRight nor ProcessAssignmentLeft does any detailed
// processing of the input expression.  They both take the approach that the
// expression's string representation will match a field descriptor if it
// is something that should be processed in this context.  More complex or
// unsupported expressions, such as "field1 + field2", will never match a
// field descriptor in the generated table map.
bool ParserValueSetMapper::ProcessAssignmentRight(const IR::Expression& right) {
  const hal::P4TableMapValue* rhs_field_entry = gtl::FindOrNull(
      table_mapper_->generated_map().table_map(), right.toString().c_str());
  if (rhs_field_entry == nullptr) return false;
  if (!rhs_field_entry->has_field_descriptor()) return false;
  const hal::P4FieldDescriptor& rhs_descriptor =
      rhs_field_entry->field_descriptor();
  if (IsFieldTypeUnspecified(rhs_descriptor)) return false;
  if (rhs_descriptor.header_type() == P4_HEADER_UNKNOWN)
      return false;
  visiting_state_->header_type = rhs_descriptor.header_type();
  return true;
}

bool ParserValueSetMapper::ProcessAssignmentLeft(const IR::Expression& left) {
  const hal::P4TableMapValue* lhs_field_entry = gtl::FindOrNull(
      table_mapper_->generated_map().table_map(), left.toString().c_str());
  if (lhs_field_entry == nullptr) return false;
  if (!lhs_field_entry->has_field_descriptor()) return false;
  const hal::P4FieldDescriptor& lhs_descriptor =
      lhs_field_entry->field_descriptor();
  if (!lhs_descriptor.is_local_metadata()) return false;
  if (!IsFieldTypeUnspecified(lhs_descriptor)) return false;
  return true;
}

}  // namespace p4c_backends
}  // namespace stratum
