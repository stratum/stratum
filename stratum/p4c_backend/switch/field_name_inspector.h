// A FieldNameInspector is a p4c Inspector subclass that visits the node
// hierarchy under various types of IR fields to extract a field name string.

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_FIELD_NAME_INSPECTOR_H_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_FIELD_NAME_INSPECTOR_H_

#include <string>
#include <vector>

#include "google/protobuf/map.h"
#include "p4lang_p4c/frontends/p4/coreLibrary.h"

namespace google {
namespace hercules {
namespace p4c_backend {

// A FieldNameInspector visits IR nodes related to a P4 field expression in
// order to extract the name of the target field.  Examples of target fields
// include the left-hand side of an assignment statement in an action body or a
// match key expression inside a table definition.  A typical usage is:
//
//  const IR::Expression* expression = <some interesting expression in IR>;
//  FieldNameInspector field_inspector(<ignored-prefixes>);
//  field_inspector.ConvertExpression(expression);
//  const std::string field_name = header_inspector.field_name();
//
// The constructor expects the shared P4ModelNames instance to contain a set of
// prefixes to be ignored when forming the field name. This input assures
// consistency between names extracted by this class and the names that p4c
// generates in the P4Info.
class FieldNameInspector : public Inspector {
 public:
  FieldNameInspector();
  ~FieldNameInspector() override {}

  // This method extracts the field name from the IR representation of the
  // input expression.  It can only execute once per FieldNameInspector
  // instance.  Upon successful return, the field name is available via the
  // field_name() accessor.  In some cases, additional names may be available
  // via the stacked_header_names() accessor.
  // TODO(teverman): Should this return the name directly?  This requires
  //                 figuring out how to deal with expressions that the switch
  //                 can't support, such as arithmetic expressions in an
  //                 action assignment.
  void ExtractName(const IR::Expression& expression);

  // The postorder overrides below extract various pieces of the field name.
  // TODO(teverman): Do postorder matches for unsupported types need to be
  //                 added in order to handle syntax errors?
  void postorder(const IR::Member* member) override;
  void postorder(const IR::Type_Stack* stack) override;
  void postorder(const IR::PathExpression* path) override;
  void postorder(const IR::ArrayIndex* array_index) override;

  // This accessor returns the field_name after ExtractName runs.  The
  // returned name is empty if no valid name is extracted.
  const std::string& field_name() const { return field_name_; }

  // This accessor returns a series of stacked header names that may be relevant
  // when the ExtractName input ends with the P4 parser "next" operator.
  const std::vector<std::string>& stacked_header_names() const {
    return stacked_header_names_;
  }

  // FieldNameInspector is neither copyable nor movable.
  FieldNameInspector(const FieldNameInspector&) = delete;
  FieldNameInspector& operator=(const FieldNameInspector&) = delete;

 private:
  // Appends the input name to the extracted field_name_.
  void AppendName(const std::string& name);

  // Appends a sequence of stacked header names to stacked_header_names_.
  void AppendStackedHeaderPathNames();

  // Injected prefixes to ignore.
  ::google::protobuf::Map<::std::string, ::google::protobuf::int32>
      ignored_path_prefixes_;

  // This member is the extracted name.
  std::string field_name_;

  // FieldNameInspector uses these members when encountering a member with
  // IR::Type_stack.  It saves the header stack size in stack_size_.  When
  // the field contains the P4 parser "next" operation ("hdr.vlan_tag.next"),
  // FieldNameInspector creates a list of corresponding stacked header path
  // names in the stacked_header_names_ container, such as "hdr.vlan_tag[0]",
  // "hdr.vlan_tag[1]", etc.
  uint32 stack_size_;
  std::vector<std::string> stacked_header_names_;
};

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_FIELD_NAME_INSPECTOR_H_
