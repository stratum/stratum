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

// This file implements ProgramInspector.

#include "stratum/p4c_backends/common/program_inspector.h"

#include "stratum/glue/logging.h"

namespace stratum {
namespace p4c_backends {

// Records the action node along with the control context.
void ProgramInspector::postorder(const IR::P4Action* action) {
  VLOG(1) << "postorder action " << action->externalName();
  auto control = findContext<IR::P4Control>();
  actions_.emplace(action, control);
}

// To get a full set of header fields and types, a combination of
// IR::Type_StructLike and IR::Type_Header needs to be processed.
void ProgramInspector::postorder(const IR::Type_StructLike* header) {
  VLOG(1) << "postorder struct " << header->externalName();
  // A p4c dump shows all fields enumerated when header is Type_Struct.  The
  // enumerated fields may be real base-level fields (Type_Bits) within
  // a P4 struct _t type.  They can also be names of higher-level fields, i.e.
  // headers.arp, which will have a type of Type_Name.  The tor.p4 dump shows:
  //  Type_Struct name=standard_metadata_t (Type_Bits fields)
  //  Type_Struct name=local_metadata_t (Type_Bits fields)
  //  Type_Struct name=metadata (Type_name field)
  //  Type_Struct name=headers (Type_name fields)
  //  Type_Struct name=tuple_0
  struct_likes_.push_back(header);
}

// This method records all the P4-program-defined _t types, but it has none of
// the built-in metadata types (arp_t, cpu_header_t, ethernet_t, icmp_t,
// ipv4_base_t, ipv6_base_t, tcp_t, udp_t, vlan_tag_t).
void ProgramInspector::postorder(const IR::Type_Header* header) {
  VLOG(1) << "postorder header " << header->externalName();
  header_types_.push_back(header);
}

void ProgramInspector::postorder(const IR::Type_Typedef* p4_typedef) {
  VLOG(1) << "postorder p4_typedef " << p4_typedef->externalName();
  p4_typedefs_.push_back(p4_typedef);
}

void ProgramInspector::postorder(const IR::Type_Enum* p4_enum) {
  VLOG(1) << "postorder p4_enum " << p4_enum->externalName();
  p4_enums_.push_back(p4_enum);
}

// Only paths to Type_Struct are interesting to the backend, and only one
// instance of each path needs to be recorded.
void ProgramInspector::postorder(const IR::PathExpression* path) {
  if (!path->type->is<IR::Type_Struct>()) return;
  const std::string path_key = std::string(path->path->toString());
  if (struct_path_filter_.find(path_key) != struct_path_filter_.end()) return;
  struct_paths_.push_back(path);
  struct_path_filter_.insert(path_key);
}

void ProgramInspector::postorder(const IR::P4Table* table) {
  VLOG(1) << "postorder table " << table->externalName();
  tables_.push_back(table);
}

// Records match keys if and only if it's within a table context.
void ProgramInspector::postorder(const IR::KeyElement* match) {
  VLOG(1) << "postorder match " << match;
  auto table = findContext<IR::P4Table>();
  if (table != nullptr) {
    match_keys_.push_back(match);
  } else {
    VLOG(1) << "postorder matchType " << match << " has no table context";
  }
}

void ProgramInspector::postorder(const IR::P4Parser* parser) {
  VLOG(1) << "postorder parser " << parser->externalName();
  parsers_.push_back(parser);
}

void ProgramInspector::postorder(const IR::P4Control* control) {
  VLOG(1) << "postorder control " << control->externalName();
  controls_.push_back(control);
}

// Builds one container of all assignments and another container limited
// to assignments within action bodies.
void ProgramInspector::postorder(const IR::AssignmentStatement* assignment) {
  VLOG(1) << "postorder assignment " << assignment->toString();
  assignments_.push_back(assignment);
  auto action = findContext<IR::P4Action>();
  if (action != nullptr) action_assignments_.push_back(assignment);
}

}  // namespace p4c_backends
}  // namespace stratum
