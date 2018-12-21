// Contains unit tests for FieldNameInspector.

#include "platforms/networking/hercules/p4c_backend/switch/field_name_inspector.h"

#include <memory>
#include <string>

#include "platforms/networking/hercules/p4c_backend/switch/p4_model_names.host.pb.h"
#include "platforms/networking/hercules/p4c_backend/switch/utils.h"
#include "platforms/networking/hercules/p4c_backend/test/ir_test_helpers.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "p4lang_p4c/ir/ir.h"

namespace google {
namespace hercules {
namespace p4c_backend {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.
class FieldNameInspectorTest : public testing::Test {
 protected:
  // The SetUp method applies a ProgramInspector to record IR nodes that
  // will reference some header fields.
  void SetUp() override {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "platforms/networking/hercules/p4c_backend/switch/"
        "testdata/field_inspect_test.ir.json";
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
  }

  // Sets up P4 model to strip the given header prefix.  If the prefix is empty
  // the default P4 model for testing is restored.
  void SetUpP4ModelHeaderPrefix(const std::string& prefix) {
    SetUpTestP4ModelNames();
    if (!prefix.empty()) {
      P4ModelNames hdr_prefix_in_p4_model = GetP4ModelNames();
      (*hdr_prefix_in_p4_model.mutable_strip_path_prefixes())[prefix] = 0;
      SetP4ModelNames(hdr_prefix_in_p4_model);
    }
  }

  std::unique_ptr<FieldNameInspector> inspector_;  // Tested FieldNameInspector.
  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
};

// Tests extraction of the match key names in the P4 program.  The IR includes
// the "hdr" prefix in front of header fields, which is not used in the P4Info,
// so it is ignored.
TEST_F(FieldNameInspectorTest, TestMatchKeys) {
  SetUpP4ModelHeaderPrefix("hdr");
  std::set<std::string> extract_names;
  ASSERT_NE(0, ir_helper_->program_inspector().match_keys().size());

  // This loop saves all the extracted field names in a set.  When the loop
  // ends, the test verifies the presence of expected keys.
  for (auto match_key : ir_helper_->program_inspector().match_keys()) {
    inspector_ = absl::make_unique<FieldNameInspector>();
    inspector_->ExtractName(*match_key->expression);
    EXPECT_FALSE(inspector_->field_name().empty());
    extract_names.insert(inspector_->field_name());
    EXPECT_TRUE(inspector_->stacked_header_names().empty());
  }

  EXPECT_TRUE(extract_names.find("ethernet.dstAddr") != extract_names.end());
  EXPECT_TRUE(extract_names.find("ethernet.srcAddr") != extract_names.end());
  EXPECT_TRUE(extract_names.find("vlan_tag[0].vlan_id") != extract_names.end());
}

// Verifies that if the "hdr" prefix is not ignored, FieldNameInspector includes
// the prefix at the beginning of the returned names.
TEST_F(FieldNameInspectorTest, TestMatchKeysNoIgnoredPrefixes) {
  SetUpP4ModelHeaderPrefix("");
  ASSERT_NE(0, ir_helper_->program_inspector().match_keys().size());
  for (auto match_key : ir_helper_->program_inspector().match_keys()) {
    inspector_ = absl::make_unique<FieldNameInspector>();
    inspector_->ExtractName(*match_key->expression);
    EXPECT_FALSE(inspector_->field_name().empty());
    EXPECT_EQ(0, inspector_->field_name().find("hdr."));
    EXPECT_TRUE(inspector_->stacked_header_names().empty());
  }
}

// Verifies parser extraction of a stacked header using the ".next" operation.
TEST_F(FieldNameInspectorTest, TestParserStackNext) {
  // The expression to be tested is the extract parameter, which is the
  // first argument of the MethodCallStatement in the first component
  // of the P4 parser state named "ParserImpl.parse_vlan_tag".
  SetUpP4ModelHeaderPrefix("");
  ASSERT_NE(0, ir_helper_->program_inspector().parsers().size());
  const IR::ParserState* vlan_state = nullptr;
  for (auto state : ir_helper_->program_inspector().parsers()[0]->states) {
    if (state->externalName() == cstring("ParserImpl.parse_vlan_tag")) {
      vlan_state = state;
      break;
    }
  }
  ASSERT_TRUE(vlan_state != nullptr);
  ASSERT_LE(1, vlan_state->components.size());
  auto method_call_statement =
      vlan_state->components[0]->to<IR::MethodCallStatement>();
  ASSERT_TRUE(method_call_statement != nullptr);
  ASSERT_LE(1, method_call_statement->methodCall->arguments->size());

  inspector_ = absl::make_unique<FieldNameInspector>();
  inspector_->ExtractName(
      *method_call_statement->methodCall->arguments->at(0)->expression);
  EXPECT_EQ("hdr.vlan_tag.next", inspector_->field_name());
  ASSERT_EQ(6, inspector_->stacked_header_names().size());
  EXPECT_EQ("hdr.vlan_tag[0]", inspector_->stacked_header_names()[0]);
  EXPECT_EQ("hdr.vlan_tag[1]", inspector_->stacked_header_names()[1]);
  EXPECT_EQ("hdr.vlan_tag[2]", inspector_->stacked_header_names()[2]);
  EXPECT_EQ("hdr.vlan_tag[3]", inspector_->stacked_header_names()[3]);
  EXPECT_EQ("hdr.vlan_tag[4]", inspector_->stacked_header_names()[4]);
  EXPECT_EQ("hdr.vlan_tag.last", inspector_->stacked_header_names()[5]);
}

// Verifies behavior when the same FieldNameInspector is reused.
TEST_F(FieldNameInspectorTest, TestExtractTwice) {
  SetUpP4ModelHeaderPrefix("hdr");
  ASSERT_NE(0, ir_helper_->program_inspector().match_keys().size());

  // The first time works.
  inspector_ = absl::make_unique<FieldNameInspector>();
  inspector_->ExtractName(
      *(ir_helper_->program_inspector().match_keys()[0])->expression);
  EXPECT_FALSE(inspector_->field_name().empty());

  // The second time fails with an empty name.
  inspector_->ExtractName(
      *(ir_helper_->program_inspector().match_keys()[0])->expression);
  EXPECT_TRUE(inspector_->field_name().empty());
}

// TODO(teverman): Additional test coverage for header stacks and for fields
//                 extracted from action assignments.

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
