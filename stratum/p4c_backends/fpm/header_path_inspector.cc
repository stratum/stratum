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

// This file implements HeaderPathInspector.

#include "stratum/p4c_backends/fpm/header_path_inspector.h"

#include "base/logging.h"
#include "stratum/p4c_backends/fpm/p4_model_names.host.pb.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "absl/debugging/leak_check.h"

namespace stratum {
namespace p4c_backends {

HeaderPathInspector::HeaderPathInspector() : header_stack_size_(0) {
  // The same type can appear multiple times in a header definition, so
  // HeaderPathInspector turns off the base class's visitDagOnce to make sure
  // it visits every possible path to each type.
  visitDagOnce = false;
}

// The input expression has two top-level forms.  For an expression representing
// the P4 program's packet headers, it contains a field list of StructField
// nodes, each of which has IR::Type_Header.  For metadata, it also has a list
// of StructField nodes, but each of these nodes has IR::Type_Struct.
bool HeaderPathInspector::Inspect(const IR::PathExpression& expression) {
  if (!path_context_stack_.empty()) {
    LOG(ERROR) << "HeaderPathInspector can only inspect one IR::PathExpression";
    return false;
  }

  // The apply method visits expression nodes and invokes the relevant
  // preorder methods.
  absl::LeakCheckDisabler disable_ir_expression_leak_checks;
  expression.apply(*this);
  if (path_context_stack_.empty())
    return false;
  return true;
}

bool HeaderPathInspector::preorder(const IR::PathExpression* path) {
  // The ProgramInspector should filter out all types except Type_Struct.
  DCHECK(path->type->is<IR::Type_Struct>()) << "Unexpected PathExpression type";
  DCHECK(path_context_stack_.empty()) << "Unexpected nested PathExpression";
  std::string root_path = std::string(path->path->toString());
  VLOG(2) << "PathExpression preorder " << root_path;
  const P4ModelNames& p4_model_names = GetP4ModelNames();
  if (p4_model_names.strip_path_prefixes().find(root_path) !=
      p4_model_names.strip_path_prefixes().end()) {
    root_path.clear();
  }
  auto path_struct = path->type->to<IR::Type_Struct>();
  const std::string outer_type = std::string(path_struct->name.toString());
  VLOG(3) << "Outer type is " << outer_type;

  // A PathExpression contains fields that represent nested types within the
  // expression or data members of the outer_type.  In either case, the next
  // level preorder function sorts out the details, so this loop just makes
  // sure each expression field is visited.
  for (auto field : path_struct->fields) {
    path_context_stack_.clear();
    header_stack_size_ = 0;
    PushPathContext(root_path);
    UpdatePathHeaderType(outer_type);
    visit(field);
  }

  // Upon reaching this point, the Inspector has visited everything of interest,
  // so the return is false to indicate no deeper IR Node traversal is needed.
  return false;
}

bool HeaderPathInspector::preorder(const IR::StructField* field) {
  VLOG(2) << "preorder StructField " << field->externalName() << " depth "
          << getContextDepth();

  // IR::Type_Bits and IR::Type_Enum occur on reaching the end of the header
  // path, so the output map can be updated with information about the current
  // header type.
  // TODO: This backend should restrict enums to metadata types.
  // They don't have bit widths, which will cause problems for enums appearing
  // in packet headers during parser state decoding.
  if (field->type->is<IR::Type_Bits>() || field->type->is<IR::Type_Enum>()) {
    MapPathsToHeaderType();
    return false;  // No need to inspect any further on this path.
  }

  // Stacked headers need to have their stack size recorded before visiting
  // deeper nodes.
  if (field->type->is<IR::Type_Stack>()) {
    // P4_16 section 7.2.3 states that nested header stacks are not supported.
    if (header_stack_size_) {
      LOG(ERROR) << "Compiler bug: Unexpected nested header stack in "
                 << field->externalName();
      return false;
    }
    PushPathContext(std::string(field->externalName()));
    header_stack_size_ = field->type->to<IR::Type_Stack>()->getSize();
    return true;  // Returns true to visit deeper nodes in the path.
  }

  // The field type should be IR::Type_Struct for metadata, IR::Type_Header
  // for a packet header type, or IR::Type_HeaderUnion for a union within
  // one of these types.
  if (!field->type->is<IR::Type_Struct>() &&
      !field->type->is<IR::Type_Header>() &&
      !field->type->is<IR::Type_HeaderUnion>()) {
    LOG(WARNING) << "Unexpected type " << field->type->node_type_name()
                 << " for field " << field->externalName()
                 << " in header PathExpression";
    return false;  // Returns false since no deeper IR traversal is useful.
  }

  PushPathContext(std::string(field->externalName()));
  return true;  // Returns true to traverse deeper nodes for the header type.
}

bool HeaderPathInspector::preorder(const IR::Type_Header* header) {
  VLOG(2) << "preorder Type_Header " << header->externalName();
  UpdatePathHeaderType(std::string(header->externalName()));
  return true;  // Continues deeper inspection for this type.
}

bool HeaderPathInspector::preorder(const IR::Type_HeaderUnion* header_union) {
  VLOG(2) << "preorder Type_HeaderUnion " << header_union->externalName();
  UpdatePathHeaderType(std::string(header_union->externalName()));
  return true;  // Continues deeper inspection for this type.
}

bool HeaderPathInspector::preorder(const IR::Type_Struct* struct_type) {
  VLOG(2) << "preorder Type_Struct " << struct_type->externalName();
  UpdatePathHeaderType(std::string(struct_type->externalName()));
  return true;  // Continues deeper inspection for this type.
}

void HeaderPathInspector::MapPathsToHeaderType() {
  DCHECK(!path_context_stack_.empty());
  PopPathContexts();
  const std::string header_type = path_context_stack_.back().header_type;
  VLOG(2) << "Defining path " << GetPathString()
          << " to header type " << header_type;
  if (header_stack_size_ == 0) {
    path_to_header_type_map_[GetPathString()] = header_type;
  } else {
    const std::string path_string = GetPathString();
    for (int s = 0; s < header_stack_size_; ++s) {
      const std::string path_name = AddHeaderArrayIndex(path_string, s);
      path_to_header_type_map_[path_name] = header_type;
    }
    path_to_header_type_map_[AddHeaderArrayLast(path_string)] = header_type;
  }
}

void HeaderPathInspector::PushPathContext(const std::string& path_name) {
  PopPathContexts();
  PathContextEntry entry;
  entry.header_name = path_name;
  entry.depth = getContextDepth();
  path_context_stack_.push_back(entry);
}

void HeaderPathInspector::PopPathContexts() {
  int depth = getContextDepth();
  auto iter = path_context_stack_.rbegin();
  while (iter != path_context_stack_.rend()) {
    int current_depth = iter->depth;
    ++iter;
    if (current_depth >= depth) {
      path_context_stack_.pop_back();
    }
  }
}

// Stores header_type in the PathContextEntry at the top of the stack,
// expecting no previously stored type to be present.
void HeaderPathInspector::UpdatePathHeaderType(const std::string& header_type) {
  DCHECK(!path_context_stack_.empty());
  PathContextEntry* entry = &path_context_stack_.back();
  DCHECK(entry->header_type.empty()) << "Unexpected header type "<< header_type
                                     << " name " << entry->header_name;
  entry->header_type = header_type;
}

// Iterates path_context_stack_ to generate a '.' separated string of the
// full path name to the header at the top of the stack.
std::string HeaderPathInspector::GetPathString() {
  std::string path;
  for (const auto& iter : path_context_stack_) {
    if (!path.empty())
      path += ".";
    path += iter.header_name;
  }
  return path;
}

}  // namespace p4c_backends
}  // namespace stratum
