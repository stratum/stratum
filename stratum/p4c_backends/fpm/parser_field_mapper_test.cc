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

// Contains ParserFieldMapper unit tests.

#include "stratum/p4c_backends/fpm/parser_field_mapper.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/substitute.h"
#include "gtest/gtest.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/parser_decoder.h"
#include "stratum/p4c_backends/fpm/parser_map.pb.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/table_map_generator_mock.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"

namespace stratum {
namespace p4c_backends {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ReturnRef;
using ::testing::StartsWith;

// This struct carries the parameters for subfield tests.
struct SubFieldTestParam {
  std::string state_name;  // Name of the state that will have subfields added.

  // Gives the index of a field that will have an error introduced.  The error
  // is induced after all subfield expansions, so the index needs to be aware
  // of the fields vector below.  A negative value causes no errors.
  int error_field_index;

  // This vector specifies how to encode subfields in the test header.
  // By convention, each test header begins with two fields with fixed width
  // and type.  The second field always acts as the state transition selector.
  // If the fields vector is non-empty, then the SetUpTestInputs method adds
  // N additional base fields, where N is the vector size.  Each entry in the
  // vector defines the number of subfields.  For this example: {0, 3, 0},
  // the tested header will have five total fields. Field 0 is a fixed field
  // for all tests.  Field 1 is also fixed, and selects the next state.  Field 2
  // is defined by fields[0], and it has no subfields in this case.  Field 3 is
  // defined by fields[1], and it has 3 subfields.  Field 4 is defined by
  // fields[2], and has no subfields.
  std::vector<int> fields;
};

class ParserFieldMapperTest : public testing::TestWithParam<SubFieldTestParam> {
 protected:
  static const int kUDFPayloadSize = 10;

  // Constants for names of fields in non-extracted field tests - must match
  // the fields in testdata/non_extracted_header.p4.
  static constexpr const char* kNonExtractEtherDest =
      "hdr.non_extracted_ethernet.dstAddr";
  static constexpr const char* kNonExtractEtherSource =
      "hdr.non_extracted_ethernet.srcAddr";
  static constexpr const char* kNonExtractEtherType =
      "hdr.non_extracted_ethernet.etherType";

  ParserFieldMapperTest() : test_parser_mapper_(&mock_table_mapper_) {}

  // These methods set up a set of ParserFieldMapper::MapFields inputs
  // for testing.  SetUpBasicTestInputs sets up two fields per header type,
  // without any subfields.  SetUpTestInputs takes additional inputs that
  // accommodate subfield testing.  SetUpFieldMapExpectations and
  // SetUpTargetField are helper functions for SetUpTestInputs.
  void SetUpBasicTestInputs();
  void SetUpTestInputs(const SubFieldTestParam& subfield_param);
  void SetUpTargetField(P4FieldType type, int offset, int width,
                        ParserExtractField* target_field);
  void SetUpFieldMapExpectations(P4FieldType field_type, int offset, int width,
                                 P4HeaderType header_type,
                                 const std::string& full_field_name);

  // SetUpValueSetTestInputs is similar to SetUpBasicTestInputs and
  // SetUpTestInputs, but it does a setup specifically for parser value set
  // testing.
  void SetUpValueSetTestInputs();

  // Another test setup function, with details for testing non-extracted
  // field mapping.
  void SetUpNonExtractedFieldTestInputs();

  // Gets test inputs from the compiled internal representation of a P4 program.
  void SetUpTestFromIRFile(const std::string& ir_file);

  // Populates decoded_field_map_ with entries for Ethernet header decoding.
  // The input flag indicates whether to setup for decoding fields not
  // extracted by a parser state.
  void SetUpEthernetDecodeMap(bool add_non_extracted_fields);

  // For friend access to the intermediate state.
  const ParserMap& mapper_working_field_map() const {
    return test_parser_mapper_.working_field_map_;
  }

  // Tests use two TableMapGenerators, one for mock expectations and one
  // for constucting a P4PipelineConfig to return through the mock.
  TableMapGeneratorMock mock_table_mapper_;
  TableMapGenerator real_table_mapper_;

  ParserFieldMapper test_parser_mapper_;  // Common ParserFieldMapper for tests.

  // These members provide the input for testing ParserFieldMapper::MapFields:
  //  p4_parser_field_map_ - provides the input from ParserDecoder.
  //  decoded_field_map_ - provides the input from FieldDecoder.
  //  target_parser_field_map_ - specifies parser behavior on the target switch.
  ParserMap p4_parser_field_map_;
  FieldDecoder::DecodedHeaderFieldMap decoded_field_map_;
  ParserMap target_parser_field_map_;

