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

// Contains unit tests for FieldDecoder.

#include "stratum/p4c_backends/fpm/field_decoder.h"

#include <memory>
#include <string>
#include <vector>
#include "stratum/hal/lib/p4/p4_pipeline_config.host.pb.h"
#include "stratum/p4c_backends/fpm/p4_model_names.host.pb.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

// This class is the FieldDecoder test fixture.
class FieldDecoderTest : public testing::Test {
 protected:
  void SetUp() override {
    SetUpTestP4ModelNames();
    p4_model_names_ = GetP4ModelNames();
    (*p4_model_names_.mutable_strip_path_prefixes())["hdr"] = 0;
    SetP4ModelNames(p4_model_names_);
    field_decoder_ = absl::make_unique<FieldDecoder>(&table_mapper_);

    // The default header_type_map_ setup mimics the output that
    // HeaderPathInspector would produce for the header_decode_basic.ir.json
    // test file.
    header_type_map_["ethernet"] = "ethernet_t";
    header_type_map_["ipv4"] = "ipv4_t";
    header_type_map_["standard_metadata"] = "standard_metadata_t";
  }

  // Reads an IR JSON file via IRTestHelperJson to set up headers and match
  // keys for test use.
  void SetUpIR(const std::string& ir_input_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string ir_path = "stratum/p4c_backends/" +
        ir_input_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(ir_path));
  }

  std::unique_ptr<FieldDecoder> field_decoder_;  // Common instance for tests.
  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
  TableMapGenerator table_mapper_;    // Injected to field_decoder_.
  P4ModelNames p4_model_names_;       // Copy of defaults for test customizing.

  // Provides a map to setup the input that FieldDecoder::ConvertHeaderFields
  // normally receives from a HeaderPathInspector.
  HeaderPathInspector::PathToHeaderTypeMap header_type_map_;
};

TEST_F(FieldDecoderTest, TestSimpleHeaderConversion) {
  p4_model_names_.set_local_metadata_type_name("metadata");
  SetP4ModelNames(p4_model_names_);
  SetUpIR("fpm/testdata/header_decode_basic.ir.json");
  ASSERT_LT(0, ir_helper_->program_inspector().struct_likes().size());
  ASSERT_LT(0, ir_helper_->program_inspector().header_types().size());
  field_decoder_->ConvertHeaderFields(
      ir_helper_->program_inspector().p4_typedefs(),
      ir_helper_->program_inspector().p4_enums(),
      ir_helper_->program_inspector().struct_likes(),
      ir_helper_->program_inspector().header_types(),
      header_type_map_);
  const hal::P4PipelineConfig& table_map = table_mapper_.generated_map();
  EXPECT_FALSE(table_map.table_map().empty());

  // The test verifies that a few representative fields from the P4 program
  // are in the output table map.
  EXPECT_TRUE(table_map.table_map().find("ethernet.dstAddr") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("ethernet.etherType") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("ipv4.diffserv") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("ipv4.fragOffset") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("ipv4.protocol") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("ipv4.srcAddr") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("ipv4.ttl") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("standard_metadata.drop") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("standard_metadata.egress_port") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("standard_metadata.egress_spec") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("standard_metadata.ingress_port") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("standard_metadata.packet_length") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("ethernet") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("ipv4") !=
              table_map.table_map().end());
}

