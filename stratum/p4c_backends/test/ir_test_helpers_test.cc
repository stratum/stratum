// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

#include "stratum/p4c_backends/test/ir_test_helpers.h"

#include <string>
#include "stratum/p4c_backends/fpm/utils.h"
#include "gtest/gtest.h"

namespace stratum {
namespace p4c_backends {

class IRTestHelperJsonTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    SetUpTestP4ModelNames();
  }

 protected:
  IRTestHelperJson helper_;  // Common IRTestHelperJson for testing.
  static char const* kTestJsonFile;
};

char const* IRTestHelperJsonTest::kTestJsonFile =
    "stratum/p4c_backends/test/testdata/"
    "simple_vlan_stack_16.ir.json";

// Tests expected normal behavior from successful JSON IR loads.
TEST_F(IRTestHelperJsonTest, TestJsonLoad) {
  EXPECT_TRUE(helper_.GenerateTestIR(kTestJsonFile));

  // These expectations do a sanity check on the generated IR.
  const std::string node_name =
      std::string(helper_.ir_top_level()->node_type_name());
  EXPECT_EQ("ToplevelBlock", node_name);
  EXPECT_TRUE(helper_.ir_top_level()->getProgram() != nullptr);
  EXPECT_TRUE(helper_.ir_top_level()->getMain() != nullptr);
  EXPECT_TRUE(helper_.mid_end_refmap() != nullptr);
  ASSERT_TRUE(helper_.mid_end_typemap() != nullptr);
  EXPECT_NE(0, helper_.mid_end_typemap()->size());
}

// Tests expected normal behavior from successful JSON IR loads followed by
// program inspection.
TEST_F(IRTestHelperJsonTest, TestJsonLoadAndInspect) {
  EXPECT_TRUE(helper_.GenerateTestIRAndInspectProgram(kTestJsonFile));

  // These expectations do a sanity check on the generated IR.
  const std::string node_name =
      std::string(helper_.ir_top_level()->node_type_name());
  EXPECT_EQ("ToplevelBlock", node_name);
  EXPECT_TRUE(helper_.ir_top_level()->getProgram() != nullptr);
  EXPECT_TRUE(helper_.ir_top_level()->getMain() != nullptr);
  EXPECT_TRUE(helper_.mid_end_refmap() != nullptr);
  ASSERT_TRUE(helper_.mid_end_typemap() != nullptr);
  EXPECT_NE(0, helper_.mid_end_typemap()->size());

  // These expectations do a sanity check on the inspected program output.
  EXPECT_FALSE(helper_.program_inspector().tables().empty());
  EXPECT_FALSE(helper_.program_inspector().match_keys().empty());
  EXPECT_FALSE(helper_.program_inspector().controls().empty());
  EXPECT_FALSE(helper_.program_inspector().actions().empty());
  EXPECT_FALSE(helper_.program_inspector().parsers().empty());
  EXPECT_NE(nullptr, helper_.GetP4Control("ingress"));
  EXPECT_NE(nullptr, helper_.GetP4Control("egress"));
}

// The next four tests expect failures for all combinations of GenerateTestIR
// and GenerateTestIRAndInspectProgram called repeatedly.
TEST_F(IRTestHelperJsonTest, TestJsonReload1) {
  EXPECT_TRUE(helper_.GenerateTestIR(kTestJsonFile));
  EXPECT_FALSE(helper_.GenerateTestIR(kTestJsonFile));
}

TEST_F(IRTestHelperJsonTest, TestJsonReload2) {
  EXPECT_TRUE(helper_.GenerateTestIRAndInspectProgram(kTestJsonFile));
  EXPECT_FALSE(helper_.GenerateTestIRAndInspectProgram(kTestJsonFile));
}

TEST_F(IRTestHelperJsonTest, TestJsonReload3) {
  EXPECT_TRUE(helper_.GenerateTestIR(kTestJsonFile));
  EXPECT_FALSE(helper_.GenerateTestIRAndInspectProgram(kTestJsonFile));
}

TEST_F(IRTestHelperJsonTest, TestJsonReload4) {
  EXPECT_TRUE(helper_.GenerateTestIRAndInspectProgram(kTestJsonFile));
  EXPECT_FALSE(helper_.GenerateTestIR(kTestJsonFile));
}

