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

// This file contains ParserValueSetMapper unit tests.

#include "stratum/p4c_backends/fpm/parser_value_set_mapper.h"

#include <string>
#include <vector>

#include "stratum/hal/lib/p4/p4_info_manager_mock.h"
#include "stratum/p4c_backends/fpm/parser_decoder.h"
#include "stratum/p4c_backends/fpm/parser_map.pb.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/table_map_generator_mock.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "gtest/gtest.h"
#include "stratum/glue/gtl/map_util.h"

namespace stratum {
namespace p4c_backends {

using ::testing::AnyNumber;
using ::testing::_;

class ParserValueSetMapperTest : public ::testing::Test {
 protected:
  // Sets up TableMapGeneratorMock delegation to the real object.
  void SetUp() override {
    ON_CALL(mock_table_map_generator_, generated_map())
        .WillByDefault(::testing::Invoke(
            &real_table_map_generator_, &TableMapGenerator::generated_map));
  }

  // Reads an IR JSON file via IRTestHelperJson to set up P4 parser data
  // for test use.  Also populates some real_table_map_generator_ header
  // attributes.
  void SetUpIRParser(const std::string& ir_input_file);

  // Sets up field descriptor entries for the fields in the input vector.
  // The first entry is a UDP payload field expected to be on the right
  // side of UDF metadata assignments.  If tests do not want to setup this
  // field, they can specify an empty string in the first vector entry.
  // The remaining fields specify any expected local metadata fields on the
  // left side of assignments in parser states.
  void SetUpHeaderFields(const std::vector<std::string>& field_names);

  // Mocks injected to constructor of tested ParserValueSetMapper.  The
  // TableMapGeneratorMock delegates some calls to the real TableMapGenerator.
  hal::P4InfoManagerMock mock_p4_info_manager_;
  TableMapGeneratorMock mock_table_map_generator_;
  TableMapGenerator real_table_map_generator_;

  // SetUpIRParser runs a ParserDecoder to produce the test_parser_map_
  // representation of the P4 program's parser.  SetUpIRParser also initializes
  // the IR parser nodes and saves a pointer for test use in ir_parser_.
  // The IR owns the pointer.  The test_state_ pointer refers to the state
  // in test_parser_map_ that selects transitions based on value sets.
  ParserMap test_parser_map_;
  const IR::P4Parser* ir_parser_;
  ParserState* test_state_;

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Sets up an IR for tests.

