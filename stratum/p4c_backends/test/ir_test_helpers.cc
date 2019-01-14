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

// IRTestHelperP4 class implementation.

#include "stratum/p4c_backends/test/ir_test_helpers.h"

#include <fstream>
#include <memory>
#include <sstream>
#include "base/commandlineflags.h"
#include "base/logging.h"
#include "stratum/p4c_backends/common/backend_extension_interface.h"
#include "stratum/p4c_backends/common/backend_pass_manager.h"
#include "stratum/p4c_backends/fpm/hit_assign_mapper.h"
#include "stratum/p4c_backends/fpm/meter_color_mapper.h"
#include "absl/memory/memory.h"
#include "p4lang_p4c/backends/p4test/midend.h"
#include "p4lang_p4c/control-plane/p4RuntimeSerializer.h"
#include "p4lang_p4c/ir/json_loader.h"
#include "sandblaze/p4lang/p4/config/v1/p4info.host.pb.h"

DECLARE_string(p4c_fe_options);  // Specifies the p4 spec file.

namespace stratum {
namespace p4c_backends {

IRTestHelperJson::IRTestHelperJson()
    : p4c_context_(new P4CContextWithOptions<CompilerOptions>),
      ir_top_level_(nullptr) {
}

bool IRTestHelperJson::GenerateTestIR(const std::string& json_ir_file) {
  bool ir_valid = false;

  // The p4c internal ErrorReporter contains static data.  If a previous
  // test generates a p4c error without clearing the ErrorReporter, it can
  // cause IRTestHelperJson to fail in obscure ways.
  CHECK(!::errorCount()) << "Make sure prior tests clear the p4c ErrorReporter";

  if (ir_top_level_ != nullptr) {
    LOG(ERROR) << "GenerateTestIR/GenerateTestIRAndInspectProgram cannot "
               << "be called repeatedly";
    return false;
  }

  std::ifstream json_file;
  json_file.open(json_ir_file);
  if (json_file.is_open()) {
    JSONLoader loader(json_file);
    program_ = absl::make_unique<IR::P4Program>(loader);
    json_file.close();
    ir_valid = RunMapPass();
  } else {
    LOG(ERROR) << "Unable to open JSON IR input file " << json_ir_file;
  }
  return ir_valid;
}

bool IRTestHelperJson::GenerateTestIRAndInspectProgram(
    const std::string& json_ir_file) {
  if (!GenerateTestIR(json_ir_file))
    return false;
  ir_top_level_->getProgram()->apply(program_inspector_);
  for (auto control : program_inspector_.controls()) {
    control_name_to_ir_node_[std::string(control->externalName())] = control;
  }
  return true;
}

bool IRTestHelperJson::GenerateP4Info(::p4::config::v1::P4Info* p4_info) {
  if (ir_top_level_ == nullptr) {
    LOG(ERROR) << "GenerateP4Info was called before successfully "
               << "running GenerateTestIR";
    return false;
  }

  auto p4_runtime = P4::generateP4Runtime(program_.get());
  *p4_info = *p4_runtime.p4Info;

  return true;
}

const IR::P4Control* IRTestHelperJson::TransformP4Control(
    const std::string& control_name,
    const std::vector<IRControlTransforms> transform_list) {
  const IR::P4Control* ir_control = GetP4Control(control_name);
  if (ir_control == nullptr) return nullptr;
  for (auto transform : transform_list) {
    switch (transform) {
      case kHitAssignMapper:
        ir_control = HitAssignMapper::RunPreTestTransform(
            *ir_control, &mid_end_refmap_, &mid_end_typemap_);
        break;
      case kMeterColorMapper:
        // MeterColorMapper needs to be able to lookup the color
        // field type.
        CHECK(!color_field_name_.empty());
        ir_control = MeterColorMapper::RunPreTestTransform(
            *ir_control, color_field_name_,
            &mid_end_refmap_, &mid_end_typemap_);
        break;
    }
    if (::errorCount() != 0) return nullptr;
  }

  return ir_control;
}

bool IRTestHelperJson::RunMapPass() {
  mid_end_refmap_.setIsV1(true);  // TODO(teverman): Handle PSA.

  // This simplified invocation of PassManager populates mid_end_refmap_ and
  // mid_end_typemap_.  The EvaluatorPass constructs a ToplevelBlock from
  // the P4Program.
  auto evaluator = new P4::EvaluatorPass(&mid_end_refmap_, &mid_end_typemap_);
  PassManager passes = {
    new P4::ResolveReferences(&mid_end_refmap_),
    new P4::TypeInference(&mid_end_refmap_, &mid_end_typemap_),
    evaluator,
  };

  const IR::P4Program* p4_program = program_->apply(passes);
  if (p4_program == nullptr) {
    LOG(ERROR) << "Map pass was unable to process P4 program";
    return false;
  }

  // Tests need an IR top level block, which is constructed here from the
  // P4 program.
  ir_top_level_ = evaluator->getToplevelBlock();

  return true;
}

}  // namespace p4c_backends
}  // namespace stratum
