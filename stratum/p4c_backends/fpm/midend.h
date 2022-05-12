// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This midend is a custom p4c midend for Stratum switches.  Its main
// difference relative to available p4c open-source midends is the elimination
// of some IR passes that synthesize temporary tables and actions that tune
// the IR for the bmv2 pipeline.

#ifndef STRATUM_P4C_BACKENDS_FPM_MIDEND_H_
#define STRATUM_P4C_BACKENDS_FPM_MIDEND_H_

#include <memory>

#include "external/com_github_p4lang_p4c/frontends/common/options.h"
#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeMap.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"
#include "stratum/p4c_backends/common/midend_interface.h"

namespace stratum {
namespace p4c_backends {

// This MidEnd class inherits from the third-party p4c PassManager and conforms
// to the Stratum MidEndInterface class.
class MidEnd : public PassManager, MidEndInterface {
 public:
  ~MidEnd() override {}

  // Base class overrides to provide the common midend interface.
  IR::ToplevelBlock* RunMidEndPass(const IR::P4Program& program) override;
  IR::ToplevelBlock* top_level() override { return top_level_; }
  P4::ReferenceMap* reference_map() override { return &reference_map_; }
  P4::TypeMap* type_map() override { return &type_map_; }

  // Conforms with P4cFrontMidReal::MidEndCreateCallback syntax.
  static std::unique_ptr<MidEndInterface> CreateInstance(
      CompilerOptions* options);

  // MidEnd is neither copyable nor movable.
  MidEnd(const MidEnd&) = delete;
  MidEnd& operator=(const MidEnd&) = delete;

 private:
  // CreateInstance calls the private constructor.
  explicit MidEnd(const CompilerOptions& options);

  // These members support the common midend interface.
  P4::ReferenceMap reference_map_;
  P4::TypeMap type_map_;
  IR::ToplevelBlock* top_level_ = nullptr;

  bool mid_end_done_ = false;  // Becomes true in RunMidEndPass.
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_MIDEND_H_