  // ParserValueSetMapper under test.
  std::unique_ptr<ParserValueSetMapper> test_value_set_mapper_;
};

void ParserValueSetMapperTest::SetUpIRParser(const std::string& ir_input_file) {
  ir_helper_ = absl::make_unique<IRTestHelperJson>();
  const std::string ir_path = "stratum/p4c_backends/" +
      ir_input_file;
  CHECK(ir_helper_->GenerateTestIRAndInspectProgram(ir_path));
  CHECK_EQ(1, ir_helper_->program_inspector().parsers().size());
  ir_parser_ = ir_helper_->program_inspector().parsers()[0];

  // The ParserDecoder processes the ir_input_file's P4 parser to produce
  // the initial test_parser_map_ for ParserValueSetMapper input.
  ParserDecoder parser_decoder;
  bool result = parser_decoder.DecodeParser(*ir_parser_,
                                            ir_helper_->mid_end_refmap(),
                                            ir_helper_->mid_end_typemap());
  ASSERT_TRUE(result);
  test_parser_map_ = parser_decoder.parser_states();

  // This setup also creates a header descriptor entry for each
  // extracted header in the tested parser state.
  test_state_ = gtl::FindOrNull(
      *test_parser_map_.mutable_parser_states(), "parse_udf_payload");
  CHECK(test_state_ != nullptr);
  for (const auto& header_path :
      test_state_->extracted_header().header_paths()) {
    real_table_map_generator_.AddHeader(header_path);
    real_table_map_generator_.SetHeaderAttributes(
        header_path, P4_HEADER_UDP_PAYLOAD, 0);
  }
}

// Each value-set selected state typically has one assignment statement
// of the form:
//  meta.udf_N = hdr.udf_payload.last.udf_data;
// SetUpHeaderFields creates real_table_map_generator_ entries for the fields
// in these assignments.  The first field identifies the field on the right
// side of the assignment, which these tests define as a one-byte field of
// UDP payload data.  The first field is common to all value sets being tested.
// The remaining fields identify local metadata fields on the left side of the
// assignments in tested states, where one field should correspond to each value
// set.
void ParserValueSetMapperTest::SetUpHeaderFields(
    const std::vector<std::string>& field_names) {
  CHECK_LE(1, field_names.size());
  if (!field_names[0].empty()) {
    real_table_map_generator_.AddField(field_names[0]);
    real_table_map_generator_.SetFieldAttributes(
        field_names[0], P4_FIELD_TYPE_UDP_PAYLOAD_DATA,
        P4_HEADER_UDP_PAYLOAD, 0, 8);
  }
  for (const auto& meta_field : field_names) {
    if (meta_field == field_names[0]) continue;
    real_table_map_generator_.AddField(meta_field);
    real_table_map_generator_.SetFieldLocalMetadataFlag(meta_field);
  }
}

// Tests to see that normal Stratum value sets in the parser generate
// the expected field descriptor changes.
TEST_F(ParserValueSetMapperTest, TestNormalValueSet) {
  SetUpIRParser("fpm/testdata/parse_value_set.ir.json");

  // This test needs field descriptors for the payload field and the
  // local metadata UDF fields.
  SetUpHeaderFields(
      {"hdr.udf_payload.last.udf_data", "meta.udf_1", "meta.udf_2"});

  test_value_set_mapper_ = absl::make_unique<ParserValueSetMapper>(
      test_parser_map_, mock_p4_info_manager_, &mock_table_map_generator_);
  EXPECT_CALL(mock_table_map_generator_, generated_map()).Times(AnyNumber());
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(
      "meta.udf_1", "ParserImpl.udf_vs_1", P4_HEADER_UDP_PAYLOAD)).Times(1);
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(
      "meta.udf_2", "ParserImpl.udf_vs_2", P4_HEADER_UDP_PAYLOAD)).Times(1);
  EXPECT_TRUE(test_value_set_mapper_->MapValueSets(*ir_parser_));
}

// Verifies no output when the test_parser_map_ has no value set transitions.
TEST_F(ParserValueSetMapperTest, TestNoValueSetTransitions) {
  SetUpIRParser("fpm/testdata/parse_value_set.ir.json");
  SetUpHeaderFields(
      {"hdr.udf_payload.last.udf_data", "meta.udf_1", "meta.udf_2"});

  // This test clears the original cases in test_parser_map, replacing them
  // with a simple default case.
  auto parser_select = test_state_->mutable_transition()->mutable_select();
  parser_select->clear_cases();
  auto default_case = parser_select->add_cases();
  default_case->set_is_default(true);
  default_case->set_next_state("accept");

  test_value_set_mapper_ = absl::make_unique<ParserValueSetMapper>(
      test_parser_map_, mock_p4_info_manager_, &mock_table_map_generator_);
  EXPECT_CALL(mock_table_map_generator_, generated_map()).Times(AnyNumber());
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(_, _, _)).Times(0);
  EXPECT_TRUE(test_value_set_mapper_->MapValueSets(*ir_parser_));
}

// Verifies that the test_parser_map_ works with no extracted header paths.
TEST_F(ParserValueSetMapperTest, TestNoHeaderPaths) {
  SetUpIRParser("fpm/testdata/parse_value_set.ir.json");
  SetUpHeaderFields(
      {"hdr.udf_payload.last.udf_data", "meta.udf_1", "meta.udf_2"});
  test_state_->mutable_extracted_header()->clear_header_paths();

  test_value_set_mapper_ = absl::make_unique<ParserValueSetMapper>(
      test_parser_map_, mock_p4_info_manager_, &mock_table_map_generator_);
  EXPECT_CALL(mock_table_map_generator_, generated_map()).Times(AnyNumber());
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(
      "meta.udf_1", "ParserImpl.udf_vs_1", P4_HEADER_UDP_PAYLOAD)).Times(1);
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(
      "meta.udf_2", "ParserImpl.udf_vs_2", P4_HEADER_UDP_PAYLOAD)).Times(1);
  EXPECT_TRUE(test_value_set_mapper_->MapValueSets(*ir_parser_));
}

