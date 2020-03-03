// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// Contains unit tests for ParserDecoder.

#include "stratum/p4c_backends/fpm/parser_decoder.h"

#include <memory>
#include <string>
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

// The test Param string is a file name with JSON IR input.
class ParserDecoderTest : public testing::TestWithParam<std::string> {
 protected:
  // Reads an IR JSON file via IRTestHelperJson to set up a P4Parser
  // for test use.
  void SetUpIRParser(const std::string& ir_input_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string ir_path = "stratum/p4c_backends/" +
        ir_input_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(ir_path));
  }

  ParserDecoder parser_decoder_;  // Common instance for test use.
  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
};

// This set of tests should work for all input files with valid parser IR data:
//  TestAllStatesParsed - verifies that all IR parser states appear in
//      parser_decoder_'s decoded output.
//  TestReservedStates - verifies that parser_decoder_ recognizes the P4
//      parser's reserved states.
TEST_P(ParserDecoderTest, TestAllStatesParsed) {
  SetUpIRParser(GetParam());
  ASSERT_EQ(1, ir_helper_->program_inspector().parsers().size());
  const IR::P4Parser& ir_parser =
      *(ir_helper_->program_inspector().parsers()[0]);
  bool result = parser_decoder_.DecodeParser(ir_parser,
                                             ir_helper_->mid_end_refmap(),
                                             ir_helper_->mid_end_typemap());
  ASSERT_TRUE(result);
  const auto& decoded_states_map =
      parser_decoder_.parser_states().parser_states();
  EXPECT_EQ(ir_parser.states.size(), decoded_states_map.size());
  for (const auto ir_parser_state : ir_parser.states) {
    auto iter = decoded_states_map.find(std::string(
        ir_parser_state->getName().toString()));
    EXPECT_TRUE(iter != decoded_states_map.end());
  }
}

TEST_P(ParserDecoderTest, TestReservedStates) {
  SetUpIRParser(GetParam());
  ASSERT_EQ(1, ir_helper_->program_inspector().parsers().size());
  const IR::P4Parser& ir_parser =
      *(ir_helper_->program_inspector().parsers()[0]);
  bool result = parser_decoder_.DecodeParser(ir_parser,
                                             ir_helper_->mid_end_refmap(),
                                             ir_helper_->mid_end_typemap());
  ASSERT_TRUE(result);
  const auto& decoded_states_map =
      parser_decoder_.parser_states().parser_states();
  int start_states = 0;
  int accept_states = 0;
  int reject_states = 0;
  for (auto iter : decoded_states_map) {
    const ParserState& decoded_state = iter.second;
    if (decoded_state.reserved_state() == ParserState::P4_PARSER_STATE_START)
      ++start_states;
    if (decoded_state.reserved_state() == ParserState::P4_PARSER_STATE_ACCEPT)
      ++accept_states;
    if (decoded_state.reserved_state() == ParserState::P4_PARSER_STATE_REJECT)
      ++reject_states;
  }
  EXPECT_EQ(1, start_states);
  EXPECT_EQ(1, accept_states);
  EXPECT_EQ(1, reject_states);
}

// The test file has two states that do the same selection on the same pair
// of extracted fields.  The "start" state uses select(f1, f2).  The
// "parse_concat" state uses select(f1 ++ f2).  The parser_decoder_ should
// produce the same output for the select fields and case key values in
// both states.
TEST_F(ParserDecoderTest, ParseComplex) {
  SetUpIRParser("fpm/testdata/parse_complex.ir.json");
  ASSERT_EQ(1, ir_helper_->program_inspector().parsers().size());
  const IR::P4Parser& ir_parser =
      *(ir_helper_->program_inspector().parsers()[0]);
  bool result = parser_decoder_.DecodeParser(ir_parser,
                                             ir_helper_->mid_end_refmap(),
                                             ir_helper_->mid_end_typemap());
  ASSERT_TRUE(result);
  const auto& decoded_states_map =
      parser_decoder_.parser_states().parser_states();
  const auto& params_state_iter = decoded_states_map.find("start");
  ASSERT_TRUE(params_state_iter != decoded_states_map.end());
  const auto& concat_state_iter =
      decoded_states_map.find("parse_concat");
  ASSERT_TRUE(concat_state_iter != decoded_states_map.end());
  const auto& params_state = params_state_iter->second;
  const auto& concat_state = concat_state_iter->second;
  EXPECT_NE(0, params_state.extracted_header().header_paths_size());
  EXPECT_NE(0, concat_state.extracted_header().header_paths_size());
  ASSERT_TRUE(params_state.transition().has_select());
  ASSERT_TRUE(concat_state.transition().has_select());
  ASSERT_EQ(2, params_state.transition().select().selector_fields_size());
  ASSERT_EQ(2, concat_state.transition().select().selector_fields_size());
  ASSERT_EQ(params_state.transition().select().cases_size(),
            concat_state.transition().select().cases_size());
  for (int i = 0; i < params_state.transition().select().cases_size(); ++i) {
    const auto& params_case = params_state.transition().select().cases(i);
    const auto& concat_case = concat_state.transition().select().cases(i);
    if (params_case.is_default()) {
      EXPECT_TRUE(concat_case.is_default());
      continue;
    }
    ASSERT_EQ(2, params_case.keyset_values_size());
    ASSERT_EQ(2, concat_case.keyset_values_size());
    EXPECT_EQ(params_case.keyset_values(0).constant().value(),
              concat_case.keyset_values(0).constant().value());
    EXPECT_EQ(params_case.keyset_values(1).constant().value(),
              concat_case.keyset_values(1).constant().value());
    // TODO(unknown): Check keyset masks when properly encoded.
  }
}