// Expects failure when the input JSON file does not exist.
TEST_F(IRTestHelperJsonTest, TestJsonFileMissing) {
  const std::string kTestBogusFile = "bogus.json";
  EXPECT_FALSE(helper_.GenerateTestIR(kTestBogusFile));
}

// Verifies normal production of P4Info for the generated IR.
TEST_F(IRTestHelperJsonTest, TestP4Info) {
  ASSERT_TRUE(helper_.GenerateTestIR(kTestJsonFile));

  ::p4::config::v1::P4Info p4_info;
  EXPECT_TRUE(helper_.GenerateP4Info(&p4_info));
  EXPECT_LT(0, p4_info.tables_size());
  EXPECT_LT(0, p4_info.actions_size());
}

// Expects failure when calling GenerateP4Info before GenerateTestIR.
TEST_F(IRTestHelperJsonTest, TestP4InfoNoIR) {
  ::p4::config::v1::P4Info p4_info;
  EXPECT_FALSE(helper_.GenerateP4Info(&p4_info));
}

// Tests TransformP4Control with an empty transform list.
TEST_F(IRTestHelperJsonTest, TestTransformEmptyList) {
  ASSERT_TRUE(helper_.GenerateTestIRAndInspectProgram(kTestJsonFile));
  const IR::P4Control* transformed_control =
      helper_.TransformP4Control("ingress", {});
  EXPECT_EQ(helper_.GetP4Control("ingress"), transformed_control);
}

// Tests TransformP4Control when the transform has no effect.
TEST_F(IRTestHelperJsonTest, TestTransformNop) {
  ASSERT_TRUE(helper_.GenerateTestIRAndInspectProgram(kTestJsonFile));
  helper_.set_color_field_name("no-color");
  const IR::P4Control* transformed_control = helper_.TransformP4Control(
      "ingress", {IRTestHelperJson::kMeterColorMapper});
  EXPECT_EQ(helper_.GetP4Control("ingress"), transformed_control);
}

// Tests TransformP4Control with MeterColorMapper doing a transform.
TEST_F(IRTestHelperJsonTest, TestTransformMeterColor) {
  const std::string kTestFile = "stratum/p4c_backends/"
      "fpm/testdata/meter_colors.ir.json";
  ASSERT_TRUE(helper_.GenerateTestIRAndInspectProgram(kTestFile));
  helper_.set_color_field_name("meta.enum_color");
  const IR::P4Control* transformed_control = helper_.TransformP4Control(
      "meter_if_green", {IRTestHelperJson::kMeterColorMapper});
  EXPECT_NE(helper_.GetP4Control("meter_if_green"), transformed_control);
  EXPECT_NE(nullptr, transformed_control);
}

// Tests TransformP4Control with MeterColorMapper finding a transform error.
TEST_F(IRTestHelperJsonTest, TestTransformMeterColorError) {
  const std::string kTestFile = "stratum/p4c_backends/"
      "fpm/testdata/meter_color_errors1.ir.json";
  ASSERT_TRUE(helper_.GenerateTestIRAndInspectProgram(kTestFile));
  helper_.set_color_field_name("meta.enum_color");
  const IR::P4Control* transformed_control = helper_.TransformP4Control(
      "meter_valid_after_unsupported", {IRTestHelperJson::kMeterColorMapper});
  EXPECT_EQ(nullptr, transformed_control);
}

// Tests TransformP4Control with HitAssignMapper doing a transform.
TEST_F(IRTestHelperJsonTest, TestTransformHitAssign) {
  const std::string kTestFile = "stratum/p4c_backends/"
      "fpm/testdata/hit_assign.ir.json";
  ASSERT_TRUE(helper_.GenerateTestIRAndInspectProgram(kTestFile));
  const IR::P4Control* transformed_control = helper_.TransformP4Control(
      "basic_hit", {IRTestHelperJson::kHitAssignMapper});
  EXPECT_NE(helper_.GetP4Control("basic_hit"), transformed_control);
  EXPECT_NE(nullptr, transformed_control);
}

// Tests TransformP4Control with an unknown control name.
TEST_F(IRTestHelperJsonTest, TestTransformUnknownControl) {
  ASSERT_TRUE(helper_.GenerateTestIRAndInspectProgram(kTestJsonFile));
  const IR::P4Control* transformed_control =
      helper_.TransformP4Control("unknown-control", {});
  EXPECT_EQ(nullptr, transformed_control);
}

}  // namespace p4c_backends
}  // namespace stratum