  // SetUpBasicTestInputs fills these vectors with mock_table_mapper_
  // expectations it deduces as it sets up the input data.
  std::vector<std::pair<std::string, hal::P4FieldDescriptor>> map_expectations_;
  std::vector<std::pair<std::string, P4HeaderType>> header_expectations_;

  // Populated by SetUpNonExtractedFieldTestInputs.
  std::vector<std::string> non_extracted_field_names_;
};

// This typedef redefines the ParserFieldMapperTest fixture for use with
// a specialized parameter set.
typedef ParserFieldMapperTest ParserFieldMapperSubfieldErrorTest;

void ParserFieldMapperTest::SetUpBasicTestInputs() {
  SubFieldTestParam no_subfield_param;
  no_subfield_param.state_name = "dont-care";
  no_subfield_param.error_field_index = -1;
  SetUpTestInputs(no_subfield_param);
}

void ParserFieldMapperTest::SetUpTestInputs(
    const SubFieldTestParam& subfield_param) {
  p4_parser_field_map_.Clear();
  decoded_field_map_.clear();
  target_parser_field_map_.Clear();

  // The p4_parser_field_map_ comes from a test data file.
  const std::string kFilePath =
      "stratum/p4c_backends/"
      "fpm/testdata/parse_basic.pb.txt";
  CHECK(ReadProtoFromTextFile(kFilePath, &p4_parser_field_map_).ok())
      << "Unable to read and parse test data in " << kFilePath;

  // The decoded_field_map_ defines the fields within each header type. For
  // test purposes, it is OK if the fields don't resemble the fields the header
  // defines in the P4 input file.  Each test header field gets a unique name,
  // field type, and bit width.
  int field_type_index = 1;
  int header_type_index = 1;
  int bit_width = 8;  // Initial value allows up to 8 subfields, if needed.
  for (const auto& iter : p4_parser_field_map_.parser_states()) {
    // The first part of the loop creates a state in target_parser_field_map_
    // for each p4_parser_field_map_ state that extracts a header.
    const ParserState& parser_state = iter.second;
    if (!parser_state.has_extracted_header()) continue;
    int field_offset = 0;
    ParserState target_parser_state;
    target_parser_state.set_name(parser_state.name());

    // Tests always use "parse_ethernet" as the start state.
    if (parser_state.name() == "parse_ethernet") {
      target_parser_state.set_reserved_state(
          ParserState::P4_PARSER_STATE_START);
    }
    *target_parser_state.mutable_transition() = parser_state.transition();
    if (target_parser_state.transition().has_select()) {
      target_parser_state.mutable_transition()
          ->mutable_select()
          ->clear_selector_fields();
    }

    CHECK_EQ(1, parser_state.extracted_header().header_paths_size());
    const std::string header_name =
        parser_state.extracted_header().header_paths(0);
    auto target_header = target_parser_state.mutable_extracted_header();
    target_header->set_name(header_name);
    target_header->set_header_type(
        static_cast<P4HeaderType>(header_type_index));
    if (header_type_index < P4HeaderType_MAX) ++header_type_index;
    header_expectations_.emplace_back(header_name,
                                      target_header->header_type());

    // The test setup creates at least two fields per header type,
    // assigning each field a different type from the P4FieldType enum.  If
    // all types end up being used, the remaining fields get the max type.
    // When the current parser state matches the subfield parameter, additional
    // subfields may be added.
    const int kFixedFields = 2;
    int num_fields = kFixedFields;
    if (parser_state.name() == subfield_param.state_name) {
      num_fields += subfield_param.fields.size();
    }
    std::vector<ParserExtractField> field_list;
    for (int f = 0; f < num_fields; ++f) {
      auto f_type = static_cast<P4FieldType>(field_type_index);

      // The second field is always set as the selector for tests.  This works
      // now, but it could break if ParserFieldMapper does more in-depth
      // verification of selector field consistency between states.
      if (f == 1 && target_parser_state.transition().has_select()) {
        target_parser_state.mutable_transition()
            ->mutable_select()
            ->add_selector_types(f_type);
      }

      int param_index = f - kFixedFields;  // Negative if f is not a subfield.
      int num_subfields = 0;
      if (param_index >= 0) num_subfields = subfield_param.fields[param_index];
      int extracted_fields = (num_subfields ? num_subfields : 1);

      // The first two vectors contain the name of the field and any alternate
      // subfields.  Subfield names, if any, are first, followed by the
      // base field name.  The third vector defines the width of each subfield.
      // If there are no subfields, the single entry represents the width
      // of the main field.
      std::vector<std::string> field_names;
      std::vector<std::string> full_field_names;
      std::vector<int> field_widths;
      int remaining_width = bit_width;

      for (int sub_f = 0; sub_f < num_subfields; ++sub_f) {
        const std::string field_name = absl::Substitute(
            "$0-$1-$2", P4FieldType_Name(f_type).c_str(), bit_width, sub_f);
        field_names.push_back(field_name);
        full_field_names.push_back(
            absl::Substitute("$0.$1", header_name.c_str(), field_name.c_str()));
        int subfield_width = bit_width / extracted_fields;
        if (sub_f == num_subfields - 1) subfield_width = remaining_width;
        field_widths.push_back(subfield_width);
        remaining_width -= subfield_width;
      }
      const std::string field_name = absl::Substitute(
          "$0-$1", P4FieldType_Name(f_type).c_str(), bit_width);
      field_names.push_back(field_name);
      full_field_names.push_back(
          absl::Substitute("$0.$1", header_name.c_str(), field_name.c_str()));
      field_widths.push_back(bit_width);

      // When setting up a field extracted by the parser, only the subfields
      // need to be created when a field has both a base field and subfield
      // options.  In the subfield case, the base field width is split among
      // the subfields by the field_widths vector entries.
      int subfield_total_width = 0;
      for (int i = 0; i < extracted_fields; ++i) {
        ParserExtractField field;
        field.set_name(field_names[i]);
        field.set_bit_offset(field_offset + subfield_total_width);
        field.set_bit_width(field_widths[i]);
        field.add_full_field_names(full_field_names[i]);
        field_list.push_back(field);
        subfield_total_width += field_widths[i];
      }

      // Target field setup is as follows:
      //  No subfields - The base field is created, with an expectation that
      //      the table map gets an entry for the base field.
      //  With subfields - The base field is created and linked to a subfield
      //      set.  The subfield set is populated with each subfield.  An
      //      expectation is defined for each subfield's table map entry,
      //      but the base field should not be present in the table map.
      ParserExtractField* base_field = target_header->add_fields();
      SetUpTargetField(f_type, field_offset, bit_width, base_field);
      if (num_subfields > 0) {
        const std::string subfield_set_name =
            absl::Substitute("$0-subfields", field_name.c_str());
        base_field->set_subfield_set_name(subfield_set_name);
        ParserSubFieldSet* subfield_set = target_header->add_subfield_sets();
        subfield_set->set_name(subfield_set_name);
        int subfield_offset = field_offset;
        for (int s = 0; s < num_subfields; ++s) {
          // Every subfield currently uses the same f_type value.  While not
          // totally realistic, this does not cause any test problems.
          SetUpTargetField(f_type, subfield_offset, field_widths[s],
                           subfield_set->add_fields());
          SetUpFieldMapExpectations(f_type, subfield_offset, field_widths[s],
                                    target_header->header_type(),
                                    full_field_names[s]);
          subfield_offset += field_widths[s];
        }
      } else {
        SetUpFieldMapExpectations(f_type, field_offset, bit_width,
                                  target_header->header_type(),
                                  full_field_names[0]);
      }

      field_offset += bit_width;
      ++bit_width;
      if (field_type_index < P4FieldType_MAX) ++field_type_index;
    }

    decoded_field_map_[parser_state.extracted_header().name()] = field_list;
    (*target_parser_field_map_.mutable_parser_states())[parser_state.name()] =
        target_parser_state;
  }

  EXPECT_CALL(mock_table_mapper_, generated_map())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(real_table_mapper_.generated_map()));
}

void ParserFieldMapperTest::SetUpTargetField(P4FieldType type, int offset,
                                             int width,
                                             ParserExtractField* target_field) {
  target_field->set_type(type);
  target_field->set_bit_offset(offset);
  target_field->set_bit_width(width);
}

void ParserFieldMapperTest::SetUpFieldMapExpectations(
    P4FieldType field_type, int offset, int width, P4HeaderType header_type,
    const std::string& full_field_name) {
  hal::P4FieldDescriptor expected_descriptor;
  expected_descriptor.set_type(field_type);
  expected_descriptor.set_header_type(header_type);
  expected_descriptor.set_bit_offset(offset);
  expected_descriptor.set_bit_width(width);
  map_expectations_.push_back(
      std::make_pair(full_field_name, expected_descriptor));
}

// Unlike the previous setup methods, SetUpValueSetTestInputs gets its test
// data by reading the IR from a JSON file.  Then, a ParserDecoder converts
// the IR::P4Parser from the test program into the p4_parser_field_map_ for
// test use.  The target_parser_field_map_ also comes from a file instead
// of being deduced from the p4_parser_field_map_.
void ParserFieldMapperTest::SetUpValueSetTestInputs() {
  SetUpTestFromIRFile("parse_value_set.ir.json");

  // The P4 test program parses Ethernet standard headers and fields, which the
  // test makes part of the decoded_field_map_.
  SetUpEthernetDecodeMap(false);

  // The P4 test program parses these payload headers and fields with
  // value sets.  As the Stratum P4 programs are written, every payload
  // octet is a member of a large payload header stack.
  ParserExtractField field;
  std::vector<ParserExtractField> field_list;
  field.set_name("udf_data");
  field.set_bit_offset(0);
  field.set_bit_width(8);
  for (int i = 0; i < kUDFPayloadSize; ++i) {
    const std::string full_field_name =
        absl::Substitute("hdr.udf_payload[$0].udf_data", i);
    field.add_full_field_names(full_field_name);
    header_expectations_.emplace_back(
        absl::Substitute("hdr.udf_payload[$0]", i), P4_HEADER_UDP_PAYLOAD);
    SetUpFieldMapExpectations(P4_FIELD_TYPE_UDP_PAYLOAD_DATA,
                              field.bit_offset(), field.bit_width(),
                              P4_HEADER_UDP_PAYLOAD, full_field_name);
  }
  field_list.push_back(field);
  field.Clear();
  decoded_field_map_["udf_payload_t"] = field_list;

  EXPECT_CALL(mock_table_mapper_, generated_map())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(real_table_mapper_.generated_map()));
}