// Verifies that the decoded parser states properly recognize header stacks.
TEST_F(ParserDecoderTest, ParseExtractStackedHeader) {
  SetUpIRParser("test/testdata/simple_vlan_stack_16.ir.json");
  ASSERT_EQ(1, ir_helper_->program_inspector().parsers().size());
  const IR::P4Parser& ir_parser =
      *(ir_helper_->program_inspector().parsers()[0]);
  bool result = parser_decoder_.DecodeParser(ir_parser,
                                             ir_helper_->mid_end_refmap(),
                                             ir_helper_->mid_end_typemap());
  ASSERT_TRUE(result);
  const auto& decoded_states_map =
      parser_decoder_.parser_states().parser_states();
  const auto& vlan_state_iter =
      decoded_states_map.find("parse_vlan_tag");
  ASSERT_TRUE(vlan_state_iter != decoded_states_map.end());
  const auto& vlan_state = vlan_state_iter->second;
  ASSERT_EQ(6, vlan_state.extracted_header().header_paths_size());
  EXPECT_EQ("hdr.vlan_tag[0]", vlan_state.extracted_header().header_paths(0));
  EXPECT_EQ("hdr.vlan_tag[1]", vlan_state.extracted_header().header_paths(1));
  EXPECT_EQ("hdr.vlan_tag[2]", vlan_state.extracted_header().header_paths(2));
  EXPECT_EQ("hdr.vlan_tag[3]", vlan_state.extracted_header().header_paths(3));
  EXPECT_EQ("hdr.vlan_tag[4]", vlan_state.extracted_header().header_paths(4));
  EXPECT_EQ("hdr.vlan_tag.last", vlan_state.extracted_header().header_paths(5));
}

// Verifies that the decoded parser states properly recognize value sets.
TEST_F(ParserDecoderTest, ParseValueSet) {
  SetUpIRParser("fpm/testdata/parse_value_set.ir.json");
  ASSERT_EQ(1, ir_helper_->program_inspector().parsers().size());
  const IR::P4Parser& ir_parser =
      *(ir_helper_->program_inspector().parsers()[0]);
  bool result = parser_decoder_.DecodeParser(ir_parser,
                                             ir_helper_->mid_end_refmap(),
                                             ir_helper_->mid_end_typemap());
  ASSERT_TRUE(result);
  const auto& decoded_states_map =
      parser_decoder_.parser_states().parser_states();
  const auto& udf_state_iter = decoded_states_map.find("parse_udf_payload");
  ASSERT_TRUE(udf_state_iter != decoded_states_map.end());
  const auto& udf_state = udf_state_iter->second;

  // The state with the value set should extract all the payload bytes.
  ASSERT_EQ(11, udf_state.extracted_header().header_paths_size());
  EXPECT_EQ("hdr.udf_payload[0]", udf_state.extracted_header().header_paths(0));
  EXPECT_EQ("hdr.udf_payload[1]", udf_state.extracted_header().header_paths(1));
  EXPECT_EQ("hdr.udf_payload[2]", udf_state.extracted_header().header_paths(2));
  EXPECT_EQ("hdr.udf_payload[3]", udf_state.extracted_header().header_paths(3));
  EXPECT_EQ("hdr.udf_payload[4]", udf_state.extracted_header().header_paths(4));
  EXPECT_EQ("hdr.udf_payload[5]", udf_state.extracted_header().header_paths(5));
  EXPECT_EQ("hdr.udf_payload[6]", udf_state.extracted_header().header_paths(6));
  EXPECT_EQ("hdr.udf_payload[7]", udf_state.extracted_header().header_paths(7));
  EXPECT_EQ("hdr.udf_payload[8]", udf_state.extracted_header().header_paths(8));
  EXPECT_EQ("hdr.udf_payload[9]", udf_state.extracted_header().header_paths(9));
  EXPECT_EQ("hdr.udf_payload.last",
            udf_state.extracted_header().header_paths(10));

  // The state with the value set selection should select the first two
  // transition cases by value set.
  ASSERT_LE(2, udf_state.transition().select().cases_size());
  const auto& udf_case1 = udf_state.transition().select().cases(0);
  ASSERT_EQ(1, udf_case1.keyset_values_size());
  EXPECT_TRUE(udf_case1.keyset_values(0).has_value_set());
  EXPECT_EQ("ParserImpl.udf_vs_1",
            udf_case1.keyset_values(0).value_set().value_set_name());
  const auto& udf_case2 = udf_state.transition().select().cases(1);
  ASSERT_EQ(1, udf_case2.keyset_values_size());
  EXPECT_TRUE(udf_case2.keyset_values(0).has_value_set());
  EXPECT_EQ("ParserImpl.udf_vs_2",
            udf_case2.keyset_values(0).value_set().value_set_name());
}

