// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// The ParserFieldMapper combines three sets of inputs to generate detailed
// P4 field type mapping data:
//  1) The ParserMap from the ParserDecoder's IR processing.
//  2) The DecodedHeaderFieldMap from the FieldDecoder's IR processing.
//  3) A ParserMap that defines the parser operation for the target platform.
// The ParserFieldMapper's role is to translate P4 fields into well known
// hal.P4FieldType values for the table map in the P4PipelineConfig.
// It does not validate the overall P4 parser behavior relative to the target
// hardware.

#ifndef STRATUM_P4C_BACKENDS_FPM_PARSER_FIELD_MAPPER_H_
#define STRATUM_P4C_BACKENDS_FPM_PARSER_FIELD_MAPPER_H_

#include <deque>
#include <set>
#include <string>
#include <unordered_map>

#include "stratum/p4c_backends/fpm/field_decoder.h"
#include "stratum/p4c_backends/fpm/parser_map.pb.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "absl/container/node_hash_map.h"

namespace stratum {
namespace p4c_backends {

// ParserFieldMapper is intended to be invoked once via its MapFields method
// to produce field type data in the Stratum p4c table map output.  It
// combines the outputs from the ParserDecoder and FieldDecoder with a
// protobuf specifying the parser behavior of the target.
class ParserFieldMapper {
 public:
  // The caller retains ownership of the injected table_mapper.
  explicit ParserFieldMapper(TableMapGenerator* table_mapper);
  virtual ~ParserFieldMapper() {}

  // MapFields processes its inputs and updates the table map output.  It
  // can only be called once per ParserFieldMapper instance.  MapFields fails
  // and returns false when called twice or when one of its inputs is invalid.
  bool MapFields(const ParserMap& ir_parser_field_map,
                 const FieldDecoder::DecodedHeaderFieldMap& header_field_map,
                 const ParserMap& target_parser_field_map);

  // ParserFieldMapper is neither copyable nor movable.
  ParserFieldMapper(const ParserFieldMapper&) = delete;
  ParserFieldMapper& operator=(const ParserFieldMapper&) = delete;

 private:
  // This struct represents one entry in the ParserFieldMapper's internal
  // work queue.  Each entry describes a state transition in the parser.
  // The target_state_name identifies the next state in the target ParserMap,
  // and the p4_state_name identifies the P4 IR ParserMap state that is
  // expected to have matching fields.  The is_tunnel_entry flag indicates
  // whether the transition expects a tunneled (inner) packet header.
  struct WorkQueueEntry {
    WorkQueueEntry(const std::string& target_state_name,
                   const std::string& p4_state_name, bool is_tunnel_entry)
        : target_state_name(target_state_name),
          p4_state_name(p4_state_name),
          is_tunnel_entry(is_tunnel_entry) {
    }

    const std::string target_state_name;
    const std::string p4_state_name;
    const bool is_tunnel_entry;
  };

  // The MappedFieldData defines the essential data that ParserFieldMapper
  // stores in table_mapper's FieldDescriptor entries.  It provides a
  // provisional repository for fields derived from parser states, but not
  // yet ready to output to the table_mapper.
  struct MappedFieldData {
    MappedFieldData(const std::string& name, P4FieldType field_type,
                    P4HeaderType header_type, uint32 bit_offset,
                    uint32 bit_width)
        : name(name),
          field_type(field_type),
          header_type(header_type),
          bit_offset(bit_offset),
          bit_width(bit_width) {}

    std::string name;
    P4FieldType field_type;
    P4HeaderType header_type;
    uint32 bit_offset;
    uint32 bit_width;
  };

  // These methods verify the inputs to MapFields, returning true when all
  // inputs are valid, false if any input is invalid.
  bool VerifyInputs(const ParserMap& ir_parser_field_map,
                    const FieldDecoder::DecodedHeaderFieldMap& header_field_map,
                    const ParserMap& target_parser_field_map);
  bool VerifyTargetParserMap(const ParserMap& target_parser_field_map);

  // Internally, MapFields operates in three passes.  The first pass combines
  // the FieldDecoder and ParserDecoder outputs into the private
  // working_field_map_ member.  The second pass takes working_field_map_
  // and applies target_parser_field_map to generate field mapping data,
  // giving priority to mapping fields that are directly extracted by the
  // parser state machine.  The third pass does additional field mapping for
  // fields in P4 header types that were processed by the parser, but not
  // directly extracted by a parser state.
  bool RunPass1(const FieldDecoder::DecodedHeaderFieldMap& header_field_map);
  bool RunPass2(const ParserMap& target_parser_field_map);
  void RunPass3();

