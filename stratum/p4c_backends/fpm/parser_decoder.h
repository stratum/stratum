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

// The ParserDecoder traverses the states and expressions within a P4Parser
// instance in the IR, generating a ParserMap message to represent the
// parser behavior.

#ifndef STRATUM_P4C_BACKENDS_FPM_PARSER_DECODER_H_
#define STRATUM_P4C_BACKENDS_FPM_PARSER_DECODER_H_

#include <map>
#include <string>

#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"
#include "stratum/p4c_backends/fpm/parser_map.pb.h"

namespace stratum {
namespace p4c_backends {

class ParserDecoder {
 public:
  ParserDecoder();
  virtual ~ParserDecoder() {}

  // DecodeParser takes the P4Parser node from the IR as input.  It visits
  // all of the underlying parser states and extracts information into a
  // ParserMap message, which is available through the parser_states accessor.
  // The caller provides ref_map and type_map from p4c frontend and midend
  // passes.  The return value is true if ParserMap creation succeeds.
  virtual bool DecodeParser(const IR::P4Parser& p4_parser,
                            P4::ReferenceMap* ref_map, P4::TypeMap* type_map);

  // This accessor is valid only after DecodeParser runs successfully.
  const ParserMap& parser_states() const { return parser_states_; }

  // ParserDecoder is neither copyable nor movable.
  ParserDecoder(const ParserDecoder&) = delete;
  ParserDecoder& operator=(const ParserDecoder&) = delete;

 private:
  // These methods decode the statements and expressions within the P4Parser
  // IR node, returning true if successful.
  bool DecodeStatements(const IR::Vector<IR::StatOrDecl>& components,
                        ParserState* decoded_state);
  bool DecodeSelectExpression(const IR::SelectExpression& expression,
                              ParserState* decoded_state);
  bool DecodePathExpression(const IR::PathExpression& expression,
                            ParserState* decoded_state);

  // These methods handle select key decoding.  "Simple" keys have a single
  // header field per select expression.  "Complex" keys have a list of
  // multiple fields per select expression.  Parser value sets are another
  // form of select key.
  void DecodeSimpleSelectKeySet(const IR::Expression& key_set,
                                ParserSelectCase* decoded_case);
  void DecodeComplexSelectKeySet(const IR::ListExpression& key_set,
                                 const IR::ListExpression& select,
                                 ParserSelectCase* decoded_case);
  bool DecodeValueSetSelectKeySet(const IR::Expression& key_set,
                                  ParserSelectCase* decoded_case);

  // Decodes the situation where a select expression joins two fields via
  // the P4 "concat" operator.
  void DecodeConcatOperator(const IR::Concat& concat,
                            ParserSelectExpression* decoded_select);

  // Determines whether the input statement extracts a P4 header type.  If so,
  // the returned string contains the extracted type's name.  Otherwise, the
  // returned string is empty.
  std::string ExtractHeaderType(const IR::MethodCallStatement& statement);

  // DecodeParser uses this member to store the generated ParserMap.
  ParserMap parser_states_;

  // This map stores the parser value sets.  The key is the value set name,
  // and the value is the bit width.
  std::map<std::string, int> value_sets_;

  // The ref_map_ and type_map_ are provided by the DecodeParser method caller.
  // They are cached here for member use while DecodeParser runs,
  // and the caller retains ownership.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_PARSER_DECODER_H_
