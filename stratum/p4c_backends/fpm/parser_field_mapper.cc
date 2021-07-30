// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file implements the ParserFieldMapper class in the Stratum p4c backend.

#include "stratum/p4c_backends/fpm/parser_field_mapper.h"

#include <tuple>
#include <utility>
#include <vector>

#include "stratum/glue/logging.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "absl/strings/match.h"
#include "stratum/glue/gtl/map_util.h"

namespace stratum {
namespace p4c_backends {

ParserFieldMapper::ParserFieldMapper(TableMapGenerator* table_mapper)
    : table_mapper_(ABSL_DIE_IF_NULL(table_mapper)),
      done_(false) {}

bool ParserFieldMapper::MapFields(
    const ParserMap& ir_parser_field_map,
    const FieldDecoder::DecodedHeaderFieldMap& header_field_map,
    const ParserMap& target_parser_field_map) {
  if (!VerifyInputs(ir_parser_field_map, header_field_map,
      target_parser_field_map)) {
    return false;
  }

  working_field_map_ = ir_parser_field_map;
  done_ = true;
  bool status = RunPass1(header_field_map) && RunPass2(target_parser_field_map);
  if (status) RunPass3();
  return status;
}

bool ParserFieldMapper::VerifyInputs(
    const ParserMap& ir_parser_field_map,
    const FieldDecoder::DecodedHeaderFieldMap& header_field_map,
    const ParserMap& target_parser_field_map) {
  if (done_) {
    LOG(ERROR) << "ParserFieldMapper can only map one set of inputs";
    return false;
  }

  // The ir_parser_field_map and header_field_map are produced by earlier
  // stages of p4c, and they are generally assumed to be valid, with
  // DCHECK/CHECK macros sprinkled elsewhere through the code for confirmation.
  bool valid = true;
  if (ir_parser_field_map.parser_states().empty()) {
    LOG(ERROR) << "Unable to map header fields from empty P4 ParserMap";
    valid = false;
  }

  if (header_field_map.empty()) {
    LOG(ERROR) << "Unable to map header fields from empty decoded fields set";
    valid = false;
  }

  // The target_parser_field_map comes from a command-line input file, so it
  // needs to be more thoroughly checked for sanity and consistency.
  valid = VerifyTargetParserMap(target_parser_field_map) && valid;

  return valid;
}

// Doing an extra upfront detailed pass through the target_parser_field_map
// adds slightly more p4c backend overhead (vs. doing checks as the states
// are processed in pass2), but it makes the pass2 code more readable overall.
bool ParserFieldMapper::VerifyTargetParserMap(
    const ParserMap& target_parser_field_map) {
  bool valid = true;
  if (!target_parser_field_map.parser_states().empty()) {
    target_start_name_.clear();
    for (const auto& iter : target_parser_field_map.parser_states()) {
      // There must be exactly one start state.
      const ParserState& target_state = iter.second;
      if (target_state.reserved_state() == ParserState::P4_PARSER_STATE_START) {
        if (target_start_name_.empty()) {
          // Store the start state so another search isn't needed later.
          VLOG(1) << target_state.name() << " is the target start state";
          target_start_name_ = target_state.name();
        } else {
          LOG(ERROR) << "Target parser map has multiple start states "
                     << target_start_name_ << " and " << target_state.name();
          valid = false;
        }
      }

      // All states must extract a header with at least one field.
      if (!target_state.has_extracted_header()) {
        LOG(ERROR) << "Target state " << target_state.name() << " is invalid; "
                   << "missing extracted header";
        valid = false;
        continue;
      }
      if (!target_state.extracted_header().fields_size()) {
        LOG(ERROR) << "Target state " << target_state.name() << " is invalid; "
                      "its extracted header does not specify any fields";
        valid = false;
        continue;
      }

      // Field offsets within the header must be monotonically increasing.
      int64_t current_field_offset = -1;
      for (const auto& field : target_state.extracted_header().fields()) {
        int64_t field_offset = int64_t{field.bit_offset()};
        if (field_offset <= current_field_offset) {
          LOG(ERROR) << "Fields in target state " << target_state.name()
                     << " must be in order of increasing bit offset";
          valid = false;
        }
        current_field_offset = field.bit_offset();
      }

      // If a select expression is present, it must use exactly one field.
      if (target_state.transition().has_select()) {
        const auto& select = target_state.transition().select();
        if (select.selector_types_size() != 1) {
          LOG(ERROR) << "Target state " << target_state.name() << " select "
                     << "expression must contain exactly one selector type";
          valid = false;
        }

        // The next state for each select case must exist in the parser map,
        // and each case must have exactly one key value.
        for (const auto& select_case : select.cases()) {
          if (select_case.is_default())
            continue;
          if (target_parser_field_map.parser_states().find(
              select_case.next_state()) ==
              target_parser_field_map.parser_states().end()) {
            LOG(ERROR) << "Target state " << target_state.name() << " next "
                       << "state " << select_case.next_state()
                       << " does not exist";
            valid = false;
          }
          if (select_case.keyset_values_size() != 1) {
            LOG(ERROR) << "Target state " << target_state.name() << " select "
                       << "cases must have exactly one key value";
            valid = false;
          }
        }
      }
    }

    if (target_start_name_.empty()) {
      LOG(ERROR) << "Target parser map has no start state";
      valid = false;
    }

  } else {
    // An empty parser map for the target generates a warning, but it is not
    // considered to be invalid.  There are two cases where this is normal:
    // 1) A target that wants to rely exclusively on field type annotations.
    // 2) Unit tests that verify the intermediate state between pass1 and pass2.
    LOG(WARNING) << "Target parser has no states - "
                 << "skipping  header field mapping";
  }

  return valid;
}

// For each parser state that extracts a header, the header type should have
// an entry in the input header_field_map.
bool ParserFieldMapper::RunPass1(
    const FieldDecoder::DecodedHeaderFieldMap& header_field_map) {
  VLOG(2) << __PRETTY_FUNCTION__;

  for (auto& state_iter : *working_field_map_.mutable_parser_states()) {
    ParserState* state = &state_iter.second;
    if (state->has_extracted_header()) {
      const auto header_type_iter =
          header_field_map.find(state->extracted_header().name());
      if (header_type_iter == header_field_map.end()) {
        LOG(ERROR) << "Unable to find header field map entry for extracted "
                   << "header type " << state->extracted_header().name()
                   << " in parser state " << state->name();
        return false;
      }

      // The loop below accumulates and records the field offsets as it adds
      // each field to the working state data for the extracted header.
      VLOG(2) << "Adding fields to header " << state->extracted_header().name()
              << " in parser state " << state->name();
      int offset = 0;
      for (const auto& field_iter : header_type_iter->second) {
        ParserExtractField* out_field =
            state->mutable_extracted_header()->add_fields();
        *out_field = field_iter;
        out_field->set_bit_offset(offset);
        offset += out_field->bit_width();
      }
    }
  }

  VLOG(2) << "Pass1 output " << working_field_map_.DebugString();
  return true;
}

// Pass2 aligns the target start state with the corresponding P4 start state.
// It then expects to follow the select expression transitions in each state
// machine and find the same header types extracted by each next state pair.
bool ParserFieldMapper::RunPass2(const ParserMap& target_parser_field_map) {
  if (!ProcessStartState(target_parser_field_map))
    return false;

  // The pass2_work_queue_ collects new pairs of states to process as
  // ProcessStartState and ProcessStatePair identify parser transitions.
  while (!pass2_work_queue_.empty()) {
    const WorkQueueEntry& q_entry = pass2_work_queue_.front();
    VLOG(2) << "Comparing target parser state " << q_entry.target_state_name
            << " to P4 parser state " << q_entry.p4_state_name;
    const auto& target_iter =
        target_parser_field_map.parser_states().find(q_entry.target_state_name);
    DCHECK(target_iter != target_parser_field_map.parser_states().end());
    const auto& p4_iter =
        working_field_map_.parser_states().find(q_entry.p4_state_name);
    DCHECK(p4_iter != working_field_map_.parser_states().end());
    bool is_tunnel_entry = q_entry.is_tunnel_entry;
    pass2_work_queue_.pop_front();
    ProcessStatePair(target_iter->second, p4_iter->second, is_tunnel_entry);
  }

  return true;
}

// Pass3 handles fields where the parser does not extract the header, but it
// does extract some other header with the same type.  For example, ERSPAN
// packets use Ethernet, IP, and GRE headers during egress, but they are not
// seen by the ingress parser stage.  This pass finds the table mapping for
// fields such as "hdr.erspan_ethernet.dst_addr".
void ParserFieldMapper::RunPass3() {
  // This loop looks for P4 table map field descriptors that still have
  // an unspecified type.  It maps them to an entry in pass3_fields_map_,
  // if one exists.
  for (const auto& iter : table_mapper_->generated_map().table_map()) {
    if (iter.second.has_field_descriptor()) {
      const auto& field_descriptor = iter.second.field_descriptor();
      if (!IsFieldTypeUnspecified(field_descriptor)) continue;
      const MappedFieldData* mapped_field =
          gtl::FindOrNull(pass3_fields_map_, iter.first);
      if (mapped_field == nullptr) continue;
      table_mapper_->SetFieldAttributes(
          mapped_field->name, mapped_field->field_type,
          mapped_field->header_type, mapped_field->bit_offset,
          mapped_field->bit_width);
    }
  }
}

// ProcessStartState handles the start state in the target parser map.  Since
// this state is not reached by a parser select expression with a protocol
// dependent case value, the header type needs to be deduced by comparing
// field offsets and widths to each state in the P4 parser.  Due to
// implementation differences, it is unlikely that the P4 parser map and the
// target parser map will have equivalent start states. ProcessStartState
// assumes that VerifyInputs has previously verified the validity of
// target_parser_field_map.
bool ParserFieldMapper::ProcessStartState(
    const ParserMap& target_parser_field_map) {
  if (target_parser_field_map.parser_states().empty()) {
    return true;  // Nothing else to do.
  }
  DCHECK(!target_start_name_.empty());
  const auto& start_iter =
      target_parser_field_map.parser_states().find(target_start_name_);
  DCHECK(start_iter != target_parser_field_map.parser_states().end());
  const ParserState& target_start_state = start_iter->second;

  int p4_header_matches = 0;
  for (const auto& p4_iter : working_field_map_.parser_states()) {
    const ParserState& p4_state = p4_iter.second;
    if (!p4_state.has_extracted_header())
      continue;

    // It is possible for the target start state to match multiple P4 parser
    // states.  It is unlikely given that the Ethernet header is generally
    // the target start state, and ambiguity can occur only if some other
    // protocol has header fields in a 48/48/16 bit pattern.  The start state
    // does not support subfield matching.
    if (MatchTargetAndP4Fields(target_start_state, p4_state, false) == 0) {
      ++p4_header_matches;
      if (p4_header_matches > 1) {
        LOG(WARNING) << "Target start state " << target_start_state.name()
                     << " ambiguously matches extracted fields in multiple"
                     << " P4 parser states";
        return false;
      }
      AddVisitedHeaders(p4_state.extracted_header());
      visited_p4_states_.insert(p4_state.name());
      SelectTransitions(target_start_state, p4_state);
    }
  }

  if (!p4_header_matches) {
    LOG(WARNING) << "Target parser start state fields do not match any "
                 << "states in the P4 program parser";
    return false;
  }

  return true;
}

bool ParserFieldMapper::ProcessStatePair(
    const ParserState& target_state, const ParserState& p4_state,
    bool in_tunnel) {
  // If a P4 state doesn't extract a header, it's probably one of the built
  // in states that terminates the sequence.  If the header has already been
  // processed in another transition sequence, no more work is needed.
  // TODO(unknown): Does the P4 parser allow intermediate states that don't
  // extract any header?  The correct behavior in that case would be to
  // check for a non-built-in state and queue another work entry with the same
  // target_state and the p4_state's next state.
  DCHECK(target_state.has_extracted_header());
  if (!p4_state.has_extracted_header()) {
    return true;
  }
  if (visited_p4_states_.find(p4_state.name()) != visited_p4_states_.end()) {
    return true;
  }

  int match_status = MatchTargetAndP4Fields(target_state, p4_state, in_tunnel);
  if (match_status != 0) {
    match_status = MatchP4FieldsAndTargetSubFields(
        target_state, p4_state, in_tunnel, match_status - 1);
  }
  if (match_status == 0) {
    // If MatchP4FieldsAndTargetSubFields aligned the headers by inserting
    // subfields, it worked on a mutated copy of target_state.  It is OK to
    // pass the original state here, because SelectTransitions is not concerned
    // with the subfields.  None of the P4 programs selects based on
    // a field that ends up being produced by subfield insertion.
    SelectTransitions(target_state, p4_state);
    AddVisitedHeaders(p4_state.extracted_header());
    visited_p4_states_.insert(p4_state.name());
  } else {
    // The same protocol type/ID values led to this pair of states, so a
    // mismatch between extracted fields is not expected.  This most likely
    // indicates a bug in the p4c code or a bug in the target parser
    // input data.
    LOG(ERROR) << "Compiler bug: Unable to match headers between target state "
               << target_state.name() << " and P4 parser state "
               << p4_state.name();
    return false;
  }

  return true;
}

int ParserFieldMapper::MatchTargetAndP4Fields(
    const ParserState& target_state, const ParserState& p4_state,
    bool in_tunnel) {
  DCHECK(target_state.has_extracted_header());
  const auto& target_header = target_state.extracted_header();
  DCHECK(target_header.fields_size());
  DCHECK(p4_state.has_extracted_header());
  const ParserExtractHeader& p4_header = p4_state.extracted_header();
  DCHECK(p4_header.fields_size()) << "Compiler bug: P4 state "
                                  << p4_state.name() << " extracts "
                                  << "an empty set of fields";

  // The header_visited flag means that the extracted header's field types
  // have already been mapped by another parser state, but the header still
  // needs to be matched field-by-field with the target.  This can occur for
  // header stacks and encap/decap header adjustments.  When a state extracts
  // a header stack, it should be sufficient to check whether the first
  // member of the stack has been visited.
  bool header_visited = false;
  if (visited_p4_header_names_.find(p4_header.header_paths(0)) !=
      visited_p4_header_names_.end()) {
    header_visited = true;
  }

  // The mapped_fields container accumulates MappedFieldData for individual
  // matching fields.  If the whole header matches, the container entries are
  // written to the output table mapper or deferred to Pass3 before returning
  // to the caller.
  std::vector<MappedFieldData> mapped_fields;
  int p4_index = 0;
  for (int target_index = 0;
       target_index < target_header.fields_size(); ++target_index) {
    bool field_match = false;

    // The target does not need to specify a complete set of fields in a
    // header; it only needs those that participate in forwarding pipeline
    // decisions.  The loop below skips P4 parser fields that appear between
    // non-contiguous target parser fields.
    while (p4_index < p4_header.fields_size()) {
      if (p4_header.fields(p4_index).bit_offset() ==
          target_header.fields(target_index).bit_offset() &&
          p4_header.fields(p4_index).bit_width() ==
          target_header.fields(target_index).bit_width()) {
        field_match = true;
        if (!header_visited) {
          for (const auto& field_name :
               p4_header.fields(p4_index).full_field_names()) {
            mapped_fields.push_back(MappedFieldData(
                field_name, target_header.fields(target_index).type(),
                target_header.header_type(),
                p4_header.fields(p4_index).bit_offset(),
                p4_header.fields(p4_index).bit_width()));
          }
        }
        ++p4_index;
        break;
      }
      ++p4_index;
    }
    if (!field_match) {
      return target_index + 1;
    }
  }

  // Upon arrival here, the two headers match, and the P4 table map can be
  // updated.  Table map updates here are only for header fields directly
  // extracted by the input parser state.  This avoids anomalies that can
  // occur by processing an inner encap field in the parser state for the
  // outer header.  Non-extracted fields move to the pass3_fields_map_ map
  // for processing in Pass3 if they are still unresolved.
  int32 header_depth = 0;
  if (!mapped_fields.empty() || header_visited) {
    for (int i = 0; i < p4_header.header_paths().size(); ++i) {
      const auto& header = p4_header.header_paths().Get(i);
      if (in_tunnel) {
        header_depth = 1;
      } else if (IsHeaderArrayLast(header)) {
        header_depth = i - 1;
      } else {
        header_depth = i;
      }
      table_mapper_->SetHeaderAttributes(
          header, target_header.header_type(), header_depth);
    }
  }
  for (const auto& mapped_field : mapped_fields) {
    if (!IsFieldExtracted(p4_header, mapped_field.name)) {
      pass3_fields_map_.insert(std::make_pair(mapped_field.name, mapped_field));
      continue;
    }
    table_mapper_->SetFieldAttributes(
        mapped_field.name, mapped_field.field_type, mapped_field.header_type,
        mapped_field.bit_offset, mapped_field.bit_width);
    VLOG_IF(1, in_tunnel) << mapped_field.name << " is tunneled";
  }

  return 0;
}

int ParserFieldMapper::MatchP4FieldsAndTargetSubFields(
    const ParserState& target_state, const ParserState& p4_state,
    bool in_tunnel, int mismatch_index) {
  const ParserExtractHeader& old_header = target_state.extracted_header();
  if (mismatch_index >= old_header.fields_size())
    return 1;
  if (old_header.fields(mismatch_index).subfield_set_name().empty())
    return 1;
  ParserState new_target_state = target_state;  // Mutable copy.
  ParserExtractHeader* new_header = new_target_state.mutable_extracted_header();


  // When the loop below begins, next_subfield_index refers to the first
  // field that may benefit from attempting to match against subfields.
  // Multiple passes through the loop substitute any additional subfields
  // if the match succeeds with the new subfields, but fails at a later
  // field in the header.  Each pass through the loop inserts replacement
  // subfields at the end of the new header, copies any remaining fields
  // to later positions in the header, and then calls DeleteSubrange to
  // remove fields that are no longer relevant.  If this is the original
  // header on input:
  //  F1, F2, F3, with next_subfield_index at F2.
  // then after InsertSubFields executes, new_header has the following fields:
  //  F1, F2, F3, SF1, SF2, where SF1 and SF2 are F2's subfields.
  // The inner loop reorders a copy of F3 after the new subfields:
  //  F1, F2, F3, SF1, SF2, F3.
  // And then DeleteSubRange removes the old F2 and F3 fields to leave
  // new_header in this state for another field match attempt after subfield
  // changes:
  //  F1, SF1, SF2, F3.
  int next_subfield_index = mismatch_index;
  while (next_subfield_index < new_header->fields_size()) {
    int subrange_size = new_header->fields_size() - next_subfield_index;
    int new_subfields = InsertSubFields(next_subfield_index, new_header);
    if (new_subfields == 0) return next_subfield_index + 1;
    for (int i = 1; i < subrange_size; ++i) {
      *new_header->add_fields() = new_header->fields(i + next_subfield_index);
    }
    new_header->mutable_fields()->DeleteSubrange(
        next_subfield_index, subrange_size);
    next_subfield_index += new_subfields;
    int match_status = MatchTargetAndP4Fields(
        new_target_state, p4_state, in_tunnel);

    // Multiple outcomes can happen in MatchTargetAndP4Fields:
    //  1) The match succeeds after the most recent subfield insertion.
    //  2) The match fails at the same place, so the subfield does not help.
    //  3) The match fails, but the failure occurs after the new subfields.
    //     a) The failure moved to a field with no subfield possibilities.
    //     b) The failure moved to a field with its own subfields to try.
    if (match_status == 0) return 0;  // Outcome #1 - Success.
    int failed_index = match_status - 1;
    if (failed_index < next_subfield_index) {
      return match_status;  // Outcome #2 - Failure.
    }

    DCHECK_LT(failed_index, new_header->fields_size());
    if (new_header->fields(failed_index).subfield_set_name().empty()) {
      return match_status;  // Outcome #3a - Failure.
    }

    // Outcome #3b - retry with another subfield insertion at failed_index.
    next_subfield_index = failed_index;
  }

  return 1;
}

void ParserFieldMapper::SelectTransitions(const ParserState& target_state,
                                          const ParserState& p4_state) {
  if (!target_state.transition().has_select() &&
      !p4_state.transition().has_select()) {
    // If neither state has a select-based transition, look for any meaningful
    // unconditional transitions.
    ProcessUnconditionalTransition(target_state, p4_state);
    return;
  }

  if (!target_state.transition().has_select()) {
    // If the target state has no select expression, then it is not interested
    // in parsing any more headers beyond this state.  If the P4 program still
    // has states that extract headers beyond this point, those fields need to
    // be covered by annotations.
    return;
  }
  DCHECK_EQ(1, target_state.transition().select().selector_types_size());

  if (!p4_state.transition().has_select()) {
    // If the P4 program is not selecting any more transitions, it means that
    // the P4 parser is using only a subset of what the target parser provides.
    return;
  }

  ParserSelectExpression normalized_p4_select =
      NormalizeSelect(p4_state.transition().select());
  VLOG(3) << "Normalized select: " << normalized_p4_select.DebugString();
  const auto& target_select = target_state.transition().select();

  // For each case in the target state, search for a matching key value in
  // the P4 state cases.  When a match occurs, both states should be advancing
  // to a state that extracts the same header, which generates a work queue
  // entry for further processing.
  // TODO(unknown): What can be done here to confirm that both input states
  // select on the same field?
  for (const auto& target_case : target_select.cases()) {
    if (target_case.is_default())
      continue;
    DCHECK_EQ(1, target_case.keyset_values_size());

    for (const auto& p4_case : normalized_p4_select.cases()) {
      if (p4_case.is_default())
        continue;
      DCHECK_EQ(1, p4_case.keyset_values_size())
          << "Invalid keyset values in normalized P4 select expression";
      // The keyset mask is not important for this comparison.
      // TODO(unknown): Could this keyset ever be a value set?
      if (p4_case.keyset_values(0).constant().value() ==
          target_case.keyset_values(0).constant().value()) {
        VLOG(1) << "Adding field map work queue entry for "
                << target_case.next_state() << ", " << p4_case.next_state();
        pass2_work_queue_.emplace_back(
            target_case.next_state(), p4_case.next_state(),
            target_case.is_tunnel_entry());
        break;
      }
    }
  }
}

// Filters out unnecessary attributes for P4 Parser select expressions.
ParserSelectExpression ParserFieldMapper::NormalizeSelect(
    const ParserSelectExpression& select) {
  DCHECK_LE(1, select.selector_fields_size()) << "Fatal compiler bug: missing "
      << "fields in select expression " << select.ShortDebugString();
  if (select.selector_fields_size() == 1) {
    return select;  // No normalization required.
  }

  // TODO(unknown): The only current use case for multiple select fields is
  // in the tor.p4 IPv4 parser state, where a non-zero fragment offset is used
  // to avoid further transitions on the IP protocol.  This usage is not
  // important here, so the select case gets normalized to a single entry,
  // which should indicate the next states for each IP protocol type.  The
  // logic below is specific to this case.
  // TODO(unknown): In P4_16, the select expression uses a concat operator
  // to combine select fields into one value.
  ParserSelectExpression normalized;
  int select_index = select.selector_fields_size() - 1;
  normalized.add_selector_fields(select.selector_fields(select_index));
  for (const auto& select_case : select.cases()) {
    auto new_case = normalized.add_cases();
    new_case->set_is_default(select_case.is_default());
    new_case->set_next_state(select_case.next_state());
    if (!select_case.is_default()) {
      DCHECK_LT(select_index, select_case.keyset_values_size())
          << "Parser select expression has invalid number of keyset values "
          << select.ShortDebugString();
      *new_case->add_keyset_values() = select_case.keyset_values(select_index);
    }
  }
  return normalized;
}

void ParserFieldMapper::ProcessUnconditionalTransition(
    const ParserState& target_state, const ParserState& p4_state) {
  if (IsParserEndState(p4_state)) {
    VLOG(2) << "State " << p4_state.name() << " terminates transitions";
    return;
  }
  if (IsParserEndState(target_state)) {
    VLOG(2) << "State " << target_state.name() << " terminates transitions";
    return;
  }
  VLOG(1) << "Adding field map work queue entry for unconditional transition "
          << "to " << target_state.transition().next_state()
          << ", " << p4_state.transition().next_state();
  pass2_work_queue_.emplace_back(target_state.transition().next_state(),
                                 p4_state.transition().next_state(), false);
}

int ParserFieldMapper::InsertSubFields(
    int sub_index, ParserExtractHeader* new_header) {
  const std::string& subfield_set_name =
      new_header->fields(sub_index).subfield_set_name();
  if (subfield_set_name.empty()) return 0;
  for (const auto& subfield_set : new_header->subfield_sets()) {
    if (subfield_set_name == subfield_set.name()) {
      for (const auto& field : subfield_set.fields()) {
        *new_header->add_fields() = field;
      }
      return subfield_set.fields_size();
    }
  }

  LOG(ERROR) << "Unable to find subfield set " << subfield_set_name;
  return 0;
}

void ParserFieldMapper::AddVisitedHeaders(
    const ParserExtractHeader& extracted_header) {
  // Multiple paths will be present for extracted header stacks.
  for (const auto& header : extracted_header.header_paths()) {
    visited_p4_header_names_.insert(header);
  }
}

// The FieldDecoder::DecodedHeaderFieldMap input to ParserFieldMapper gives
// all the field names associated with a given P4 header type.  Sometimes,
// ParserFieldMapper needs to limit the fields it processes to those extracted
// by a given parser state.  For example, "ipv4_base_t" is the type for both
// inner and outer headers in IPv4 tunnels, but ParserFieldMapper wants to
// avoid processing inner header fields in the parser state for the outer
// header, and vice versa.
bool ParserFieldMapper::IsFieldExtracted(const ParserExtractHeader& p4_header,
                                         const std::string& field_name) {
  for (const auto& header_path : p4_header.header_paths()) {
    if (absl::StartsWith(field_name, header_path)) return true;
  }
  return false;
}

}  // namespace p4c_backends
}  // namespace stratum