  // ProcessStartState processes the "start" state in the
  // target_parser_field_map at the beginning of MapField's second pass.
  bool ProcessStartState(const ParserMap& target_parser_field_map);

  // ProcessStatePair handles all state transitions after the start state.
  // The input states have the property that the previous target state
  // and P4 parser state used the same select key value to transition to these
  // states.  Thus, ProcessStatePair expects both states to extract the same
  // header header type.  For example, both states selected by ethertype 0x8100
  // expect to find a VLAN header.
  bool ProcessStatePair(const ParserState& target_state,
                        const ParserState& p4_state, bool in_tunnel);

  // Compares the extracted header fields of the two input states to determine
  // whether they are equivalent.  If the states are equivalent, the return
  // value is 0.  If the states do not match, the return value is a positive
  // integer, which equals one plus the index of the target_state's extracted
  // header field that failed to match.
  int MatchTargetAndP4Fields(const ParserState& target_state,
                             const ParserState& p4_state, bool in_tunnel);

  // When MatchTargetAndP4Fields fails on the original state from the target
  // parser map, this method attempts additional matches based on any sub-field
  // definitions in the target_state.  The mismatch_index is the field in the
  // target state that failed the original match.  The result is 0 if the
  // sub-field match succeeds, non-zero otherwise.
  int MatchP4FieldsAndTargetSubFields(const ParserState& target_state,
                                      const ParserState& p4_state,
                                      bool in_tunnel, int mismatch_index);

  // Looks through the transitions in the input states for matching select
  // cases.  Cases with equivalent keys should mean that both states are
  // advancing to the next state to process the same header.
  void SelectTransitions(const ParserState& target_state,
                         const ParserState& p4_state);

  // Given a select expression from a P4 parser state, this method filters
  // out any attributes that are not relevant to the field mapping procedures.
  ParserSelectExpression NormalizeSelect(const ParserSelectExpression& select);

  // Processes an unconditional, non-select-based transition out of the
  // two input states.
  void ProcessUnconditionalTransition(const ParserState& target_state,
                                      const ParserState& p4_state);

  // Updates new_header with any relevant sub fields for the existing field
  // at sub_index.  The result is the number of sub fields that were added,
  // and it may be 0 if no subfields are defined for the field at sub_index.
  int InsertSubFields(int sub_index, ParserExtractHeader* new_header);

  // Updates visited_p4_header_names_ with any header name paths to the
  // extracted_header.
  void AddVisitedHeaders(const ParserExtractHeader& extracted_header);

  // Returns true if field_name is part of the p4_header extracted by the
  // current parser state.
  static bool IsFieldExtracted(const ParserExtractHeader& p4_header,
                               const std::string& field_name);

  // The TableMapGenerator is injected via the constructor.
  TableMapGenerator* table_mapper_;

  ParserMap working_field_map_;  // Scratch area for RunPass1 and RunPass2.
  bool done_;                    // Indicates MapFields has run when true.

  // This member caches the name of the start state in the target parser map.
  std::string target_start_name_;

  // This double-ended queue lists the state pairs that need to be compared
  // for field matches.
  std::deque<WorkQueueEntry> pass2_work_queue_;

  // Records header names that have already been mapped, avoiding attempts
  // to map them twice if the parser state machine has multiple paths to the
  // same header.
  std::set<std::string> visited_p4_header_names_;

  // Records parser state names that have already been visited to avoid
  // processing them again if multiple transitions lead to the same state.
  std::set<std::string> visited_p4_states_;

  // This container stores mapping data for fields that ParserFieldMapper
  // encounters as it processes the parser states, but they are not yet ready
  // for P4PipelineConfig updates. It has two categories of fields:
  //  1) Fields in headers that are not extracted by the parser, but are
  //     internally generated by the P4 program to be emitted during packet
  //     egress, such as ERSPAN.
  //  2) Fields in a header type that is extracted by multiple parser states,
  //     such as IPv4 inner and outer headers.
  // The map key is the field name.
  absl::node_hash_map<std::string, MappedFieldData> pass3_fields_map_;

  friend class ParserFieldMapperTest;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_PARSER_FIELD_MAPPER_H_