void ParserFieldMapperTest::SetUpNonExtractedFieldTestInputs() {
  non_extracted_field_names_ = {kNonExtractEtherDest, kNonExtractEtherSource,
                                kNonExtractEtherType};
  SetUpTestFromIRFile("non_extracted_header.ir.json");
  SetUpEthernetDecodeMap(true);

  // These very general expectations are for the standard ethernet header,
  // since extracted headers have been well tested elsewhere.
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(mock_table_mapper_,
              SetFieldAttributes(StartsWith("hdr.ethernet"), _, _, _, _))
      .Times(AnyNumber());

  EXPECT_CALL(mock_table_mapper_, generated_map())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(real_table_mapper_.generated_map()));
}

void ParserFieldMapperTest::SetUpTestFromIRFile(const std::string& ir_file) {
  auto ir_helper = absl::make_unique<IRTestHelperJson>();
  const std::string ir_path =
      "stratum/p4c_backends/"
      "fpm/testdata/" +
      ir_file;
  ASSERT_TRUE(ir_helper->GenerateTestIRAndInspectProgram(ir_path));
  p4_parser_field_map_.Clear();
  auto decoder = absl::make_unique<ParserDecoder>();
  ASSERT_TRUE(decoder->DecodeParser(
      *(ir_helper->program_inspector().parsers()[0]),
      ir_helper->mid_end_refmap(), ir_helper->mid_end_typemap()));
  p4_parser_field_map_ = decoder->parser_states();

  // This target parser map text file is suitable for all IR-based tests.
  const std::string kFilePath =
      "stratum/p4c_backends/"
      "fpm/testdata/value_set_parser_map.pb.txt";
  CHECK(ReadProtoFromTextFile(kFilePath, &target_parser_field_map_).ok())
      << "Unable to read and parse test data in " << kFilePath;
}

