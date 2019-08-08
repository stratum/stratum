/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// The ActionDecoder processes P4Action nodes in the p4c IR.  It adds table map
// ActionDescriptor data to the backend's output P4PipelineConfig.

#ifndef STRATUM_P4C_BACKENDS_FPM_ACTION_DECODER_H_
#define STRATUM_P4C_BACKENDS_FPM_ACTION_DECODER_H_

#include <set>
#include <string>

#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

// A single ActionDecoder instance processes all P4Action nodes in a P4 program.
// Normal usage is to construct an ActionDecoder, then repeatedly call its
// ConvertActionBody method for each action specified by the program.
class ActionDecoder {
 public:
  // The constructor requires p4c's TypeMap and ReferenceMap and a
  // TableMapGenerator as injected dependencies, with the caller retaining
  // ownership of all pointers.  ActionDecoder expects the shared instance of
  // P4ModelNames to identify model-dependent prefixes, externs, and other
  // resources.
  ActionDecoder(TableMapGenerator* table_mapper,
                P4::ReferenceMap* ref_map, P4::TypeMap* type_map);

  // Converts the statements within one P4 action into a P4PipelineConfig table
  // map action descriptor.  The body parameter is the IR representation of the
  // statements and declarations within the action.  This method may recurse
  // upon encountering an IR::BlockStatement within the action.
  void ConvertActionBody(const std::string& p4_action_name,
                         const IR::IndexedVector<IR::StatOrDecl>& body);

  // ActionDecoder is neither copyable nor movable.
  ActionDecoder(const ActionDecoder&) = delete;
  ActionDecoder& operator=(const ActionDecoder&) = delete;

 private:
  // Handles a block of action statements, starting with the main body of
  // the action.  Nested block statements in the main body are handled
  // recursively.
  void ConvertActionBlock(const std::string& p4_action_name,
                          const IR::IndexedVector<IR::StatOrDecl>& block);

  // Handles MethodCallStatement variations in actions.
  void ConvertMethodCall(const IR::MethodCallStatement& method_call,
                         const std::string& p4_action_name);

  // Accumulates ActionDescriptor data in the output table map; injected and
  // owned by the caller of the constructor.
  TableMapGenerator* table_mapper_;

  // The p4c global TypeMap and ReferenceMap are injected via the constructor.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;

  // The processed_actions_ set identifies actions in the IR that
  // ActionDecoder has already mapped.  It avoids reprocessing actions
  // that appear multiple times in the IR.
  std::set<std::string> processed_actions_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_ACTION_DECODER_H_
