// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file is a P4cFrontMidInterface that runs the open source p4c code.

#include "stratum/p4c_backends/common/p4c_front_mid_real.h"

#include <utility>

#include "stratum/glue/logging.h"
#include "stratum/p4c_backends/common/midend_p4c_open.h"
#include "absl/debugging/leak_check.h"
#include "absl/memory/memory.h"
#include "external/com_github_p4lang_p4c/control-plane/p4RuntimeSerializer.h"
#include "external/com_github_p4lang_p4c/frontends/common/parseInput.h"
#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeMap.h"
#include "external/com_github_p4lang_p4c/lib/crash.h"
#include "external/com_github_p4lang_p4c/lib/error.h"
#include "external/com_github_p4lang_p4c/lib/gc.h"

using StratumP4cContext = P4CContextWithOptions<CompilerOptions>;

namespace stratum {
namespace p4c_backends {

P4cFrontMidReal::P4cFrontMidReal()
    : p4c_context_(new StratumP4cContext),
      p4c_options_(StratumP4cContext::get().options()),
      p4_program_(nullptr) {
  mid_end_callback_ = std::function<std::unique_ptr<MidEndInterface>(
      CompilerOptions* p4c_options)>(&P4cFrontMidReal::CreateDefaultMidend);
}

P4cFrontMidReal::P4cFrontMidReal(MidEndCreateCallback callback)
    : p4c_context_(absl::IgnoreLeak(new StratumP4cContext)),
      p4c_options_(StratumP4cContext::get().options()),
      p4_program_(nullptr),
      mid_end_callback_(std::move(callback)) {}

void P4cFrontMidReal::Initialize() {
  setup_gc_logging();
  setup_signals();
}

int P4cFrontMidReal::ProcessCommandLineOptions(int argc, char* const argv[]) {
  p4c_options_.langVersion = CompilerOptions::FrontendVersion::P4_16;
  if (p4c_options_.process(argc, argv) != nullptr)
    p4c_options_.setInputFile();
  else
    return 1;
  if (::errorCount() > 0)
    return 1;
  return 0;
}

const IR::P4Program* P4cFrontMidReal::ParseP4File() {
  absl::LeakCheckDisabler disable_p4_parse_leak_check;
  p4_program_ = P4::parseP4File(p4c_options_);
  return p4_program_;
}

const IR::P4Program* P4cFrontMidReal::RunFrontEndPass() {
  CHECK(p4_program_ != nullptr) << "No parsed P4 program for frontend pass";
  front_end_ = absl::make_unique<P4::FrontEnd>();
  front_end_->addDebugHook(p4c_options_.getDebugHook());
  absl::LeakCheckDisabler disable_front_end_leak_check;
  p4_program_ = front_end_->run(p4c_options_, p4_program_);
  return p4_program_;
}

IR::ToplevelBlock* P4cFrontMidReal::RunMidEndPass() {
  CHECK(p4_program_ != nullptr) << "No parsed P4 program for midend pass";
  CHECK(front_end_ != nullptr) << "P4c frontend pass has not run";
  mid_end_ = mid_end_callback_(&p4c_options_);

  // The midend pass is likely to transform the input P4 program into a new one.
  absl::LeakCheckDisabler disable_mid_end_leak_check;
  IR::ToplevelBlock* top_level = mid_end_->RunMidEndPass(*p4_program_);
  if (top_level != nullptr) p4_program_ = top_level->getProgram();
  return top_level;
}

void P4cFrontMidReal::GenerateP4Runtime(
    std::ostream* p4info_out, std::ostream* static_table_entries_out) {
  CHECK(front_end_ != nullptr) << "P4c frontend pass has not run";
  absl::LeakCheckDisabler disable_p4_runtime_leak_check;
  auto p4_runtime = P4::generateP4Runtime(p4_program_);

  // The p4_runtime API has public pointers to the proto buffers that
  // get serialized below.  This method's stream parameters could be
  // replaced by P4Info and WriteRequest pointers, and the statements
  // below could simply copy the data referenced by the p4_runtime pointers,
  // eliminating a serialize and deserialize step.  This implementation seems
  // safer and avoids potential abuse of a poor choice to make the pointers
  // in generateP4Runtime publicly visible.
  p4_runtime.serializeP4InfoTo(p4info_out, P4::P4RuntimeFormat::BINARY);
  p4_runtime.serializeEntriesTo(static_table_entries_out,
                                P4::P4RuntimeFormat::BINARY);
}

unsigned P4cFrontMidReal::GetErrorCount() {
  return ::errorCount();
}

P4::ReferenceMap* P4cFrontMidReal::GetMidEndReferenceMap() {
  CHECK(mid_end_ != nullptr) << "P4c midend pass has not run";
  return mid_end_->reference_map();
}

P4::TypeMap* P4cFrontMidReal::GetMidEndTypeMap() {
  CHECK(mid_end_ != nullptr) << "P4c midend pass has not run";
  return mid_end_->type_map();
}

bool P4cFrontMidReal::IsV1Program() const {
  return p4c_options_.langVersion == CompilerOptions::FrontendVersion::P4_14;
}

std::unique_ptr<MidEndInterface> P4cFrontMidReal::CreateDefaultMidend(
    CompilerOptions* p4c_options) {
  return std::unique_ptr<MidEndInterface>(new MidEndP4cOpen(p4c_options));
}

}  // namespace p4c_backends
}  // namespace stratum