void ParserFieldMapperTest::SetUpEthernetDecodeMap(
    bool add_non_extracted_fields) {
  ParserExtractField field;
  std::vector<ParserExtractField> field_list;
  field.set_name("dstAddr");
  field.set_bit_offset(0);
  field.set_bit_width(48);
  field.add_full_field_names("hdr.ethernet.dstAddr");
  field_list.push_back(field);
  field.Clear();
  field.set_name("srcAddr");
  field.set_bit_offset(48);
  field.set_bit_width(48);
  field.add_full_field_names("hdr.ethernet.srcAddr");
  field_list.push_back(field);
  field.Clear();
  field.set_name("etherType");
  field.set_bit_offset(96);
  field.set_bit_width(16);
  field.add_full_field_names("hdr.ethernet.etherType");
  field_list.push_back(field);
  field.Clear();

  if (add_non_extracted_fields) {
    for (int i = 0; i < non_extracted_field_names_.size(); ++i) {
      field_list[i].add_full_field_names(non_extracted_field_names_[i]);
      real_table_mapper_.AddField(non_extracted_field_names_[i]);
    }
  }

  decoded_field_map_["ethernet_t"] = field_list;
  field_list.clear();
}

// The first set of tests uses an empty target_parser_field_map_.  This makes
// ParserFieldMapper's second pass a NOP, and lets these tests verify the
// output of the first pass and the way errors are handled.
TEST_F(ParserFieldMapperTest, TestPass1Only) {
  SetUpBasicTestInputs();
  target_parser_field_map_.Clear();
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  ASSERT_TRUE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));

  // Every parser state that extracts fields should have fields added that
  // match the header definition in decoded_field_map_.
  for (auto iter : mapper_working_field_map().parser_states()) {
    const ParserState& decoded_state = iter.second;
    if (decoded_state.has_extracted_header()) {
      const ParserExtractHeader& header = decoded_state.extracted_header();
      const auto iter = decoded_field_map_.find(header.name());
      ASSERT_TRUE(iter != decoded_field_map_.end());
      const std::vector<ParserExtractField>& expected_fields = iter->second;
      ASSERT_EQ(expected_fields.size(), header.fields_size());
      int expected_offset = 0;
      for (int f = 0; f < expected_fields.size(); ++f) {
        EXPECT_EQ(expected_fields[f].name(), header.fields(f).name());
        EXPECT_EQ(expected_fields[f].bit_width(), header.fields(f).bit_width());
        EXPECT_EQ(expected_offset, header.fields(f).bit_offset());
        expected_offset += header.fields(f).bit_width();
      }
    }
  }
}

