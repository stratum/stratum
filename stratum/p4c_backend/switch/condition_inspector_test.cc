// Contains unit tests for ConditionInspector.

#include "platforms/networking/hercules/p4c_backend/switch/condition_inspector.h"

#include <memory>
#include <string>

#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "p4lang_p4c/frontends/common/options.h"
#include "p4lang_p4c/ir/ir.h"
#include "p4lang_p4c/lib/compile_context.h"

namespace google {
namespace hercules {
namespace p4c_backend {

class ConditionInspectorTest : public testing::Test {
 protected:
  ConditionInspectorTest()
      : test_p4c_context_(new P4CContextWithOptions<CompilerOptions>) {
  }

  // Initializes compare_left_ and compare_right_ for comparison tests.
  void SetUpCompareTest() {
    cstring left_var_name = "tmp_var";
    compare_left_ = absl::make_unique<IR::PathExpression>(left_var_name);
    compare_right_ = absl::make_unique<IR::Constant>(123);
  }

  ConditionInspector test_inspector_;  // Common ConditionInspector for tests.

  // Left and right-hand sides of compare operator.
  std::unique_ptr<IR::PathExpression> compare_left_;
  std::unique_ptr<IR::Constant> compare_right_;

  // This test uses its own p4c context since it doesn't have the one normally
  // provided by IRTestHelperJson.
  AutoCompileContext test_p4c_context_;
};

TEST_F(ConditionInspectorTest, TestCompareEqu) {
  SetUpCompareTest();
  std::unique_ptr<IR::Equ> equ_condition(new IR::Equ(
      compare_left_.get(), compare_right_.get()));
  test_inspector_.Inspect(*equ_condition);
  EXPECT_EQ(0, ::errorCount());
  const std::string description = test_inspector_.description();
  EXPECT_FALSE(description.empty());
  EXPECT_EQ(0, description.find(compare_left_->toString()));
  EXPECT_NE(std::string::npos, description.find("=="));
  EXPECT_NE(std::string::npos, description.find(compare_right_->toString()));
}

TEST_F(ConditionInspectorTest, TestCompareNeq) {
  SetUpCompareTest();
  std::unique_ptr<IR::Neq> neq_condition(new IR::Neq(
      compare_left_.get(), compare_right_.get()));
  test_inspector_.Inspect(*neq_condition);
  EXPECT_EQ(0, ::errorCount());
  const std::string description = test_inspector_.description();
  EXPECT_FALSE(description.empty());
  EXPECT_EQ(0, description.find(compare_left_->toString()));
  EXPECT_NE(std::string::npos, description.find("!="));
  EXPECT_NE(std::string::npos, description.find(compare_right_->toString()));
}

// Uses an IR::Add operation to yield an unrecognized condition output.
TEST_F(ConditionInspectorTest, TestCompareUnknown) {
  SetUpCompareTest();
  std::unique_ptr<IR::Add> unknown_condition(new IR::Add(
      compare_left_.get(), compare_right_.get()));
  test_inspector_.Inspect(*unknown_condition);
  EXPECT_EQ(1, ::errorCount());
  const std::string description = test_inspector_.description();
  EXPECT_FALSE(description.empty());
  EXPECT_EQ("Unrecognized condition", description);
}

// Tests description access without calling Inspect.
TEST_F(ConditionInspectorTest, TestDescriptionNoInspect) {
  EXPECT_TRUE(test_inspector_.description().empty());
}

// Verifies that a second Inspect doesn't disturb the output of a prior Inspect.
TEST_F(ConditionInspectorTest, TestInspectTwice) {
  SetUpCompareTest();
  std::unique_ptr<IR::Neq> neq_condition(new IR::Neq(
      compare_left_.get(), compare_right_.get()));
  test_inspector_.Inspect(*neq_condition);
  EXPECT_EQ(0, ::errorCount());
  const std::string first_description = test_inspector_.description();
  EXPECT_FALSE(first_description.empty());
  std::unique_ptr<IR::Equ> equ_condition(new IR::Equ(
      compare_left_.get(), compare_right_.get()));
  test_inspector_.Inspect(*equ_condition);
  EXPECT_EQ(first_description, test_inspector_.description());
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