// Verifies no output when the source payload field has no descriptor.
TEST_F(ParserValueSetMapperTest, TestNoPayloadFieldDescriptor) {
  SetUpIRParser("fpm/testdata/parse_value_set.ir.json");
  SetUpHeaderFields({"", "meta.udf_1", "meta.udf_2"});  // First entry is empty.

  test_value_set_mapper_ = absl::make_unique<ParserValueSetMapper>(
      test_parser_map_, mock_p4_info_manager_, &mock_table_map_generator_);
  EXPECT_CALL(mock_table_map_generator_, generated_map()).Times(AnyNumber());
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(_, _, _)).Times(0);
  EXPECT_TRUE(test_value_set_mapper_->MapValueSets(*ir_parser_));
}

// Verifies no output for a target UDF metadata field that has no descriptor.
TEST_F(ParserValueSetMapperTest, TestNoUDFMetadataFieldDescriptor) {
  SetUpIRParser("fpm/testdata/parse_value_set.ir.json");
  SetUpHeaderFields({"hdr.udf_payload.last.udf_data", "meta.udf_2"});

  test_value_set_mapper_ = absl::make_unique<ParserValueSetMapper>(
      test_parser_map_, mock_p4_info_manager_, &mock_table_map_generator_);
  EXPECT_CALL(mock_table_map_generator_, generated_map()).Times(AnyNumber());
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet("meta.udf_1", _, _))
      .Times(0);
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(
      "meta.udf_2", "ParserImpl.udf_vs_2", P4_HEADER_UDP_PAYLOAD)).Times(1);
  EXPECT_TRUE(test_value_set_mapper_->MapValueSets(*ir_parser_));
}

// Verifies no output when the source payload field's table map entry
// is not a field descriptor.
TEST_F(ParserValueSetMapperTest, TestWrongPayloadDescriptorType) {
  SetUpIRParser("fpm/testdata/parse_value_set.ir.json");
  SetUpHeaderFields({"", "meta.udf_1", "meta.udf_2"});  // First entry is empty.

  // "hdr.udf_payload.last.udf_data" gets an action descriptor.
  real_table_map_generator_.AddAction("hdr.udf_payload.last.udf_data");

  test_value_set_mapper_ = absl::make_unique<ParserValueSetMapper>(
      test_parser_map_, mock_p4_info_manager_, &mock_table_map_generator_);
  EXPECT_CALL(mock_table_map_generator_, generated_map()).Times(AnyNumber());
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(_, _, _)).Times(0);
  EXPECT_TRUE(test_value_set_mapper_->MapValueSets(*ir_parser_));
}

// Verifies no output for a target UDF metadata field that has a
// non-field descriptor.
TEST_F(ParserValueSetMapperTest, TestWrongUDFMetadataDescriptorType) {
  SetUpIRParser("fpm/testdata/parse_value_set.ir.json");
  SetUpHeaderFields({"hdr.udf_payload.last.udf_data", "meta.udf_1"});

  // "meta.udf_2" gets a table descriptor.
  real_table_map_generator_.AddTable("meta.udf_2");

  test_value_set_mapper_ = absl::make_unique<ParserValueSetMapper>(
      test_parser_map_, mock_p4_info_manager_, &mock_table_map_generator_);
  EXPECT_CALL(mock_table_map_generator_, generated_map()).Times(AnyNumber());
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet("meta.udf_2", _, _))
      .Times(0);
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(
      "meta.udf_1", "ParserImpl.udf_vs_1", P4_HEADER_UDP_PAYLOAD)).Times(1);
  EXPECT_TRUE(test_value_set_mapper_->MapValueSets(*ir_parser_));
}

// Verifies no output when the source payload field's table map entry
// has an unknown field type.
TEST_F(ParserValueSetMapperTest, TestUnknownPayloadFieldType) {
  SetUpIRParser("fpm/testdata/parse_value_set.ir.json");

  // "hdr.udf_payload.last.udf_data" has P4_FIELD_TYPE_UNKNOWN.
  SetUpHeaderFields({"", "meta.udf_1", "meta.udf_2"});  // First entry is empty.
  real_table_map_generator_.AddField("hdr.udf_payload.last.udf_data");
  real_table_map_generator_.SetFieldAttributes(
      "hdr.udf_payload.last.udf_data", P4_FIELD_TYPE_UNKNOWN,
      P4_HEADER_UDP_PAYLOAD, 0, 8);

  test_value_set_mapper_ = absl::make_unique<ParserValueSetMapper>(
      test_parser_map_, mock_p4_info_manager_, &mock_table_map_generator_);
  EXPECT_CALL(mock_table_map_generator_, generated_map()).Times(AnyNumber());
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(_, _, _)).Times(0);
  EXPECT_TRUE(test_value_set_mapper_->MapValueSets(*ir_parser_));
}

