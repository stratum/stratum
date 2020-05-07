// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file declares a MidEndInterface wrapper around the open-source p4c
// P4Test midend.  This midend acts as the default midend pass for the
// Stratum p4c backend when it is not overridden by a custom midend.

#ifndef STRATUM_P4C_BACKENDS_COMMON_MIDEND_P4C_OPEN_H_
#define STRATUM_P4C_BACKENDS_COMMON_MIDEND_P4C_OPEN_H_

#include <memory>

#include "stratum/p4c_backends/common/midend_interface.h"
#include "external/com_github_p4lang_p4c/backends/p4test/midend.h"
#include "external/com_github_p4lang_p4c/frontends/common/options.h"

namespace stratum {
namespace p4c_backends {

// The open-source P4Test::MidEnd class is not a subclass of MidEndInterface,
// so this wrapper adapts P4Test::MidEnd's public members to comply with the
// interface definition.
class MidEndP4cOpen : public MidEndInterface {
 public:
  // The caller retains ownership of the options, which may be modified by
  // the midend.
  explicit MidEndP4cOpen(CompilerOptions* p4c_options);
  ~MidEndP4cOpen() override {}

  // Base class overrides to wrap the interface around P4Test::MidEnd.
  IR::ToplevelBlock* RunMidEndPass(const IR::P4Program& program) override;
  IR::ToplevelBlock* top_level() override { return mid_end_->toplevel; }
  P4::ReferenceMap* reference_map() override { return &mid_end_->refMap; }
  P4::TypeMap* type_map() override { return &mid_end_->typeMap; }

  // MidEndP4cOpen is neither copyable nor movable.
  MidEndP4cOpen(const MidEndP4cOpen&) = delete;
  MidEndP4cOpen& operator=(const MidEndP4cOpen&) = delete;

 private:
  CompilerOptions* p4c_options_;  // Injected p4c options - not owned.

  // Open source midend behind this MidEndInterface subclass.
  std::unique_ptr<P4Test::MidEnd> mid_end_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_COMMON_MIDEND_P4C_OPEN_H_
