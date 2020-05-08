// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// The ParserValueSetMapper is an IR Inspector subclass that processes value
// sets in the P4 program's parser.  It identifies fields that should be
// classified as P4_FIELD_TYPE_UDF_VALUE_SET, meaning that the Stratum switch
// stack should treat them as UDFs whose packet payload offset is configured
// dynamically by the P4Runtime configuration of a parser value set.

#ifndef STRATUM_P4C_BACKENDS_FPM_PARSER_VALUE_SET_MAPPER_H_
#define STRATUM_P4C_BACKENDS_FPM_PARSER_VALUE_SET_MAPPER_H_

#include <map>
#include <string>

#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/p4c_backends/fpm/parser_map.pb.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

class ParserValueSetMapper : public Inspector {
 public:
  // The constructor needs these parameters:
  //  p4_parser_map, which is the output from a previous ParserDecoder
  //      traversal of the P4 program parser.
  //  p4_info_manager, which provides access to P4Info ValueSet definitions.
  //  table_mapper, which supports appending value set attributes to field
  //      descriptors in the P4PipelineConfig.
  // The caller retains ownership of all injected pointers.
  ParserValueSetMapper(const ParserMap& p4_parser_map,
                       const hal::P4InfoManager& p4_info_manager,
                       TableMapGenerator* table_mapper);
  ~ParserValueSetMapper() override {}

  // MapValueSets inspects the input p4_parser to find fields that represent
  // UDFs configured by parser value sets.  It uses the injected table_mapper
  // to update the P4TableMap field descriptors of any fields that act as
  // value-set-configurable UDFs.
  bool MapValueSets(const IR::P4Parser& p4_parser);

  // These preorder overrides return true to visit deeper nodes in the IR.
  bool preorder(const IR::ParserState* ir_parser_state) override;
  bool preorder(const IR::AssignmentStatement* statement) override;

  // ParserValueSetMapper is neither copyable nor movable.
  ParserValueSetMapper(const ParserValueSetMapper&) = delete;
  ParserValueSetMapper& operator=(const ParserValueSetMapper&) = delete;

 private:
  // This struct contains information about a parser state that was selected
  // by the content of a ValueSet.
  struct ValueSetState {
    explicit ValueSetState(const std::string& vs_name)
        : value_set_name(vs_name),
          header_type(P4_HEADER_UNKNOWN) {
    }

    const std::string value_set_name;
    P4HeaderType header_type;
  };

  // Populates a ValueSetState entry in the value_set_states_ map for each
  // state that has a transition selected by a value set.
  void FindValueSetTransitions();

  // Processes the expression on the right side of a parser assignment.
  bool ProcessAssignmentRight(const IR::Expression& right);

  // Processes the expression on the left side of a parser assignment.
  bool ProcessAssignmentLeft(const IR::Expression& left);

  // Constructor-injected members.
  const ParserMap& p4_parser_map_;
  const hal::P4InfoManager& p4_info_manager_;
  TableMapGenerator* table_mapper_;

  // This map contains an entry for each parser state that is selected by
  // a value set.  In the example P4 parser snippet below, vset1 and
  // vset2 are previously defined as value sets:
  //
  //  state parse_vset_payload {
  //    packet.extract(hdr.vset_payload);
  //    select_index = select_index + 1;
  //    transition_select (select_index) {
  //      vset1: parse_vset1;
  //      vset2: parse_vset2;
  //      3: accept;
  //      default: parse_vset_payload;
  //    }
  //  }
  //
  // The value_set_states_ map for the example has two entries, one for
  // "parse_vset1" and one for "parse_vset2".
  std::map<std::string, ValueSetState> value_set_states_;

  // This member points to the ValueSetState corresponding to the
  // IR::ParserState that is currently inspected.  It is NULL when the current
  // inspected state does not have any value set selections, or when the
  // inspection has not yet reached an IR::ParserState.  The value_set_states_
  // map owns this object.
  ValueSetState* visiting_state_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_PARSER_VALUE_SET_MAPPER_H_
