// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// A ProgramInspector is a p4c Inspector subclass that visits all nodes in the
// P4 program's IR on behalf of a backend extension.  It records nodes of
// possible interest to the backend for subsequent processing.

#ifndef STRATUM_P4C_BACKENDS_COMMON_PROGRAM_INSPECTOR_H_
#define STRATUM_P4C_BACKENDS_COMMON_PROGRAM_INSPECTOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

// FIXME: figure out better include path
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"

namespace stratum {
namespace p4c_backends {

// The normal ProgramInspector usage is to pass an instance to the apply
// method of the ToplevelBlock in the IR. See the sample usage below:
//
//  void P4FooExtension::Compile(const IR::ToplevelBlock& top_level, ...) {
//    ...
//    ProgramInspector inspector;
//    top_level.getProgram()->apply(inspector);
//    <Use inspector accessors to iterate and process recorded IR objects.>
//  }
class ProgramInspector : public Inspector {
 public:
  ProgramInspector() {}
  ~ProgramInspector() override {}

  // As the Inspector base class visits each IR node, it calls the postorder
  // method that corresponds to the node type.  These postorder overrides
  // record the nodes of interest to the p4c backend by adding them to
  // one of the private containers below.
  void postorder(const IR::P4Action* action) override;
  void postorder(const IR::Type_StructLike* header) override;
  void postorder(const IR::Type_Header* header) override;
  void postorder(const IR::Type_Typedef* p4_typedef) override;
  void postorder(const IR::Type_Enum* p4_enum) override;
  void postorder(const IR::PathExpression* path) override;
  void postorder(const IR::P4Table* table) override;
  void postorder(const IR::KeyElement* match) override;
  void postorder(const IR::P4Parser* parser) override;
  void postorder(const IR::P4Control* control) override;
  void postorder(const IR::AssignmentStatement* assignment) override;

  // Accessors to recorded IR nodes.
  const std::map<const IR::P4Action*, const IR::P4Control*>& actions() const {
    return actions_;
  }
  const std::vector<const IR::Type_StructLike*>& struct_likes() const {
    return struct_likes_;
  }
  const std::vector<const IR::Type_Header*>& header_types() const {
    return header_types_;
  }
  const std::vector<const IR::Type_Typedef*>& p4_typedefs() const {
    return p4_typedefs_;
  }
  const std::vector<const IR::Type_Enum*>& p4_enums() const {
    return p4_enums_;
  }
  const std::vector<const IR::KeyElement*>& match_keys() const {
    return match_keys_;
  }
  const std::vector<const IR::PathExpression*>& struct_paths() const {
    return struct_paths_;
  }
  const std::vector<const IR::P4Table*>& tables() const {
    return tables_;
  }
  const std::vector<const IR::P4Parser*>& parsers() const {
    return parsers_;
  }
  const std::vector<const IR::P4Control*>& controls() const {
    return controls_;
  }
  const std::vector<const IR::AssignmentStatement*>& assignments() const {
    return assignments_;
  }
  const std::vector<const IR::AssignmentStatement*>& action_assignments()
      const {
    return action_assignments_;
  }

  // ProgramInspector is neither copyable nor movable.
  ProgramInspector(const ProgramInspector&) = delete;
  ProgramInspector& operator=(const ProgramInspector&) = delete;

 private:
  // These containers record the visited nodes for use after this inspector
  // is applied.
  // TODO(unknown): Does the P4Action need the P4Control?
  std::map<const IR::P4Action*, const IR::P4Control*> actions_;
  std::vector<const IR::Type_StructLike*> struct_likes_;
  std::vector<const IR::Type_Header*> header_types_;
  std::vector<const IR::Type_Typedef*> p4_typedefs_;
  std::vector<const IR::Type_Enum*> p4_enums_;
  std::vector<const IR::PathExpression*> struct_paths_;
  std::vector<const IR::KeyElement*> match_keys_;
  std::vector<const IR::P4Table*> tables_;
  std::vector<const IR::P4Parser*> parsers_;
  std::vector<const IR::P4Control*> controls_;
  std::vector<const IR::AssignmentStatement*> assignments_;
  std::vector<const IR::AssignmentStatement*> action_assignments_;

  // The IR contains many redundant instances of the same PathExpression.
  // This set filters the path name so that each path only appears once in
  // the struct_paths_ container.
  std::set<std::string> struct_path_filter_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_COMMON_PROGRAM_INSPECTOR_H_
