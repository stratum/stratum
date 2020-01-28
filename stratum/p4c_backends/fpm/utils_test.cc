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

#include "stratum/p4c_backends/fpm/utils.h"

#include <memory>
#include <string>
#include <vector>

#include "google/protobuf/util/message_differencer.h"
#include "stratum/hal/lib/p4/p4_info_manager_mock.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/p4_model_names.pb.h"
#include "stratum/p4c_backends/fpm/parser_map.pb.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/target_info_mock.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"
#include "external/com_github_p4lang_p4c/lib/compile_context.h"
#include "external/com_github_p4lang_p4c/lib/cstring.h"

using ::google::protobuf::util::MessageDifferencer;
using ::testing::_;
using ::testing::Return;

namespace stratum {
namespace p4c_backends {

// This test fixture verifies the p4c utility functions.
class P4cUtilsTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    target_info_mock_ = new TargetInfoMock;
    TargetInfo::InjectSingleton(target_info_mock_);
  }
  static void TearDownTestCase() {
    TargetInfo::InjectSingleton(nullptr);
    delete target_info_mock_;
    target_info_mock_ = nullptr;
  }

 protected:
  P4cUtilsTest()
      : test_p4c_context_(new P4CContextWithOptions<CompilerOptions>) {
    // These test_p4_table_ values are returned by mock_p4_info_manager_.
    test_p4_table_.mutable_preamble()->set_name("test-table");
    test_p4_table_.mutable_preamble()->set_id(1);
  }

  // The SetUpTestIR method loads an IR file in JSON format, then applies a
  // ProgramInspector to record IR nodes that contain some P4Control methods
  // to test.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
  }

  // Tests often use an annotated IR::Type_Struct as the annotated node since
  // they are easy to construct without a full set of surrounding IR nodes.
  // The ir_node_ is constructed with any annotations that have been added
  // to ir_annotations_ before calling SetUpAnnotatedIRNode.
  void SetUpAnnotatedIRNode() {
    ir_node_ = absl::make_unique<IR::Type_Struct>(
        IR::ID("dummy-node"), &ir_annotations_);
  }

  // For a few tests that specifically need a more complex IR::P4Table node,
  // SetUpIRTable creates a table, which has the ID of the table name in
  // test_p4_table_, an empty set of properties, and whatever annotations
  // currently exist in ir_annotations_.
  void SetUpIRTable() {
    cstring tblName = IR::ID(test_p4_table_.preamble().name());
    ir_table_ = absl::make_unique<IR::P4Table>(
        tblName, &ir_annotations_, &empty_properties_);
  }

  // Adds an IR::Annotation string to ir_annotations_.  In this P4 annotation:
  // @switchstack(pipeline_stage: L3_LPM)
  // The id_name is "switchstack" and the literal_value is
  // "pipeline_stage: L3_LPM".
  void AddStringAnnotation(const cstring& id_name,
                           const cstring& literal_value) {
    ir_annotations_.add(new IR::Annotation(IR::ID(id_name), {
        new IR::StringLiteral(new IR::Type_String, literal_value)}));
  }

  // Creates a new IR P4Control node for testing.  The new control's parameters
  // come from param_list.  All other control attributes come from old_control.
  std::unique_ptr<IR::P4Control> CreateControlWithParams(
      const IR::P4Control& old_control,
      const std::vector<const IR::Parameter*>& param_list) {
    for (auto param : param_list) {
      new_control_params_.push_back(param);
    }
    new_param_list_ = absl::make_unique<IR::ParameterList>(new_control_params_);
    new_control_type_ = absl::make_unique<IR::Type_Control>(
        old_control.type->name, new_param_list_.get());
    return absl::make_unique<IR::P4Control>(
            old_control.name, new_control_type_.get(), old_control.body);
  }

  // This IR node is for common test use.
  std::unique_ptr<IR::Type_Struct> ir_node_;

  // Some tests specifically need an IR table node.
  std::unique_ptr<IR::P4Table> ir_table_;

  // These annotations are added to ir_node_ in its constructor.
  // They can also be used to construct an IR::P4Table.
  IR::Annotations ir_annotations_;

  // Table propterties are not important to any tests.
  IR::TableProperties empty_properties_;

  // Tests can store GetSwitchStackAnnotation output here.
  P4Annotation annotation_out_;

  // Tests can use this member to check for empty output messages.
  P4Annotation annotation_none_;

  // Tests can store FillTableRefByName/FillTableRefFromIR output here.
  hal::P4ControlTableRef table_ref_;

  // The mock P4InfoManager is passed to the FillTableRefXXX functions.  The
  // test_p4_table_ member provides tests with a mock return value from
  // P4InfoManager::FindTableByName.
  hal::P4InfoManagerMock mock_p4_info_manager_;
  ::p4::config::v1::Table test_p4_table_;

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.

  // CreateControlWithParams stores its internally populated IR nodes in
  // these members to keep them in scope for the duration of the test.
  IR::IndexedVector<IR::Parameter> new_control_params_;
  std::unique_ptr<IR::ParameterList> new_param_list_;
  std::unique_ptr<IR::Type_Control> new_control_type_;

  // This test uses its own p4c context for the tests that don't use the one
  // provided by IRTestHelperJson.
  AutoCompileContext test_p4c_context_;

  // The TableMapGenerator facilitates easy setup of P4PipelineConfig data.
  TableMapGenerator table_map_generator_;

  static TargetInfoMock* target_info_mock_;
};

TargetInfoMock* P4cUtilsTest::target_info_mock_ = nullptr;