TEST_F(ParserFieldMapperTest, TestMapFieldsNoParserInput) {
  // This test clears p4_parser_field_map_ to test empty input.
  SetUpBasicTestInputs();
  p4_parser_field_map_.Clear();
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

TEST_F(ParserFieldMapperTest, TestMapFieldsMissingHeaderType) {
  // This test erases one header type from the decoded_field_map_, verifying
  // the error when the p4_parser_field_map_ can't find an expected header.
  SetUpBasicTestInputs();
  target_parser_field_map_.Clear();
  decoded_field_map_.erase("vlan_tag_t");
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// Confirms that MapFields returns an error when called multiple times.
TEST_F(ParserFieldMapperTest, TestMapFieldsTwice) {
  SetUpBasicTestInputs();
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _))
      .Times(AnyNumber());
  EXPECT_TRUE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

TEST_F(ParserFieldMapperTest, TestNormalMapping) {
  SetUpBasicTestInputs();
  for (const auto& iter : map_expectations_) {
    const hal::P4FieldDescriptor& expected_descriptor = iter.second;
    EXPECT_CALL(mock_table_mapper_,
                SetFieldAttributes(iter.first, expected_descriptor.type(),
                                   expected_descriptor.header_type(),
                                   expected_descriptor.bit_offset(),
                                   expected_descriptor.bit_width()))
        .Times(1);
  }
  for (const auto& iter : header_expectations_) {
    EXPECT_CALL(mock_table_mapper_,
                SetHeaderAttributes(iter.first, iter.second, _))
        .Times(1);
  }
  EXPECT_TRUE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// The next series of tests verifies that MapFields returns an error when the
// input target_parser_field_map_ is invalid.
TEST_F(ParserFieldMapperTest, TestMultipleTargetStartStates) {
  SetUpBasicTestInputs();
  for (auto& iter : *target_parser_field_map_.mutable_parser_states()) {
    if (iter.second.reserved_state() != ParserState::P4_PARSER_STATE_START) {
      iter.second.set_reserved_state(ParserState::P4_PARSER_STATE_START);
      break;
    }
  }
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _))
      .Times(AnyNumber());
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// This test clears all reserved states so there is no start.
TEST_F(ParserFieldMapperTest, TestNoTargetStartState) {
  SetUpBasicTestInputs();
  for (auto& iter : *target_parser_field_map_.mutable_parser_states()) {
    iter.second.set_reserved_state(ParserState::P4_PARSER_STATE_NOT_RESERVED);
  }
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// This test clears all extracted header details in one state.
TEST_F(ParserFieldMapperTest, TestTargetStateExtractsNoHeader) {
  SetUpBasicTestInputs();
  auto iter = target_parser_field_map_.mutable_parser_states()->begin();
  iter->second.clear_extracted_header();
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// This test clears all the fields in one state's header.
TEST_F(ParserFieldMapperTest, TestTargetStateExtractsNoHeaderFields) {
  SetUpBasicTestInputs();
  auto iter = target_parser_field_map_.mutable_parser_states()->begin();
  iter->second.mutable_extracted_header()->clear_fields();
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// This test makes the first two fields in one state have the same offset.
TEST_F(ParserFieldMapperTest, TestTargetStateNonIncreasingFieldOffset) {
  SetUpBasicTestInputs();
  auto iter = target_parser_field_map_.mutable_parser_states()->begin();
  auto header = iter->second.mutable_extracted_header();
  ASSERT_LE(2, header->fields_size());
  header->mutable_fields(0)->set_bit_offset(header->fields(1).bit_offset());
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// This test adds a second select field type to one state's select expression.
TEST_F(ParserFieldMapperTest, TestTargetStateMultipleSelectFields) {
  SetUpBasicTestInputs();
  for (auto& iter : *target_parser_field_map_.mutable_parser_states()) {
    auto& target_state = iter.second;
    if (target_state.transition().has_select()) {
      target_state.mutable_transition()->mutable_select()->add_selector_types(
          P4_FIELD_TYPE_EGRESS_PORT);
      break;
    }
  }
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// This test adds an unknown next state to one state's select expression.
TEST_F(ParserFieldMapperTest, TestTargetStateUnknownNextState) {
  SetUpBasicTestInputs();
  for (auto& iter : *target_parser_field_map_.mutable_parser_states()) {
    auto& target_state = iter.second;
    if (target_state.transition().has_select()) {
      ASSERT_LE(1, target_state.transition().select().cases_size());
      target_state.mutable_transition()
          ->mutable_select()
          ->mutable_cases(0)
          ->set_next_state("unknown-state");
      break;
    }
  }
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// This test adds an extra case key to one state's select expression.
TEST_F(ParserFieldMapperTest, TestTargetStateMultipleCaseKeyValues) {
  SetUpBasicTestInputs();
  for (auto& iter : *target_parser_field_map_.mutable_parser_states()) {
    auto& target_state = iter.second;
    if (target_state.transition().has_select()) {
      ASSERT_LE(1, target_state.transition().select().cases_size());
      auto new_key = target_state.mutable_transition()
                         ->mutable_select()
                         ->mutable_cases(0)
                         ->add_keyset_values();
      new_key->mutable_constant()->set_value(123);
      new_key->mutable_constant()->set_mask(0x1ff);
      break;
    }
  }
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// This test verifies failure with an empty field decoder map input.
TEST_F(ParserFieldMapperTest, TestTargetNoDecodedFields) {
  SetUpBasicTestInputs();
  decoded_field_map_.clear();
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// This test verifies failure when the target start state doesn't match
// any of the P4 parser states.
TEST_F(ParserFieldMapperTest, TestNoStartStateMatch) {
  SetUpBasicTestInputs();
  for (auto& iter : *target_parser_field_map_.mutable_parser_states()) {
    if (iter.second.reserved_state() == ParserState::P4_PARSER_STATE_START) {
      // This test adds a field to the start state header so it won't match.
      auto start_header = iter.second.mutable_extracted_header();
      ParserExtractField new_field;
      new_field.set_type(P4_FIELD_TYPE_VRF);
      new_field.set_bit_offset(128);
      new_field.set_bit_width(16);
      *start_header->add_fields() = new_field;
      break;
    }
  }
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// This test verifies failure when the target start state ambiguously matches
// multiple P4 parser states.
TEST_F(ParserFieldMapperTest, TestAmbiguousStartState) {
  SetUpBasicTestInputs();

  // This test replaces the target start state and all P4 parser states with
  // this ambiguous single-field header.
  ParserExtractField field;
  field.set_bit_offset(0);
  field.set_bit_width(32);
  for (auto& iter : *target_parser_field_map_.mutable_parser_states()) {
    if (iter.second.reserved_state() == ParserState::P4_PARSER_STATE_START) {
      auto start_header = iter.second.mutable_extracted_header();
      field.set_type(P4_FIELD_TYPE_VRF);
      start_header->clear_fields();
      *start_header->add_fields() = field;
      break;
    }
  }
  for (auto& iter : decoded_field_map_) {
    auto& field_vector = iter.second;
    ASSERT_LE(1, field_vector.size());
    const std::string field_name = field_vector[0].name();
    field_vector.clear();
    field.set_name(field_name);
    field_vector.push_back(field);
  }

  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _)).Times(0);
  EXPECT_FALSE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// Tests parser mapping of alternate subfields in various valid combinations,
// as specified by the test parameters.
TEST_P(ParserFieldMapperTest, TestSubFields) {
  SetUpTestInputs(GetParam());
  for (const auto& iter : map_expectations_) {
    const hal::P4FieldDescriptor& expected_descriptor = iter.second;
    EXPECT_CALL(mock_table_mapper_,
                SetFieldAttributes(iter.first, expected_descriptor.type(),
                                   expected_descriptor.header_type(),
                                   expected_descriptor.bit_offset(),
                                   expected_descriptor.bit_width()))
        .Times(1);
  }
  for (const auto& iter : header_expectations_) {
    EXPECT_CALL(mock_table_mapper_,
                SetHeaderAttributes(iter.first, iter.second, _))
        .Times(1);
  }
  EXPECT_TRUE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// Tests parser mapping of alternate subfields when a subfield set is undefined.
TEST_F(ParserFieldMapperTest, TestMissingSubFieldSet) {
  SubFieldTestParam test_param;
  test_param.state_name = "parse_l3_protocol_2";
  test_param.error_field_index = -1;
  test_param.fields.push_back(2);  // Creates a two-subfield split.
  SetUpTestInputs(test_param);

  // This test triggers the error by finding the header with subfields and
  // mangling the subfield set name.
  for (auto& state_iter : *target_parser_field_map_.mutable_parser_states()) {
    ParserExtractHeader* extracted_header =
        state_iter.second.mutable_extracted_header();
    for (auto& subfield_set : *extracted_header->mutable_subfield_sets()) {
      subfield_set.set_name(
          absl::Substitute("XX-$0", subfield_set.name().c_str()));
    }
  }

  // This is a primitive test that only assures nothing crashes when subfield
  // set name references cannot be resolved.
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _))
      .Times(AnyNumber());
  EXPECT_TRUE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// Tests parser mapping of alternate subfields in various invalid combinations,
// as specified by the test parameters.
TEST_P(ParserFieldMapperSubfieldErrorTest, TestSubFieldErrors) {
  const SubFieldTestParam& params = GetParam();
  SetUpTestInputs(params);

  // To cause a failure, the parameterized parser state is found, and then
  // the Nth decoded field's bit width is adjusted to make the
  // test_parser_mapper_ fail to align fields across states.
  const auto& state_iter =
      p4_parser_field_map_.parser_states().find(params.state_name);
  ASSERT_TRUE(state_iter != p4_parser_field_map_.parser_states().end());
  const auto& decoded_field_iter =
      decoded_field_map_.find(state_iter->second.extracted_header().name());
  ASSERT_TRUE(decoded_field_iter != decoded_field_map_.end());
  ASSERT_LT(params.error_field_index, decoded_field_iter->second.size());
  ParserExtractField* error_field =
      &decoded_field_iter->second[params.error_field_index];
  error_field->set_bit_width(error_field->bit_width() + 1);

  // The ParserFieldMapper doesn't stop until it hits an error, so it
  // will create some header and field descriptors during the test, which
  // is the reason for the very broad expectations below.  There are also
  // more explicit expectations to make sure the field or header attributes
  // related to the field in error do not get written.
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(
      mock_table_mapper_,
      SetHeaderAttributes(state_iter->second.extracted_header().name(), _, _))
      .Times(0);
  EXPECT_CALL(mock_table_mapper_,
              SetFieldAttributes(error_field->full_field_names(0), _, _, _, _))
      .Times(0);

  // The ParserFieldMapper returns true if it successfully processes at least
  // one header, and this test covers a partial failure, hence the EXPECT_TRUE.
  EXPECT_TRUE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// Verifies parsing of value sets.
TEST_F(ParserFieldMapperTest, TestValueSets) {
  SetUpValueSetTestInputs();

  // This test assumes that other tests have verified standard fixed-field
  // headers, so it sets very loose expectations for them.  It sets more
  // specific expectations to verify the proper output of the UDF payload.
  EXPECT_CALL(mock_table_mapper_, SetHeaderAttributes(_, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(_, _, _, _, _))
      .Times(AnyNumber());
  for (const auto& iter : header_expectations_) {
    EXPECT_CALL(mock_table_mapper_,
                SetHeaderAttributes(iter.first, iter.second, _))
        .Times(1);
  }
  for (const auto& iter : map_expectations_) {
    const hal::P4FieldDescriptor& expected_descriptor = iter.second;
    EXPECT_CALL(mock_table_mapper_,
                SetFieldAttributes(iter.first, expected_descriptor.type(),
                                   expected_descriptor.header_type(),
                                   expected_descriptor.bit_offset(),
                                   expected_descriptor.bit_width()))
        .Times(1);
  }

  EXPECT_TRUE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// Tests basic field mapping for a non-extracted header.
TEST_F(ParserFieldMapperTest, TestNonExtractedHeader) {
  SetUpNonExtractedFieldTestInputs();

  // The field attributes of the non-extracted fields should be set
  // via the mock_table_mapper_.
  for (const auto& field_name : non_extracted_field_names_) {
    EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(field_name, _, _, _, _))
        .Times(1);
  }

  EXPECT_TRUE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// Tests field mapping for a non-extracted header with additional unmappable
// descriptors in the P4PipelineConfig table map.
TEST_F(ParserFieldMapperTest, TestNonExtractedHeaderWithOtherDescriptors) {
  SetUpNonExtractedFieldTestInputs();
  real_table_mapper_.AddTable("must-not-map-this-table");
  real_table_mapper_.AddField("unknown-field-1");
  real_table_mapper_.AddField("unknown-field-2");
  EXPECT_CALL(mock_table_mapper_,
              SetFieldAttributes("unknown-field-1", _, _, _, _))
      .Times(0);
  EXPECT_CALL(mock_table_mapper_,
              SetFieldAttributes("unknown-field-2", _, _, _, _))
      .Times(0);

  // The field attributes of the non-extracted fields should be set
  // via the mock_table_mapper_.
  for (const auto& field_name : non_extracted_field_names_) {
    EXPECT_CALL(mock_table_mapper_, SetFieldAttributes(field_name, _, _, _, _))
        .Times(1);
  }

  EXPECT_TRUE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// Tests field mapping where some of the non-extracted fields obtained a
// field type assignment by some other means.  A realistic use case of this
// is an inner header field that was initially processed with the outer header,
// treated as non-extracted, then fully processed again with the inner header.
TEST_F(ParserFieldMapperTest, TestNonExtractedHeaderOuterInner) {
  SetUpNonExtractedFieldTestInputs();

  // Create a full field descriptor for the destination address.
  real_table_mapper_.AddField(non_extracted_field_names_[0]);
  real_table_mapper_.SetFieldAttributes(non_extracted_field_names_[0],
                                        P4_FIELD_TYPE_ETH_DST,
                                        P4_HEADER_ETHERNET, 0, 48);
  EXPECT_CALL(mock_table_mapper_,
              SetFieldAttributes(non_extracted_field_names_[0], _, _, _, _))
      .Times(0);

  // The field attributes of the other two non-extracted fields should
  // still be set via the mock_table_mapper_.
  for (int i = 1; i < non_extracted_field_names_.size(); ++i) {
    EXPECT_CALL(mock_table_mapper_,
                SetFieldAttributes(non_extracted_field_names_[i], _, _, _, _))
        .Times(1);
  }

  EXPECT_TRUE(test_parser_mapper_.MapFields(
      p4_parser_field_map_, decoded_field_map_, target_parser_field_map_));
}

// TODO(unknown): Coverage is needed for normalization of P4 parser states
// using multiple fields, either comma-separated or via P4 "concat" operator.

// This set of parameters tests some normal subfield combinations.
INSTANTIATE_TEST_SUITE_P(
    SubFields, ParserFieldMapperTest,
    ::testing::Values(
        SubFieldTestParam{"parse_l3_protocol_2", -1, std::vector<int>{0, 0}},
        SubFieldTestParam{"parse_l3_protocol_2", -1, std::vector<int>{5, 3}},
        SubFieldTestParam{"parse_l3_protocol_2", -1, std::vector<int>{2}},
        SubFieldTestParam{"parse_l3_protocol_2", -1, std::vector<int>{2, 0}},
        SubFieldTestParam{"parse_l3_protocol_2", -1, std::vector<int>{2, 0, 3}},
        SubFieldTestParam{"parse_l3_protocol_2", -1,
                          std::vector<int>{5, 0, 2, 0}},
        SubFieldTestParam{"parse_l3_protocol_2", -1, std::vector<int>{4, 3}}));

// This set of parameters tests various failures to match on subfields.
INSTANTIATE_TEST_SUITE_P(
    SubFieldErrors, ParserFieldMapperSubfieldErrorTest,
    ::testing::Values(
        SubFieldTestParam{"parse_l3_protocol_2", 2, std::vector<int>{0, 0}},
        SubFieldTestParam{"parse_l3_protocol_2", 3, std::vector<int>{0, 0}},
        SubFieldTestParam{"parse_l3_protocol_2", 3, std::vector<int>{2}},
        SubFieldTestParam{"parse_l3_protocol_2", 3, std::vector<int>{2, 0}},
        SubFieldTestParam{"parse_l3_protocol_2", 4, std::vector<int>{2, 0}},
        SubFieldTestParam{"parse_l3_protocol_2", 4, std::vector<int>{2, 0, 3}},
        SubFieldTestParam{"parse_l3_protocol_2", 5, std::vector<int>{2, 0, 3}},
        SubFieldTestParam{"parse_l3_protocol_2", 9, std::vector<int>{5, 3}}));

}  // namespace p4c_backends
}  // namespace stratum