// Verifies no output for a target UDF metadata field that has
// a previously specified field type.
TEST_F(ParserValueSetMapperTest, TestUDFMetadataFieldPriorType) {
  SetUpIRParser("fpm/testdata/parse_value_set.ir.json");

  // "meta.udf_1" is pre-assigned P4_FIELD_TYPE_ETH_SRC.
  SetUpHeaderFields({"hdr.udf_payload.last.udf_data", "meta.udf_2"});
  real_table_map_generator_.AddField("meta.udf_1");
  real_table_map_generator_.SetFieldType("meta.udf_1", P4_FIELD_TYPE_ETH_SRC);
  real_table_map_generator_.SetFieldLocalMetadataFlag("meta.udf_1");

  test_value_set_mapper_ = absl::make_unique<ParserValueSetMapper>(
      test_parser_map_, mock_p4_info_manager_, &mock_table_map_generator_);
  EXPECT_CALL(mock_table_map_generator_, generated_map()).Times(AnyNumber());
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet("meta.udf_1", _, _))
      .Times(0);
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(
      "meta.udf_2", "ParserImpl.udf_vs_2", P4_HEADER_UDP_PAYLOAD)).Times(1);
  EXPECT_TRUE(test_value_set_mapper_->MapValueSets(*ir_parser_));
}

// Verifies no output when the source payload field's table map entry's
// header type is unknown.
TEST_F(ParserValueSetMapperTest, TestUnknownPayloadHeaderType) {
  SetUpIRParser("fpm/testdata/parse_value_set.ir.json");

  // "hdr.udf_payload.last.udf_data" has P4_HEADER_TCP instead of
  // P4_HEADER_UDP_PAYLOAD.
  SetUpHeaderFields({"", "meta.udf_1", "meta.udf_2"});  // First entry is empty.
  real_table_map_generator_.AddField("hdr.udf_payload.last.udf_data");
  real_table_map_generator_.SetFieldAttributes(
      "hdr.udf_payload.last.udf_data", P4_FIELD_TYPE_UDP_PAYLOAD_DATA,
      P4_HEADER_UNKNOWN, 0, 8);

  test_value_set_mapper_ = absl::make_unique<ParserValueSetMapper>(
      test_parser_map_, mock_p4_info_manager_, &mock_table_map_generator_);
  EXPECT_CALL(mock_table_map_generator_, generated_map()).Times(AnyNumber());
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(_, _, _)).Times(0);
  EXPECT_TRUE(test_value_set_mapper_->MapValueSets(*ir_parser_));
}

// Verifies no output for a target field that is not local metadata.
TEST_F(ParserValueSetMapperTest, TestUDFFieldNotMetadata) {
  SetUpIRParser("fpm/testdata/parse_value_set.ir.json");

  // "meta.udf_1" is not set as local metadata.
  SetUpHeaderFields({"hdr.udf_payload.last.udf_data", "meta.udf_2"});
  real_table_map_generator_.AddField("meta.udf_1");

  test_value_set_mapper_ = absl::make_unique<ParserValueSetMapper>(
      test_parser_map_, mock_p4_info_manager_, &mock_table_map_generator_);
  EXPECT_CALL(mock_table_map_generator_, generated_map()).Times(AnyNumber());
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet("meta.udf_1", _, _))
      .Times(0);
  EXPECT_CALL(mock_table_map_generator_, SetFieldValueSet(
      "meta.udf_2", "ParserImpl.udf_vs_2", P4_HEADER_UDP_PAYLOAD)).Times(1);
  EXPECT_TRUE(test_value_set_mapper_->MapValueSets(*ir_parser_));
}

}  // namespace p4c_backends
}  // namespace stratum