TEST_F(FieldDecoderTest, TestExtractedFields) {
  p4_model_names_.set_local_metadata_type_name("metadata");
  SetP4ModelNames(p4_model_names_);
  SetUpIR("fpm/testdata/header_decode_basic.ir.json");
  ASSERT_LT(0, ir_helper_->program_inspector().struct_likes().size());
  ASSERT_LT(0, ir_helper_->program_inspector().header_types().size());
  field_decoder_->ConvertHeaderFields(
      ir_helper_->program_inspector().p4_typedefs(),
      ir_helper_->program_inspector().p4_enums(),
      ir_helper_->program_inspector().struct_likes(),
      ir_helper_->program_inspector().header_types(),
      header_type_map_);

  // The expected number of generated header types should be at least as
  // many as the header_types vector defines.  The struct_likes size is
  // less useful because it includes some internally generated structures,
  // bit it does act as an upper bound.
  const size_t kMinHeaderTypes =
      ir_helper_->program_inspector().header_types().size();
  const size_t kMaxHeaderTypes = kMinHeaderTypes +
      ir_helper_->program_inspector().struct_likes().size();
  const auto& fields_per_type = field_decoder_->extracted_fields_per_type();
  EXPECT_LE(kMinHeaderTypes, fields_per_type.size());
  EXPECT_GE(kMaxHeaderTypes, fields_per_type.size());

  // This test verifies that all entries in the extracted fields map define
  // a field name, a bit width, and one or more fully-qualified field names.
  for (auto iter : fields_per_type) {
    const auto& field_list = iter.second;
    EXPECT_FALSE(field_list.empty());
    for (const auto& field_iter : field_list) {
      // TODO(teverman): Metadata types are a problem for this test, although
      // they don't seem to cause any problems in tor.p4 compilations.
      if (iter.first == "routing_metadata_t")
        continue;
      EXPECT_FALSE(field_iter.name().empty());
      EXPECT_NE(0, field_iter.bit_width());
      ASSERT_LT(0, field_iter.full_field_names_size());
      EXPECT_NE(string::npos,
                field_iter.full_field_names(0).rfind(field_iter.name()));
    }
  }

  // This test also verifies the field map details for a representative header.
  // The fields should have the expected name and size, and the order is
  // also significant.
  auto iter = fields_per_type.find("ethernet_t");
  ASSERT_TRUE(iter != fields_per_type.end());
  const auto& fields = iter->second;
  ASSERT_EQ(3, fields.size());
  EXPECT_EQ("dstAddr", fields[0].name());
  EXPECT_EQ(48, fields[0].bit_width());
  EXPECT_EQ("srcAddr", fields[1].name());
  EXPECT_EQ(48, fields[1].bit_width());
  EXPECT_EQ("etherType", fields[2].name());
  EXPECT_EQ(16, fields[2].bit_width());
}