// Verifies error when calling DecodeParser multiple times.
TEST_F(ParserDecoderTest, TestDecodeTwice) {
  SetUpIRParser("fpm/testdata/parse_basic.ir.json");
  ASSERT_EQ(1, ir_helper_->program_inspector().parsers().size());
  const IR::P4Parser& ir_parser =
      *(ir_helper_->program_inspector().parsers()[0]);
  bool result1 = parser_decoder_.DecodeParser(ir_parser,
                                              ir_helper_->mid_end_refmap(),
                                              ir_helper_->mid_end_typemap());
  EXPECT_TRUE(result1);
  bool result2 = parser_decoder_.DecodeParser(ir_parser,
                                              ir_helper_->mid_end_refmap(),
                                              ir_helper_->mid_end_typemap());
  EXPECT_FALSE(result2);
}

// Tests error detection when the P4Parser has no start state.
TEST_F(ParserDecoderTest, TestNoStartState) {
  SetUpIRParser("fpm/testdata/parse_basic.ir.json");
  ASSERT_EQ(1, ir_helper_->program_inspector().parsers().size());
  const IR::P4Parser& ir_parser =
      *(ir_helper_->program_inspector().parsers()[0]);

  // The test constructs a new P4Parser with attributes from the normal test
  // parser, but using an empty ParserState vector.
  std::unique_ptr<IR::IndexedVector<IR::ParserState>> states(
      new IR::IndexedVector<IR::ParserState>());
  auto new_parser = new IR::P4Parser(ir_parser.srcInfo, ir_parser.name,
                                     ir_parser.type,
                                     ir_parser.constructorParams,
                                     ir_parser.parserLocals, *states);

  bool result = parser_decoder_.DecodeParser(*new_parser,
                                             ir_helper_->mid_end_refmap(),
                                             ir_helper_->mid_end_typemap());
  EXPECT_FALSE(result);
}

#if 0
// Tests error detection when the P4Parser has multiple start states.
// TODO(unknown): This test apparently messes with the IR in such a way that
//   IRTestHelperJson can't load the IR any more after it runs.
TEST_F(ParserDecoderTest, TestMultipleStartStates) {
  SetUpIRParser("fpm/testdata/parse_basic.ir.json");
  ASSERT_EQ(1, ir_helper_->program_inspector().parsers().size());
  const IR::P4Parser& ir_parser =
      *(ir_helper_->program_inspector().parsers()[0]);

  // This code to create a new IR::P4Parser is based on midend/noMatch.cpp
  // in the open source p4c code.
  auto vec1 = new IR::IndexedVector<IR::StatOrDecl>();
  auto start1 = new IR::ParserState(
      IR::ID(IR::ParserState::start), vec1,
      new IR::PathExpression(IR::ID(IR::ParserState::reject)));
  auto vec2 = new IR::IndexedVector<IR::StatOrDecl>();
  auto start2 = new IR::ParserState(
      IR::ID(IR::ParserState::start), vec2,
      new IR::PathExpression(IR::ID(IR::ParserState::reject)));
  auto two_start_states = new IR::IndexedVector<IR::ParserState>();
  two_start_states->push_back(start1);
  two_start_states->push_back(start2);
  auto new_parser = new IR::P4Parser(ir_parser.srcInfo, ir_parser.name,
                                     ir_parser.type,
                                     ir_parser.constructorParams,
                                     ir_parser.parserLocals,
                                     two_start_states);

  bool result = parser_decoder_.DecodeParser(*new_parser,
                                             ir_helper_->mid_end_refmap(),
                                             ir_helper_->mid_end_typemap());
  EXPECT_FALSE(result);
}
#endif

INSTANTIATE_TEST_SUITE_P(
  ValidParserIRInputFiles,
  ParserDecoderTest,
  ::testing::Values("fpm/testdata/parse_basic.ir.json",
                    "fpm/testdata/parse_complex.ir.json",
                    "fpm/testdata/parse_annotated_state.ir.json",
                    "fpm/testdata/parse_value_set.ir.json",
                    "test/testdata/simple_vlan_stack_16.ir.json")
);

}  // namespace p4c_backends
}  // namespace stratum