TEST_F(P4cUtilsTest, TestNoAnnotations) {
  SetUpAnnotatedIRNode();
  EXPECT_FALSE(GetSwitchStackAnnotation(*ir_node_, &annotation_out_));
  EXPECT_TRUE(MessageDifferencer::Equals(annotation_none_, annotation_out_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a single normal pipeline_stage annotation.
TEST_F(P4cUtilsTest, TestPipelineStageAnnotation) {
  AddStringAnnotation("switchstack", "pipeline_stage: L3_LPM");
  SetUpAnnotatedIRNode();
  EXPECT_TRUE(GetSwitchStackAnnotation(*ir_node_, &annotation_out_));
  EXPECT_EQ(P4Annotation::L3_LPM, annotation_out_.pipeline_stage());
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a single normal field_type annotation.
TEST_F(P4cUtilsTest, TestFieldTypeAnnotation) {
  AddStringAnnotation("switchstack", "field_type: P4_FIELD_TYPE_VRF");
  SetUpAnnotatedIRNode();
  EXPECT_TRUE(GetSwitchStackAnnotation(*ir_node_, &annotation_out_));
  EXPECT_EQ(P4_FIELD_TYPE_VRF, annotation_out_.field_type());
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests finding a "switchstack" annotation among other node annotations.
TEST_F(P4cUtilsTest, TestMixedAnnotations) {
  AddStringAnnotation("name1", "literal1");
  AddStringAnnotation("name2", "literal2");
  AddStringAnnotation("switchstack", "pipeline_stage: INGRESS_ACL");
  SetUpAnnotatedIRNode();
  EXPECT_TRUE(GetSwitchStackAnnotation(*ir_node_, &annotation_out_));
  EXPECT_EQ(P4Annotation::INGRESS_ACL, annotation_out_.pipeline_stage());
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests multiple "switchstack" annotations for different P4Annotation fields.
TEST_F(P4cUtilsTest, TestMultipleAnnotations) {
  AddStringAnnotation("switchstack", "pipeline_stage: L2");
  AddStringAnnotation("switchstack", "field_type: P4_FIELD_TYPE_EGRESS_PORT");
  SetUpAnnotatedIRNode();
  EXPECT_TRUE(GetSwitchStackAnnotation(*ir_node_, &annotation_out_));
  EXPECT_EQ(P4Annotation::L2, annotation_out_.pipeline_stage());
  EXPECT_EQ(P4_FIELD_TYPE_EGRESS_PORT, annotation_out_.field_type());
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests valid pipeline_stage annotation with invalid field_type.
TEST_F(P4cUtilsTest, TestValidStageInvalidFieldType) {
  AddStringAnnotation("switchstack", "pipeline_stage: L2");
  AddStringAnnotation("switchstack", "field_type: BOGUS_FIELD");
  SetUpAnnotatedIRNode();
  EXPECT_FALSE(GetSwitchStackAnnotation(*ir_node_, &annotation_out_));
  EXPECT_TRUE(MessageDifferencer::Equals(annotation_none_, annotation_out_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests invalid pipeline_stage annotation with valid field_type.
TEST_F(P4cUtilsTest, TestInvalidStageValidFieldType) {
  AddStringAnnotation("switchstack", "pipeline_stage: BOGUS_STAGE");
  AddStringAnnotation("switchstack", "field_type: P4_FIELD_TYPE_CLASS_ID");
  SetUpAnnotatedIRNode();
  EXPECT_FALSE(GetSwitchStackAnnotation(*ir_node_, &annotation_out_));
  EXPECT_TRUE(MessageDifferencer::Equals(annotation_none_, annotation_out_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a pipeline_stage annotation that won't parse.
TEST_F(P4cUtilsTest, TestBogusPipelineStageAnnotation) {
  AddStringAnnotation("switchstack", "pipeline_stage: BOGUS");
  SetUpAnnotatedIRNode();
  EXPECT_FALSE(GetSwitchStackAnnotation(*ir_node_, &annotation_out_));
  EXPECT_TRUE(MessageDifferencer::Equals(annotation_none_, annotation_out_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a field_type annotation that won't parse.
TEST_F(P4cUtilsTest, TestBogusFieldTypeAnnotation) {
  AddStringAnnotation("switchstack", "field_type: BOGUS_FIELD");
  SetUpAnnotatedIRNode();
  EXPECT_FALSE(GetSwitchStackAnnotation(*ir_node_, &annotation_out_));
  EXPECT_TRUE(MessageDifferencer::Equals(annotation_none_, annotation_out_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a "switchstack" annotation that is not an IR::StringLiteral.
TEST_F(P4cUtilsTest, TestNonLiteralAnnotation) {
  ir_annotations_.add(new IR::Annotation(IR::ID("switchstack"), {
      new IR::BoolLiteral(true)}));
  SetUpAnnotatedIRNode();
  EXPECT_FALSE(GetSwitchStackAnnotation(*ir_node_, &annotation_out_));
  EXPECT_TRUE(MessageDifferencer::Equals(annotation_none_, annotation_out_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// The type below reuses P4cUtilsTest for testing
// GetAnnotatedPipelineStage and GetAnnotatedPipelineStageOrP4Error.
typedef P4cUtilsTest GetAnnotatedPipelineStageTest;

TEST_F(GetAnnotatedPipelineStageTest, TestNoAnnotations) {
  SetUpAnnotatedIRNode();
  EXPECT_EQ(P4Annotation::DEFAULT_STAGE, GetAnnotatedPipelineStage(*ir_node_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a single normal pipeline_stage annotation.
TEST_F(GetAnnotatedPipelineStageTest, TestPipelineStageAnnotation) {
  AddStringAnnotation("switchstack", "pipeline_stage: L3_LPM");
  SetUpAnnotatedIRNode();
  EXPECT_EQ(P4Annotation::L3_LPM, GetAnnotatedPipelineStage(*ir_node_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a pipeline_stage annotation that won't parse.
TEST_F(GetAnnotatedPipelineStageTest, TestBogusPipelineStageAnnotation) {
  AddStringAnnotation("switchstack", "pipeline_stage: BOGUS_STAGE");
  SetUpAnnotatedIRNode();
  EXPECT_EQ(P4Annotation::DEFAULT_STAGE, GetAnnotatedPipelineStage(*ir_node_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

TEST_F(GetAnnotatedPipelineStageTest, TestNoAnnotationsError) {
  SetUpIRTable();
  EXPECT_EQ(P4Annotation::DEFAULT_STAGE,
            GetAnnotatedPipelineStageOrP4Error(*ir_table_));
  EXPECT_NE(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a single normal pipeline_stage annotation.
TEST_F(GetAnnotatedPipelineStageTest, TestPipelineStageAnnotationNoError) {
  AddStringAnnotation("switchstack", "pipeline_stage: L3_LPM");
  SetUpIRTable();
  EXPECT_EQ(P4Annotation::L3_LPM,
            GetAnnotatedPipelineStageOrP4Error(*ir_table_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a pipeline_stage annotation that won't parse.
TEST_F(GetAnnotatedPipelineStageTest, TestBogusPipelineStageAnnotationError) {
  AddStringAnnotation("switchstack", "pipeline_stage: BOGUS_STAGE");
  SetUpIRTable();
  EXPECT_EQ(P4Annotation::DEFAULT_STAGE,
            GetAnnotatedPipelineStageOrP4Error(*ir_table_));
  EXPECT_NE(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a normal @controller_header("packet_in") annotation.
TEST_F(P4cUtilsTest, TestGetControllerHeaderPacketInAnnotation) {
  const std::string packet_in_value = "packet_in";
  AddStringAnnotation("controller_header", packet_in_value);
  SetUpAnnotatedIRNode();
  EXPECT_EQ(packet_in_value, GetControllerHeaderAnnotation(*ir_node_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a normal @controller_header("packet_out") annotation.
TEST_F(P4cUtilsTest, TestGetControllerHeaderPacketOutAnnotation) {
  const std::string packet_out_value = "packet_out";
  AddStringAnnotation("controller_header", packet_out_value);
  SetUpAnnotatedIRNode();
  EXPECT_EQ(packet_out_value, GetControllerHeaderAnnotation(*ir_node_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests GetControllerHeaderAnnotation without a @controller_header annotation.
TEST_F(P4cUtilsTest, TestGetControllerHeaderAnnotationNone) {
  SetUpAnnotatedIRNode();
  const std::string expected_value = "";
  EXPECT_EQ(expected_value, GetControllerHeaderAnnotation(*ir_node_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a @controller_header annotation that is not an IR::StringLiteral.
TEST_F(P4cUtilsTest, TestNonLiteralControllerHeaderAnnotation) {
  ir_annotations_.add(new IR::Annotation(IR::ID("controller_header"), {
      new IR::BoolLiteral(true)}));
  SetUpAnnotatedIRNode();
  const std::string expected_value = "";
  EXPECT_EQ(expected_value, GetControllerHeaderAnnotation(*ir_node_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests multiple @controller_header annotations in one node.
TEST_F(P4cUtilsTest, TestMultipleControllerHeaderAnnotations) {
  AddStringAnnotation("controller_header", "packet_in");
  AddStringAnnotation("controller_header", "packet_in2");
  SetUpAnnotatedIRNode();
  const std::string expected_value = "";
  EXPECT_EQ(expected_value, GetControllerHeaderAnnotation(*ir_node_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests FillTableRefByName.
TEST_F(P4cUtilsTest, TestFillTableRefByName) {
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
      .WillOnce(Return(test_p4_table_));
  FillTableRefByName(test_p4_table_.preamble().name(),
                     mock_p4_info_manager_, &table_ref_);
  EXPECT_EQ(test_p4_table_.preamble().name(), table_ref_.table_name());
  EXPECT_EQ(test_p4_table_.preamble().id(), table_ref_.table_id());
  EXPECT_EQ(P4Annotation::DEFAULT_STAGE, table_ref_.pipeline_stage());
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests FillTableRefFromIR with no table annotation.
TEST_F(P4cUtilsTest, TestFillTableRefFromIRNotAnnotated) {
  SetUpIRTable();
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
      .WillOnce(Return(test_p4_table_));
  FillTableRefFromIR(*ir_table_, mock_p4_info_manager_, &table_ref_);
  EXPECT_EQ(test_p4_table_.preamble().name(), table_ref_.table_name());
  EXPECT_EQ(test_p4_table_.preamble().id(), table_ref_.table_id());
  EXPECT_EQ(P4Annotation::DEFAULT_STAGE, table_ref_.pipeline_stage());
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests FillTableRefFromIR with table annotation.
TEST_F(P4cUtilsTest, TestFillTableRefFromIRAnnotated) {
  AddStringAnnotation("switchstack", "pipeline_stage: L3_LPM");
  SetUpIRTable();
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
      .WillOnce(Return(test_p4_table_));
  FillTableRefFromIR(*ir_table_, mock_p4_info_manager_, &table_ref_);
  EXPECT_EQ(test_p4_table_.preamble().name(), table_ref_.table_name());
  EXPECT_EQ(test_p4_table_.preamble().id(), table_ref_.table_id());
  EXPECT_EQ(P4Annotation::L3_LPM, table_ref_.pipeline_stage());
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests FillTableRefFromIR with table @name annotation.
// See P4 Spec 18.2.3 Control-plane API annotations
TEST_F(P4cUtilsTest, TestFillTableRefFromIRNameAnnotated) {
  const std::string kOverrideTableName = "tableName";
  // Override with fully-qualified name (starts with ".").
  AddStringAnnotation("name", "." + kOverrideTableName);
  SetUpIRTable();
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
      .WillOnce(Return(test_p4_table_));
  FillTableRefFromIR(*ir_table_, mock_p4_info_manager_, &table_ref_);
  // Make sure the leading dot is stripped.
  EXPECT_EQ(kOverrideTableName, table_ref_.table_name());
  EXPECT_EQ(test_p4_table_.preamble().id(), table_ref_.table_id());
  EXPECT_EQ(P4Annotation::DEFAULT_STAGE, table_ref_.pipeline_stage());
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

// Tests a node that doesn't support annotations.
TEST_F(P4cUtilsTest, TestUnannotatedNode) {
  // IR::BoolLiteral is not an IAnnotated subclass.
  std::unique_ptr<IR::BoolLiteral> test_node(new IR::BoolLiteral(true));
  const std::string expected_value = "";
  EXPECT_EQ(expected_value, GetControllerHeaderAnnotation(*test_node.get()));
  EXPECT_FALSE(GetSwitchStackAnnotation(*test_node.get(), &annotation_out_));
  EXPECT_TRUE(MessageDifferencer::Equals(annotation_none_, annotation_out_));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

TEST_F(P4cUtilsTest, TestIsPipelineStageFixed) {
  EXPECT_CALL(*target_info_mock_, IsPipelineStageFixed(P4Annotation::L2))
      .WillOnce(Return(true));
  EXPECT_TRUE(IsPipelineStageFixed(P4Annotation::L2));
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

TEST_F(P4cUtilsTest, TestIsTableApplyInstance) {
  // TODO(unknown): There is no obvious public way to create a
  // P4::MethodInstance for direct unit tests of IsTableApplyInstance, but
  // it gets reasonable indirect coverage from the pipeline pass tests.
}

TEST_F(P4cUtilsTest, TestFindLocalMetadataType) {
  // SetUpTestIR("tor_p4.ir.json"); // Google only
  SetUpTestIR("field_inspect_test.ir.json");
  P4ModelNames model_names;
  model_names.set_ingress_control_name("ingress");
  model_names.set_egress_control_name("egress");
  FindLocalMetadataType(
      ir_helper_->program_inspector().controls(), &model_names);
  EXPECT_EQ("local_metadata_t", model_names.local_metadata_type_name());
  EXPECT_EQ(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

TEST_F(P4cUtilsTest, TestFindLocalMetadataTypeWrongArgCount) {
  SetUpTestIR("action_misc.ir.json");
  P4ModelNames model_names;

  // This test uses the verify checksum control as the egress control in order
  // to test a control with an unexpected number of arguments.
  model_names.set_ingress_control_name("ingress");
  model_names.set_egress_control_name("verify_checksum_stub");
  FindLocalMetadataType(
      ir_helper_->program_inspector().controls(), &model_names);
  EXPECT_TRUE(model_names.local_metadata_type_name().empty());
  EXPECT_NE(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

TEST_F(P4cUtilsTest, TestFindLocalMetadataDifferentTypes) {
  SetUpTestIR("action_misc.ir.json");
  P4ModelNames model_names;
  model_names.set_ingress_control_name("ingress");
  model_names.set_egress_control_name("egress_stub");
  const IR::P4Control* ingress_control = ir_helper_->GetP4Control("ingress");
  ASSERT_TRUE(ingress_control != nullptr);
  const IR::P4Control* egress_control = ir_helper_->GetP4Control("egress_stub");
  ASSERT_TRUE(egress_control != nullptr);
  ASSERT_EQ(3, egress_control->type->applyParams->size());

  // This test creates a new egress control, which is a copy of the original,
  // except that the second and third parameters are reversed to generate
  // a local metadata type mismatch between the ingress and egress controls.
  std::unique_ptr<IR::P4Control> test_egress_control =
      CreateControlWithParams(*egress_control, {
          egress_control->type->applyParams->parameters[0],
          egress_control->type->applyParams->parameters[2],
          egress_control->type->applyParams->parameters[1]});
  std::vector<const IR::P4Control*> test_controls =
      {ingress_control, test_egress_control.get()};
  FindLocalMetadataType(test_controls, &model_names);
  EXPECT_TRUE(model_names.local_metadata_type_name().empty());
  EXPECT_NE(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

TEST_F(P4cUtilsTest, TestFindLocalMetadataWrongParamType) {
  SetUpTestIR("action_misc.ir.json");
  P4ModelNames model_names;
  model_names.set_ingress_control_name("ingress");
  model_names.set_egress_control_name("egress_stub");
  const IR::P4Control* ingress_control = ir_helper_->GetP4Control("ingress");
  ASSERT_TRUE(ingress_control != nullptr);
  const IR::P4Control* egress_control = ir_helper_->GetP4Control("egress_stub");
  ASSERT_TRUE(egress_control != nullptr);
  ASSERT_EQ(3, egress_control->type->applyParams->size());

  // This test creates a new egress control, which is a copy of the original
  // with the type of the local metadata parameter changed to Type_Void.
  const IR::Parameter& old_param =
      *egress_control->type->applyParams->parameters[1];
  IR::Type_Void bad_type_param;
  IR::Parameter new_param(old_param.name, old_param.direction, &bad_type_param);
  std::unique_ptr<IR::P4Control> test_egress_control =
      CreateControlWithParams(*egress_control, {
          egress_control->type->applyParams->parameters[0],
          &new_param,
          egress_control->type->applyParams->parameters[2]});
  std::vector<const IR::P4Control*> test_controls =
      {ingress_control, test_egress_control.get()};
  FindLocalMetadataType(test_controls, &model_names);
  EXPECT_TRUE(model_names.local_metadata_type_name().empty());
  EXPECT_NE(0, ::errorCount());  // Errors from p4c's internal error reporter.
}

TEST_F(P4cUtilsTest, TestFieldTypeNotSet) {
  hal::P4FieldDescriptor field_descriptor;
  EXPECT_TRUE(IsFieldTypeUnspecified(field_descriptor));
}

TEST_F(P4cUtilsTest, TestFieldTypeUnknown) {
  hal::P4FieldDescriptor field_descriptor;
  field_descriptor.set_type(P4_FIELD_TYPE_UNKNOWN);
  EXPECT_TRUE(IsFieldTypeUnspecified(field_descriptor));
}

TEST_F(P4cUtilsTest, TestFieldTypeAnnotated) {
  hal::P4FieldDescriptor field_descriptor;
  field_descriptor.set_type(P4_FIELD_TYPE_ANNOTATED);
  EXPECT_TRUE(IsFieldTypeUnspecified(field_descriptor));
}

TEST_F(P4cUtilsTest, TestFieldTypeSpecified) {
  hal::P4FieldDescriptor field_descriptor;
  field_descriptor.set_type(P4_FIELD_TYPE_ETH_SRC);
  EXPECT_FALSE(IsFieldTypeUnspecified(field_descriptor));
}

TEST_F(P4cUtilsTest, TestP4ModelNamesGetNotSet) {
  const P4ModelNames& model_names = GetP4ModelNames();
  const P4ModelNames empty_model_names;
  EXPECT_TRUE(ProtoEqual(empty_model_names, model_names));
}

TEST_F(P4cUtilsTest, TestP4ModelNamesSetGet) {
  P4ModelNames test_model_names;
  test_model_names.set_ingress_control_name("test-p4-model-ingress");
  test_model_names.set_egress_control_name("test-p4-model-egress");
  SetP4ModelNames(test_model_names);
  const P4ModelNames& model_names = GetP4ModelNames();
  EXPECT_TRUE(ProtoEqual(test_model_names, model_names));
}

TEST_F(P4cUtilsTest, TestP4ModelNamesTest) {
  SetUpTestP4ModelNames();
  const P4ModelNames& model_names = GetP4ModelNames();
  EXPECT_FALSE(model_names.ingress_control_name().empty());
  EXPECT_FALSE(model_names.egress_control_name().empty());
  EXPECT_FALSE(model_names.drop_extern_name().empty());
  EXPECT_FALSE(model_names.clone_extern_name().empty());
  EXPECT_FALSE(model_names.clone3_extern_name().empty());
  EXPECT_FALSE(model_names.counter_extern_name().empty());
  EXPECT_FALSE(model_names.meter_extern_name().empty());
  EXPECT_FALSE(model_names.direct_counter_extern_name().empty());
  EXPECT_FALSE(model_names.direct_meter_extern_name().empty());
  EXPECT_FALSE(model_names.counter_count_method_name().empty());
  EXPECT_FALSE(model_names.direct_counter_count_method_name().empty());
  EXPECT_FALSE(model_names.meter_execute_method_name().empty());
  EXPECT_FALSE(model_names.direct_meter_read_method_name().empty());
  EXPECT_FALSE(model_names.color_enum_type().empty());
  EXPECT_FALSE(model_names.color_enum_green().empty());
  EXPECT_FALSE(model_names.color_enum_yellow().empty());
  EXPECT_FALSE(model_names.color_enum_red().empty());
  EXPECT_FALSE(model_names.clone_type_ingress_to_egress().empty());
  EXPECT_FALSE(model_names.clone_type_egress_to_egress().empty());
  EXPECT_FALSE(model_names.no_action().empty());
  EXPECT_FALSE(model_names.exact_match().empty());
  EXPECT_FALSE(model_names.lpm_match().empty());
  EXPECT_FALSE(model_names.ternary_match().empty());
  EXPECT_FALSE(model_names.range_match().empty());
  EXPECT_FALSE(model_names.selector_match().empty());
}

TEST_F(P4cUtilsTest, TestAddHeaderArrayIndex) {
  const std::string test_output = AddHeaderArrayIndex("hdr.field", 0);
  EXPECT_EQ("hdr.field[0]", test_output);
}

TEST_F(P4cUtilsTest, TestArrayIndexOnly) {
  const std::string test_output = AddHeaderArrayIndex("", 10);
  EXPECT_EQ("[10]", test_output);
}

TEST_F(P4cUtilsTest, TestAddHeaderArrayLast) {
  const std::string test_output = AddHeaderArrayLast("hdr.field");
  EXPECT_EQ("hdr.field.last", test_output);
}

TEST_F(P4cUtilsTest, TestRejectState) {
  ParserState test_state;
  test_state.mutable_transition()->set_next_state("reject");
  EXPECT_TRUE(IsParserEndState(test_state));
}

TEST_F(P4cUtilsTest, TestAcceptState) {
  ParserState test_state;
  test_state.mutable_transition()->set_next_state("accept");
  EXPECT_TRUE(IsParserEndState(test_state));
}

TEST_F(P4cUtilsTest, TestNonEndState) {
  ParserState test_state;
  test_state.mutable_transition()->set_next_state("not-accept-or-reject");
  EXPECT_FALSE(IsParserEndState(test_state));
}

TEST_F(P4cUtilsTest, TestFindTableDescriptorOrDie) {
  const std::string kTestTable = "test-table";
  table_map_generator_.AddTable(kTestTable);
  table_map_generator_.SetTableType(kTestTable, P4_TABLE_L3_IP);
  const auto& table_descriptor = FindTableDescriptorOrDie(
      kTestTable, table_map_generator_.generated_map());
  EXPECT_EQ(P4_TABLE_L3_IP, table_descriptor.type());
}

TEST_F(P4cUtilsTest, TestFindMutableTableDescriptorOrDie) {
  const std::string kTestTable = "test-table";
  table_map_generator_.AddTable(kTestTable);
  table_map_generator_.SetTableType(kTestTable, P4_TABLE_L3_IP);
  hal::P4PipelineConfig mutable_map = table_map_generator_.generated_map();
  auto table_descriptor =
      FindMutableTableDescriptorOrDie(kTestTable, &mutable_map);
  EXPECT_EQ(P4_TABLE_L3_IP, table_descriptor->type());
}

TEST_F(P4cUtilsTest, TestFindActionDescriptorOrDie) {
  const std::string kTestAction = "test-action";
  table_map_generator_.AddAction(kTestAction);
  table_map_generator_.AddDropPrimitive(kTestAction);
  const auto& action_descriptor = FindActionDescriptorOrDie(
      kTestAction, table_map_generator_.generated_map());
  EXPECT_EQ(1, action_descriptor.primitive_ops_size());
}

TEST_F(P4cUtilsTest, TestFindMutableActionDescriptorOrDie) {
  const std::string kTestAction = "test-action";
  table_map_generator_.AddAction(kTestAction);
  table_map_generator_.AddDropPrimitive(kTestAction);
  hal::P4PipelineConfig mutable_map = table_map_generator_.generated_map();
  auto action_descriptor =
      FindMutableActionDescriptorOrDie(kTestAction, &mutable_map);
  EXPECT_EQ(1, action_descriptor->primitive_ops_size());
}

TEST_F(P4cUtilsTest, TestFindHeaderDescriptorOrDie) {
  const std::string kTestHeader = "test-header";
  table_map_generator_.AddHeader(kTestHeader);
  table_map_generator_.SetHeaderAttributes(kTestHeader, P4_HEADER_GRE, 0);
  const auto& header_descriptor = FindHeaderDescriptorOrDie(
      kTestHeader, table_map_generator_.generated_map());
  EXPECT_EQ(P4_HEADER_GRE, header_descriptor.type());
}

TEST_F(P4cUtilsTest, TestFindHeaderDescriptorForFieldOrDieInner) {
  const std::string kTestHeaderOuter = "outer-header";
  const std::string kTestHeaderInner = "inner-header";
  table_map_generator_.AddHeader(kTestHeaderOuter);
  table_map_generator_.SetHeaderAttributes(kTestHeaderOuter, P4_HEADER_IPV4, 0);
  table_map_generator_.AddHeader(kTestHeaderInner);
  table_map_generator_.SetHeaderAttributes(kTestHeaderInner, P4_HEADER_IPV4, 1);
  const std::string kTestFieldName = kTestHeaderInner + ".field";
  const auto& header_descriptor = FindHeaderDescriptorForFieldOrDie(
      kTestFieldName, P4_HEADER_IPV4, table_map_generator_.generated_map());
  EXPECT_EQ(P4_HEADER_IPV4, header_descriptor.type());
  EXPECT_EQ(1, header_descriptor.depth());
}

TEST_F(P4cUtilsTest, TestFindHeaderDescriptorForFieldOrDieOuter) {
  const std::string kTestHeaderOuter = "outer-header";
  const std::string kTestHeaderInner = "inner-header";
  table_map_generator_.AddHeader(kTestHeaderOuter);
  table_map_generator_.SetHeaderAttributes(kTestHeaderOuter, P4_HEADER_IPV4, 0);
  table_map_generator_.AddHeader(kTestHeaderInner);
  table_map_generator_.SetHeaderAttributes(kTestHeaderInner, P4_HEADER_IPV4, 1);
  const std::string kTestFieldName = kTestHeaderOuter + ".field";
  const auto& header_descriptor = FindHeaderDescriptorForFieldOrDie(
      kTestFieldName, P4_HEADER_IPV4, table_map_generator_.generated_map());
  EXPECT_EQ(P4_HEADER_IPV4, header_descriptor.type());
  EXPECT_EQ(0, header_descriptor.depth());
}

TEST_F(P4cUtilsTest, TestFindFieldDescriptorOrNull) {
  const std::string kTestField = "test-field";
  table_map_generator_.AddField(kTestField);
  table_map_generator_.SetFieldType(kTestField, P4_FIELD_TYPE_ETH_SRC);
  const auto field_descriptor = FindFieldDescriptorOrNull(
      kTestField, table_map_generator_.generated_map());
  ASSERT_NE(nullptr, field_descriptor);
  EXPECT_EQ(P4_FIELD_TYPE_ETH_SRC, field_descriptor->type());
}

TEST_F(P4cUtilsTest, TestNoFieldDescriptor) {
  hal::P4PipelineConfig empty_map;
  EXPECT_EQ(nullptr, FindFieldDescriptorOrNull("no-field", empty_map));
}

TEST_F(P4cUtilsTest, TestNotFieldDescriptor) {
  const std::string kTestDescriptor = "test-header";
  table_map_generator_.AddHeader(kTestDescriptor);  // Header descriptor.
  EXPECT_EQ(nullptr, FindFieldDescriptorOrNull(
      kTestDescriptor, table_map_generator_.generated_map()));
}

TEST_F(P4cUtilsTest, TestFindMutableFieldDescriptorOrNull) {
  const std::string kTestField = "test-field";
  table_map_generator_.AddField(kTestField);
  table_map_generator_.SetFieldType(kTestField, P4_FIELD_TYPE_ETH_SRC);
  hal::P4PipelineConfig mutable_map = table_map_generator_.generated_map();
  auto field_descriptor = FindMutableFieldDescriptorOrNull(
      kTestField, &mutable_map);
  ASSERT_NE(nullptr, field_descriptor);
  EXPECT_EQ(P4_FIELD_TYPE_ETH_SRC, field_descriptor->type());
}

TEST_F(P4cUtilsTest, TestNoMutableFieldDescriptor) {
  hal::P4PipelineConfig empty_map;
  EXPECT_EQ(nullptr, FindMutableFieldDescriptorOrNull("no-field", &empty_map));
}

TEST_F(P4cUtilsTest, TestNotMutableFieldDescriptor) {
  const std::string kTestDescriptor = "test-header";
  table_map_generator_.AddHeader(kTestDescriptor);  // Header descriptor.
  hal::P4PipelineConfig mutable_map = table_map_generator_.generated_map();
  EXPECT_EQ(nullptr, FindMutableFieldDescriptorOrNull(
      kTestDescriptor, &mutable_map));
}

// Tests below exercise various combinations of repeated field deletion.
TEST(RepeatedDeleteTest, TestDeleteFirstRepeatedPtrField) {
  ParserExtractHeader test_fields;
  test_fields.add_header_paths("field1");
  test_fields.add_header_paths("field2");
  test_fields.add_header_paths("field3");
  std::vector<int> deleted_fields = {0};
  DeleteRepeatedFields(deleted_fields, test_fields.mutable_header_paths());
  ASSERT_EQ(2, test_fields.header_paths_size());
  EXPECT_EQ("field2", test_fields.header_paths(0));
  EXPECT_EQ("field3", test_fields.header_paths(1));
}

TEST(RepeatedDeleteTest, TestDeleteFirstRepeatedField) {
  hal::P4ActionDescriptor test_fields;
  test_fields.add_primitive_ops(P4_ACTION_OP_CLONE);
  test_fields.add_primitive_ops(P4_ACTION_OP_DROP);
  test_fields.add_primitive_ops(P4_ACTION_OP_NOP);
  std::vector<int> deleted_fields = {0};
  DeleteRepeatedNonPtrFields(deleted_fields,
                             test_fields.mutable_primitive_ops());
  ASSERT_EQ(2, test_fields.primitive_ops_size());
  EXPECT_EQ(P4_ACTION_OP_DROP, test_fields.primitive_ops(0));
  EXPECT_EQ(P4_ACTION_OP_NOP, test_fields.primitive_ops(1));
}

TEST(RepeatedDeleteTest, TestDeleteMiddleRepeatedPtrField) {
  ParserExtractHeader test_fields;
  test_fields.add_header_paths("field1");
  test_fields.add_header_paths("field2");
  test_fields.add_header_paths("field3");
  std::vector<int> deleted_fields = {1};
  DeleteRepeatedFields(deleted_fields, test_fields.mutable_header_paths());
  ASSERT_EQ(2, test_fields.header_paths_size());
  EXPECT_EQ("field1", test_fields.header_paths(0));
  EXPECT_EQ("field3", test_fields.header_paths(1));
}

TEST(RepeatedDeleteTest, TestDeleteMiddleRepeatedField) {
  hal::P4ActionDescriptor test_fields;
  test_fields.add_primitive_ops(P4_ACTION_OP_CLONE);
  test_fields.add_primitive_ops(P4_ACTION_OP_DROP);
  test_fields.add_primitive_ops(P4_ACTION_OP_NOP);
  std::vector<int> deleted_fields = {1};
  DeleteRepeatedNonPtrFields(deleted_fields,
                             test_fields.mutable_primitive_ops());
  ASSERT_EQ(2, test_fields.primitive_ops_size());
  EXPECT_EQ(P4_ACTION_OP_CLONE, test_fields.primitive_ops(0));
  EXPECT_EQ(P4_ACTION_OP_NOP, test_fields.primitive_ops(1));
}

TEST(RepeatedDeleteTest, TestDeleteLastRepeatedPtrField) {
  ParserExtractHeader test_fields;
  test_fields.add_header_paths("field1");
  test_fields.add_header_paths("field2");
  test_fields.add_header_paths("field3");
  std::vector<int> deleted_fields = {2};
  DeleteRepeatedFields(deleted_fields, test_fields.mutable_header_paths());
  ASSERT_EQ(2, test_fields.header_paths_size());
  EXPECT_EQ("field1", test_fields.header_paths(0));
  EXPECT_EQ("field2", test_fields.header_paths(1));
}

TEST(RepeatedDeleteTest, TestDeleteLastRepeatedField) {
  hal::P4ActionDescriptor test_fields;
  test_fields.add_primitive_ops(P4_ACTION_OP_CLONE);
  test_fields.add_primitive_ops(P4_ACTION_OP_DROP);
  test_fields.add_primitive_ops(P4_ACTION_OP_NOP);
  std::vector<int> deleted_fields = {2};
  DeleteRepeatedNonPtrFields(deleted_fields,
                             test_fields.mutable_primitive_ops());
  ASSERT_EQ(2, test_fields.primitive_ops_size());
  EXPECT_EQ(P4_ACTION_OP_CLONE, test_fields.primitive_ops(0));
  EXPECT_EQ(P4_ACTION_OP_DROP, test_fields.primitive_ops(1));
}

TEST(RepeatedDeleteTest, TestDeleteAllRepeatedPtrField) {
  ParserExtractHeader test_fields;
  test_fields.add_header_paths("field1");
  test_fields.add_header_paths("field2");
  test_fields.add_header_paths("field3");
  std::vector<int> deleted_fields = {0, 1, 2};
  DeleteRepeatedFields(deleted_fields, test_fields.mutable_header_paths());
  EXPECT_EQ(0, test_fields.header_paths_size());
}

TEST(RepeatedDeleteTest, TestDeleteAllRepeatedField) {
  hal::P4ActionDescriptor test_fields;
  test_fields.add_primitive_ops(P4_ACTION_OP_CLONE);
  test_fields.add_primitive_ops(P4_ACTION_OP_DROP);
  test_fields.add_primitive_ops(P4_ACTION_OP_NOP);
  std::vector<int> deleted_fields = {0, 1, 2};
  DeleteRepeatedNonPtrFields(deleted_fields,
                             test_fields.mutable_primitive_ops());
  EXPECT_EQ(0, test_fields.primitive_ops_size());
}

TEST(RepeatedDeleteTest, TestDeleteNoneRepeatedPtrField) {
  ParserExtractHeader test_fields;
  test_fields.add_header_paths("field1");
  test_fields.add_header_paths("field2");
  test_fields.add_header_paths("field3");
  std::vector<int> deleted_fields = {};
  DeleteRepeatedFields(deleted_fields, test_fields.mutable_header_paths());
  ASSERT_EQ(3, test_fields.header_paths_size());
  EXPECT_EQ("field1", test_fields.header_paths(0));
  EXPECT_EQ("field2", test_fields.header_paths(1));
  EXPECT_EQ("field3", test_fields.header_paths(2));
}

TEST(RepeatedDeleteTest, TestDeleteNoneRepeatedField) {
  hal::P4ActionDescriptor test_fields;
  test_fields.add_primitive_ops(P4_ACTION_OP_CLONE);
  test_fields.add_primitive_ops(P4_ACTION_OP_DROP);
  test_fields.add_primitive_ops(P4_ACTION_OP_NOP);
  std::vector<int> deleted_fields = {};
  DeleteRepeatedNonPtrFields(deleted_fields,
                             test_fields.mutable_primitive_ops());
  ASSERT_EQ(3, test_fields.primitive_ops_size());
  EXPECT_EQ(P4_ACTION_OP_CLONE, test_fields.primitive_ops(0));
  EXPECT_EQ(P4_ACTION_OP_DROP, test_fields.primitive_ops(1));
  EXPECT_EQ(P4_ACTION_OP_NOP, test_fields.primitive_ops(2));
}

using P4cUtilsDeathTest = P4cUtilsTest;

TEST_F(P4cUtilsDeathTest, TestNoTableDescriptor) {
  hal::P4PipelineConfig empty_map;
  EXPECT_DEATH(FindTableDescriptorOrDie("no-table", empty_map), "");
}

TEST_F(P4cUtilsDeathTest, TestNoMutableTableDescriptor) {
  hal::P4PipelineConfig empty_map;
  EXPECT_DEATH(FindMutableTableDescriptorOrDie("no-table", &empty_map), "");
}

TEST_F(P4cUtilsDeathTest, TestNotTableDescriptor) {
  const std::string kTestDescriptor = "test-header";
  table_map_generator_.AddHeader(kTestDescriptor);  // Header descriptor.
  EXPECT_DEATH(FindTableDescriptorOrDie(
      kTestDescriptor, table_map_generator_.generated_map()),
      "not a table descriptor");
}

TEST_F(P4cUtilsDeathTest, TestNotMutableTableDescriptor) {
  const std::string kTestDescriptor = "test-header";
  table_map_generator_.AddHeader(kTestDescriptor);  // Header descriptor.
  hal::P4PipelineConfig mutable_map = table_map_generator_.generated_map();
  EXPECT_DEATH(FindMutableTableDescriptorOrDie(
      kTestDescriptor, &mutable_map), "not a table descriptor");
}

TEST_F(P4cUtilsDeathTest, TestNoActionDescriptor) {
  hal::P4PipelineConfig empty_map;
  EXPECT_DEATH(FindActionDescriptorOrDie("no-action", empty_map), "");
}

TEST_F(P4cUtilsDeathTest, TestNoMutableActionDescriptor) {
  hal::P4PipelineConfig empty_map;
  EXPECT_DEATH(FindMutableActionDescriptorOrDie("no-action", &empty_map), "");
}

TEST_F(P4cUtilsDeathTest, TestNotActionDescriptor) {
  const std::string kTestDescriptor = "test-header";
  table_map_generator_.AddHeader(kTestDescriptor);  // Header descriptor.
  EXPECT_DEATH(FindActionDescriptorOrDie(
      kTestDescriptor, table_map_generator_.generated_map()),
      "not an action descriptor");
}

TEST_F(P4cUtilsDeathTest, TestNotMutableActionDescriptor) {
  const std::string kTestDescriptor = "test-header";
  table_map_generator_.AddHeader(kTestDescriptor);  // Header descriptor.
  hal::P4PipelineConfig mutable_map = table_map_generator_.generated_map();
  EXPECT_DEATH(FindMutableActionDescriptorOrDie(
      kTestDescriptor, &mutable_map), "not an action descriptor");
}

TEST_F(P4cUtilsDeathTest, TestNoHeaderDescriptor) {
  hal::P4PipelineConfig empty_map;
  EXPECT_DEATH(FindHeaderDescriptorOrDie("no-header", empty_map), "");
}

TEST_F(P4cUtilsDeathTest, TestNotHeaderDescriptor) {
  const std::string kTestDescriptor = "test-action";
  table_map_generator_.AddAction(kTestDescriptor);  // Action descriptor.
  EXPECT_DEATH(FindHeaderDescriptorOrDie(
      kTestDescriptor, table_map_generator_.generated_map()),
      "not a header descriptor");
}

TEST_F(P4cUtilsDeathTest, TestNoHeaderDescriptorForField) {
  const std::string kTestHeader = "test-header";
  const std::string kTestFieldName = kTestHeader + ".field";
  table_map_generator_.AddField(kTestHeader);
  table_map_generator_.AddHeader(kTestHeader);
  table_map_generator_.SetHeaderAttributes(kTestHeader, P4_HEADER_IPV6, 0);
  EXPECT_DEATH(FindHeaderDescriptorForFieldOrDie(
      kTestFieldName, P4_HEADER_IPV4, table_map_generator_.generated_map()),
      "No header descriptor with type");
}

}  // namespace p4c_backends
}  // namespace stratum
