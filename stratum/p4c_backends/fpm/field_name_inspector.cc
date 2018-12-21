// This file implements FieldNameInspector.

#include "platforms/networking/hercules/p4c_backend/switch/field_name_inspector.h"

#include "base/logging.h"
#include "platforms/networking/hercules/p4c_backend/switch/p4_model_names.host.pb.h"
#include "platforms/networking/hercules/p4c_backend/switch/utils.h"
#include "absl/debugging/leak_check.h"

namespace google {
namespace hercules {
namespace p4c_backend {

FieldNameInspector::FieldNameInspector()
    : ignored_path_prefixes_(GetP4ModelNames().strip_path_prefixes()),
      field_name_("") {
}

void FieldNameInspector::ExtractName(const IR::Expression& expression) {
  if (!field_name_.empty()) {
    LOG(ERROR) << "ExtractName has already run in this FieldNameInspector";
    field_name_ = "";
    return;
  }

  // When the expression is applied to this FieldNameInspector, it causes
  // the postorder methods to run and extract the field path strings.
  // TODO(teverman): Figure out how to report a syntax error if arithmetic
  //                 appears in the expression.
  VLOG(4) << "ExtractName from " << expression.toString();
  absl::LeakCheckDisabler disable_ir_expression_leak_checks;
  expression.apply(*this);
  VLOG(4) << "Extracted field name is " << field_name_;
}

// Saves the member name as part of the field name.  Also handles header
// stacks upon encountering the P4 parser ".next" operator.
void FieldNameInspector::postorder(const IR::Member* member) {
  VLOG(4) << "FieldNameInspector Member " << member->member;
  if (member->member == IR::Type_Stack::next) {
    AppendStackedHeaderPathNames();
  } else {
    auto stack = member->type->to<IR::Type_Stack>();
    if (stack != nullptr)
      stack_size_ = stack->getSize();
  }
  AppendName(std::string(member->member.name));
}

void FieldNameInspector::postorder(const IR::Type_Stack* stack) {
  VLOG(4) << "FieldNameInspector found a header stack " << stack->toString();
  // TODO(teverman): Does this need to set a flag for header stack processing?
  //                 Could an IR::ArrayIndex appear in some other context?
}

// Saves the path name as part of the field name, subject to ignored prefixes.
void FieldNameInspector::postorder(const IR::PathExpression* path) {
  VLOG(4) << "FieldNameInspector Path " << path->toString();
  if (!path->type->is<IR::Type_Struct>()) {
    // TODO(teverman): What is the proper way to handle this error?
    LOG(ERROR) << "Expected header path expression " << path
               << " to be Type_struct";
    return;
  }

  // The V1 conversion prefixes are ignored at the beginning of the name.
  const std::string path_name(std::string(path->path->name.toString()));
  if (field_name_.empty()) {
    if (ignored_path_prefixes_.find(path_name) !=
        ignored_path_prefixes_.end()) {
      ignored_path_prefixes_.clear();
      return;
    }
  }
  AppendName(path_name);
}

// The index value comes from the array_index right expression, which
// should be Constant for this implementation.
void FieldNameInspector::postorder(const IR::ArrayIndex* array_index) {
  VLOG(4) << "FieldNameInspector Array Index " << array_index->toString();
  if (!array_index->right->is<IR::Constant>()) {
    LOG(ERROR) << "Expected array index right field to be Constant "
               << array_index->right;
    // TODO(teverman): What is the proper way to handle this error?
    return;
  }
  auto header_stack_index = array_index->right->to<IR::Constant>();
  if (!field_name_.empty()) {
    field_name_ = AddHeaderArrayIndex(field_name_, header_stack_index->asInt());
  }
}

void FieldNameInspector::AppendName(const std::string& name) {
  if (field_name_.empty()) {
    field_name_ = name;
  } else {
    field_name_ += "." + name;
  }
}

void FieldNameInspector::AppendStackedHeaderPathNames() {
  for (int i = 0; i < stack_size_; ++i) {
    stacked_header_names_.push_back(AddHeaderArrayIndex(field_name_, i));
  }
  stacked_header_names_.push_back(AddHeaderArrayLast(field_name_));
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
