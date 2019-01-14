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

// This MidEnd implementation is adapted from the Open Source midend in
// p4lang_p4c/backends/p4test.

#include "stratum/p4c_backends/fpm/midend.h"

#include "absl/debugging/leak_check.h"
#include "p4lang_p4c/frontends/common/constantFolding.h"
#include "p4lang_p4c/frontends/common/resolveReferences/resolveReferences.h"
#include "p4lang_p4c/frontends/p4/evaluator/evaluator.h"
#include "p4lang_p4c/frontends/p4/fromv1.0/v1model.h"
#include "p4lang_p4c/frontends/p4/moveDeclarations.h"
#include "p4lang_p4c/frontends/p4/simplify.h"
#include "p4lang_p4c/frontends/p4/simplifyParsers.h"
#include "p4lang_p4c/frontends/p4/strengthReduction.h"
#include "p4lang_p4c/frontends/p4/toP4/toP4.h"
#include "p4lang_p4c/frontends/p4/typeMap.h"
#include "p4lang_p4c/frontends/p4/unusedDeclarations.h"
#include "p4lang_p4c/midend/compileTimeOps.h"
#include "p4lang_p4c/midend/copyStructures.h"
#include "p4lang_p4c/midend/eliminateTuples.h"
#include "p4lang_p4c/midend/expandLookahead.h"
#include "p4lang_p4c/midend/local_copyprop.h"
#include "p4lang_p4c/midend/midEndLast.h"
#include "p4lang_p4c/midend/nestedStructs.h"
#include "p4lang_p4c/midend/removeParameters.h"
#include "p4lang_p4c/midend/removeSelectBooleans.h"
#include "p4lang_p4c/midend/simplifyKey.h"
#include "p4lang_p4c/midend/simplifySelectCases.h"
#include "p4lang_p4c/midend/simplifySelectList.h"

namespace stratum {
namespace p4c_backends {

MidEnd::MidEnd(const CompilerOptions& options) {
  bool is_v1 = (options.langVersion == CompilerOptions::FrontendVersion::P4_14);
  reference_map_.setIsV1(is_v1);

  // It is not possible to simply record each of these new midend pass objects
  // and delete them after RunMidEndPass runs.  Many of them internally allocate
  // additional objects that they never delete, and which are not visible at
  // this level.
  absl::LeakCheckDisabler disable_pass_manager_leak_checks;
  auto evaluator = new P4::EvaluatorPass(&reference_map_, &type_map_);
  setName("MidEnd");

  addPasses({
    new P4::RemoveActionParameters(&reference_map_, &type_map_),
    new P4::SimplifyKey(&reference_map_, &type_map_,
                        new P4::OrPolicy(
                            new P4::IsValid(&reference_map_, &type_map_),
                            new P4::IsMask())),
    new P4::ConstantFolding(&reference_map_, &type_map_),
    new P4::SimplifySelectCases(&reference_map_, &type_map_, false),
    new P4::ExpandLookahead(&reference_map_, &type_map_),
    new P4::SimplifyParsers(&reference_map_),
    new P4::StrengthReduction(),
    new P4::EliminateTuples(&reference_map_, &type_map_),
    new P4::CopyStructures(&reference_map_, &type_map_),
    new P4::NestedStructs(&reference_map_, &type_map_),
    new P4::SimplifySelectList(&reference_map_, &type_map_),
    new P4::RemoveSelectBooleans(&reference_map_, &type_map_),
    new P4::MoveDeclarations(),  // more may have been introduced
    new P4::ConstantFolding(&reference_map_, &type_map_),
    new P4::LocalCopyPropagation(&reference_map_, &type_map_),
    new P4::ConstantFolding(&reference_map_, &type_map_),
    new P4::MoveDeclarations(),  // more may have been introduced
    new P4::SimplifyControlFlow(&reference_map_, &type_map_),
    new P4::CompileTimeOperations(),
    evaluator,
    new VisitFunctor([this, evaluator]() {
      top_level_ = evaluator->getToplevelBlock();
    }),
    new P4::MidEndLast()
  });
}

IR::ToplevelBlock* MidEnd::RunMidEndPass(const IR::P4Program& program) {
  if (mid_end_done_) {
    LOG(ERROR) << "The midend pass has already processed a P4Program";
    return nullptr;
  }

  program.apply(*this);
  mid_end_done_ = true;

  return top_level_;
}

std::unique_ptr<MidEndInterface> MidEnd::CreateInstance(
    CompilerOptions* options) {
  auto new_mid_end = new MidEnd(*options);
  new_mid_end->addDebugHook(options->getDebugHook());
  return std::unique_ptr<MidEndInterface>(new_mid_end);
}

}  // namespace p4c_backends
}  // namespace stratum