TEST_F(FieldDecoderTest, TestSimpleMatchKeyConversion) {
  p4_model_names_.set_local_metadata_type_name("metadata");
  SetP4ModelNames(p4_model_names_);
  SetUpIR("fpm/testdata/header_decode_basic.ir.json");
  ASSERT_LT(0, ir_helper_->program_inspector().struct_likes().size());
  ASSERT_LT(0, ir_helper_->program_inspector().header_types().size());
  ASSERT_LT(0, ir_helper_->program_inspector().match_keys().size());
  field_decoder_->ConvertHeaderFields(
      ir_helper_->program_inspector().p4_typedefs(),
      ir_helper_->program_inspector().p4_enums(),
      ir_helper_->program_inspector().struct_likes(),
      ir_helper_->program_inspector().header_types(),
      header_type_map_);
  field_decoder_->ConvertMatchKeys(
      ir_helper_->program_inspector().match_keys());
  const hal::P4PipelineConfig& table_map = table_mapper_.generated_map();
  EXPECT_FALSE(table_map.table_map().empty());

  // The match fields from the test P4 program's tables should have
  // these expected match conversions:
  //  "ethernet.dstAddr" - P4_CONVERT_TO_U64/EXACT.
  //  "ethernet.etherType" - P4_CONVERT_TO_U32/EXACT.
  //  "ipv4.dstAddr" - P4_CONVERT_TO_U32_AND_MASK/LPM.
  //  "ipv4.srcAddr" - P4_CONVERT_TO_U32_AND_MASK/TERNARY.
  auto iter1 = table_map.table_map().find("ethernet.dstAddr");
  ASSERT_TRUE(iter1 != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_descriptor1 =
      iter1->second.field_descriptor();
  ASSERT_EQ(1, field_descriptor1.valid_conversions().size());
  EXPECT_EQ(hal::P4FieldDescriptor::P4_CONVERT_TO_U64,
            field_descriptor1.valid_conversions(0).conversion());
  EXPECT_EQ(::p4::config::v1::MatchField::EXACT,
            field_descriptor1.valid_conversions(0).match_type());
  EXPECT_FALSE(field_descriptor1.is_local_metadata());

  auto iter2 = table_map.table_map().find("ethernet.etherType");
  ASSERT_TRUE(iter2 != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_descriptor2 =
      iter2->second.field_descriptor();
  ASSERT_EQ(1, field_descriptor2.valid_conversions().size());
  EXPECT_EQ(hal::P4FieldDescriptor::P4_CONVERT_TO_U32,
            field_descriptor2.valid_conversions(0).conversion());
  EXPECT_EQ(::p4::config::v1::MatchField::EXACT,
            field_descriptor2.valid_conversions(0).match_type());
  EXPECT_FALSE(field_descriptor2.is_local_metadata());

  auto iter3 = table_map.table_map().find("ipv4.dstAddr");
  ASSERT_TRUE(iter3 != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_descriptor3 =
      iter3->second.field_descriptor();
  ASSERT_EQ(1, field_descriptor3.valid_conversions().size());
  EXPECT_EQ(hal::P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK,
            field_descriptor3.valid_conversions(0).conversion());
  EXPECT_EQ(::p4::config::v1::MatchField::LPM,
            field_descriptor3.valid_conversions(0).match_type());
  EXPECT_FALSE(field_descriptor3.is_local_metadata());

  auto iter4 = table_map.table_map().find("ipv4.srcAddr");
  ASSERT_TRUE(iter4 != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_descriptor4 =
      iter4->second.field_descriptor();
  ASSERT_EQ(1, field_descriptor4.valid_conversions().size());
  EXPECT_EQ(hal::P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK,
            field_descriptor4.valid_conversions(0).conversion());
  EXPECT_EQ(::p4::config::v1::MatchField::TERNARY,
            field_descriptor4.valid_conversions(0).match_type());
  EXPECT_FALSE(field_descriptor4.is_local_metadata());
}

TEST_F(FieldDecoderTest, TestConvertHeaderStack) {
  SetUpIR("test/testdata/simple_vlan_stack_16.ir.json");
  ASSERT_LT(0, ir_helper_->program_inspector().struct_likes().size());
  ASSERT_LT(0, ir_helper_->program_inspector().header_types().size());

  // The test tunes header_type_map_ for the test IR file.
  header_type_map_["vlan_tag[0]"] = "vlan_tag_t";
  header_type_map_["vlan_tag[1]"] = "vlan_tag_t";
  header_type_map_["vlan_tag[2]"] = "vlan_tag_t";
  header_type_map_["vlan_tag[3]"] = "vlan_tag_t";
  header_type_map_["vlan_tag[4]"] = "vlan_tag_t";

  field_decoder_->ConvertHeaderFields(
      ir_helper_->program_inspector().p4_typedefs(),
      ir_helper_->program_inspector().p4_enums(),
      ir_helper_->program_inspector().struct_likes(),
      ir_helper_->program_inspector().header_types(),
      header_type_map_);
  const hal::P4PipelineConfig& table_map = table_mapper_.generated_map();
  EXPECT_FALSE(table_map.table_map().empty());

  // The test input has a stack of 5 VLAN tags.  For simplicity, the test
  // covers only one field at each stack depth.
  EXPECT_TRUE(table_map.table_map().find("vlan_tag[0].vlan_id") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("vlan_tag[1].cfi") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("vlan_tag[2].ethertype") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("vlan_tag[3].pcp") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("vlan_tag[4].vlan_id") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("vlan_tag[0]") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("vlan_tag[4]") !=
              table_map.table_map().end());
}

TEST_F(FieldDecoderTest, TestConvertTypedefField) {
  SetUpIR("test/testdata/simple_vlan_stack_16.ir.json");
  ASSERT_LT(0, ir_helper_->program_inspector().p4_typedefs().size());
  ASSERT_LT(0, ir_helper_->program_inspector().struct_likes().size());
  ASSERT_LT(0, ir_helper_->program_inspector().header_types().size());
  field_decoder_->ConvertHeaderFields(
      ir_helper_->program_inspector().p4_typedefs(),
      ir_helper_->program_inspector().p4_enums(),
      ir_helper_->program_inspector().struct_likes(),
      ir_helper_->program_inspector().header_types(),
      header_type_map_);
  const hal::P4PipelineConfig& table_map = table_mapper_.generated_map();
  EXPECT_FALSE(table_map.table_map().empty());

  // The ethernet_t header uses a typedef for the source and destination
  // addresses.  Both fields should be present in table_map.
  EXPECT_TRUE(table_map.table_map().find("ethernet.srcAddr") !=
              table_map.table_map().end());
  EXPECT_TRUE(table_map.table_map().find("ethernet.dstAddr") !=
              table_map.table_map().end());
}

TEST_F(FieldDecoderTest, TestConvertControllerHeaders) {
  p4_model_names_.set_local_metadata_type_name("test_metadata_t");
  SetP4ModelNames(p4_model_names_);
  SetUpIR("fpm/testdata/header_decode_controller.ir.json");
  ASSERT_LT(0, ir_helper_->program_inspector().struct_likes().size());
  ASSERT_LT(0, ir_helper_->program_inspector().header_types().size());
  field_decoder_->ConvertHeaderFields(
      ir_helper_->program_inspector().p4_typedefs(),
      ir_helper_->program_inspector().p4_enums(),
      ir_helper_->program_inspector().struct_likes(),
      ir_helper_->program_inspector().header_types(),
      header_type_map_);
  const hal::P4PipelineConfig& table_map = table_mapper_.generated_map();
  EXPECT_FALSE(table_map.table_map().empty());

  // The controller packet IO metadata fields should be present in the
  // table map with the applicable "packet_in" or "packet_out" prefix.
  auto iter = table_map.table_map().find("packet_in.ingress_logical_port");
  ASSERT_TRUE(iter != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_desc1 = iter->second.field_descriptor();
  EXPECT_EQ(P4_FIELD_TYPE_INGRESS_TRUNK, field_desc1.type());
  iter = table_map.table_map().find("packet_in.ingress_physical_port");
  ASSERT_TRUE(iter != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_desc2 = iter->second.field_descriptor();
  EXPECT_EQ(P4_FIELD_TYPE_INGRESS_PORT, field_desc2.type());
  iter = table_map.table_map().find("packet_in.target_egress_port");
  ASSERT_TRUE(iter != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_desc3 = iter->second.field_descriptor();
  EXPECT_EQ(P4_FIELD_TYPE_EGRESS_PORT, field_desc3.type());
  iter = table_map.table_map().find("packet_out.egress_physical_port");
  ASSERT_TRUE(iter != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_desc4 = iter->second.field_descriptor();
  EXPECT_EQ(P4_FIELD_TYPE_EGRESS_PORT, field_desc4.type());
  iter = table_map.table_map().find("packet_out.submit_to_ingress");
  ASSERT_TRUE(iter != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_desc5 = iter->second.field_descriptor();
  EXPECT_EQ(P4_FIELD_TYPE_ANNOTATED, field_desc5.type());
}

TEST_F(FieldDecoderTest, TestConvertEnums) {
  p4_model_names_.set_local_metadata_type_name("test_metadata_t");
  SetP4ModelNames(p4_model_names_);
  SetUpIR("fpm/testdata/header_decode_advanced.ir.json");
  ASSERT_LT(0, ir_helper_->program_inspector().struct_likes().size());
  ASSERT_LT(0, ir_helper_->program_inspector().header_types().size());
  ASSERT_LT(0, ir_helper_->program_inspector().p4_enums().size());
  header_type_map_["meta"] = "test_metadata_t";
  field_decoder_->ConvertHeaderFields(
      ir_helper_->program_inspector().p4_typedefs(),
      ir_helper_->program_inspector().p4_enums(),
      ir_helper_->program_inspector().struct_likes(),
      ir_helper_->program_inspector().header_types(),
      header_type_map_);
  const hal::P4PipelineConfig& table_map = table_mapper_.generated_map();
  EXPECT_FALSE(table_map.table_map().empty());
  auto iter = table_map.table_map().find("meta.enum_color");
  ASSERT_TRUE(iter != table_map.table_map().end());
}

TEST_F(FieldDecoderTest, TestLocalMetadataConversion) {
  p4_model_names_.set_local_metadata_type_name("test_metadata_t");
  SetP4ModelNames(p4_model_names_);
  SetUpIR("fpm/testdata/header_decode_advanced.ir.json");
  ASSERT_LT(0, ir_helper_->program_inspector().struct_likes().size());
  ASSERT_LT(0, ir_helper_->program_inspector().header_types().size());
  ASSERT_LT(0, ir_helper_->program_inspector().p4_enums().size());
  header_type_map_["meta"] = "test_metadata_t";
  field_decoder_->ConvertHeaderFields(
      ir_helper_->program_inspector().p4_typedefs(),
      ir_helper_->program_inspector().p4_enums(),
      ir_helper_->program_inspector().struct_likes(),
      ir_helper_->program_inspector().header_types(),
      header_type_map_);
  const hal::P4PipelineConfig& table_map = table_mapper_.generated_map();
  EXPECT_FALSE(table_map.table_map().empty());

  // The test verifies that local metadata fields from the P4 program
  // are in the output table map with the local metadata flag set.
  auto iter = table_map.table_map().find("meta.color");
  ASSERT_TRUE(iter != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_desc1 = iter->second.field_descriptor();
  EXPECT_TRUE(field_desc1.is_local_metadata());
  iter = table_map.table_map().find("meta.enum_color");
  ASSERT_TRUE(iter != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_desc2 = iter->second.field_descriptor();
  EXPECT_TRUE(field_desc2.is_local_metadata());
  iter = table_map.table_map().find("meta.other_metadata");
  ASSERT_TRUE(iter != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_desc3 = iter->second.field_descriptor();
  EXPECT_TRUE(field_desc3.is_local_metadata());
  iter = table_map.table_map().find("meta.smaller_metadata");
  ASSERT_TRUE(iter != table_map.table_map().end());
  const hal::P4FieldDescriptor& field_desc4 = iter->second.field_descriptor();
  EXPECT_TRUE(field_desc4.is_local_metadata());
}

TEST_F(FieldDecoderTest, TestConvertHeadersTwice) {
  // The first conversion is done with empty inputs to produce an
  // empty table map.
  std::vector<const IR::Type_Typedef*> empty_p4_typedefs;
  std::vector<const IR::Type_Enum*> empty_p4_enums;
  std::vector<const IR::Type_StructLike*> empty_struct_likes;
  std::vector<const IR::Type_Header*> empty_header_types;
  HeaderPathInspector::PathToHeaderTypeMap empty_header_map;
  field_decoder_->ConvertHeaderFields(empty_p4_typedefs, empty_p4_enums,
                                      empty_struct_likes,
                                      empty_header_types, empty_header_map);
  const hal::P4PipelineConfig& table_map = table_mapper_.generated_map();
  EXPECT_TRUE(table_map.table_map().empty());

  // The second conversion attempt uses real data from the test IR,
  // but ConvertHeaderFields should detect the repeat attempt and return
  // without producing output.
  p4_model_names_.set_local_metadata_type_name("metadata");
  SetP4ModelNames(p4_model_names_);
  SetUpIR("fpm/testdata/header_decode_basic.ir.json");
  ASSERT_LT(0, ir_helper_->program_inspector().struct_likes().size());
  ASSERT_LT(0, ir_helper_->program_inspector().header_types().size());
  field_decoder_->ConvertHeaderFields(
      ir_helper_->program_inspector().p4_typedefs(),
      ir_helper_->program_inspector().p4_enums(),
      ir_helper_->program_inspector().struct_likes(),
      ir_helper_->program_inspector().header_types(),
      header_type_map_);
  EXPECT_TRUE(table_map.table_map().empty());
}

TEST_F(FieldDecoderTest, TestConvertMatchKeysBeforeHeaders) {
  // An empty match key input is OK for this test.
  std::vector<const IR::KeyElement*> empty_match_keys;
  field_decoder_->ConvertMatchKeys(empty_match_keys);
  const hal::P4PipelineConfig& table_map = table_mapper_.generated_map();
  EXPECT_TRUE(table_map.table_map().empty());
}

TEST_F(FieldDecoderTest, TestConvertMatchKeysTwice) {
  p4_model_names_.set_local_metadata_type_name("metadata");
  SetP4ModelNames(p4_model_names_);
  SetUpIR("fpm/testdata/header_decode_basic.ir.json");
  field_decoder_->ConvertHeaderFields(
      ir_helper_->program_inspector().p4_typedefs(),
      ir_helper_->program_inspector().p4_enums(),
      ir_helper_->program_inspector().struct_likes(),
      ir_helper_->program_inspector().header_types(),
      header_type_map_);

  // The first match key conversion occurs with empty headers just to
  // establish that the function was called.
  std::vector<const IR::KeyElement*> empty_match_keys;
  field_decoder_->ConvertMatchKeys(empty_match_keys);

  // The second conversion attempt uses real data from the test IR,
  // but ConvertMatchKeys should detect the repeat attempt and return
  // without producing output.  MessageDifferencer doesn't do well with proto
  // maps, so this test expects one of the match fields to not have any
  // added conversions.
  field_decoder_->ConvertMatchKeys(
      ir_helper_->program_inspector().match_keys());
  const hal::P4PipelineConfig& table_map = table_mapper_.generated_map();
  auto iter = table_map.table_map().find("ethernet.dstAddr");
  ASSERT_TRUE(iter != table_map.table_map().end());
  EXPECT_EQ(0, iter->second.field_descriptor().valid_conversions().size());
}

}  // namespace p4c_backends
}  // namespace stratum
