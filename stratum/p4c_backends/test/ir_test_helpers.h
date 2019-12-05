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

// The ir_test_helpers classes provide some common support for p4c_backends
// unit tests that need the compiler's P4 Internal Representation (IR) as input.

#ifndef STRATUM_P4C_BACKENDS_TEST_IR_TEST_HELPERS_H_
#define STRATUM_P4C_BACKENDS_TEST_IR_TEST_HELPERS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "external/com_github_p4lang_p4c/ir/ir.h"
#include "external/com_github_p4lang_p4c/lib/compile_context.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/p4c_backends/common/p4c_front_mid_real.h"
#include "stratum/p4c_backends/common/program_inspector.h"

namespace stratum {
namespace p4c_backends {

// Many p4c_backends tests need some IR data to use as input to the tested code.
// The IR structure is complex, so the data is not easy to generate in test
// fixtures.  The main purpose of this helper class is to run a .p4 file through
// the compiler front and mid end to produce an IR that can be used for further
// testing.  This approach frees test cases from the burden of IR setup.
//
// The IRTestHelperJson class requires a JSON representation of the IR as input.
// Test developers can run "p4c_ir_json_saver" to convert a P4 program into its
// JSON IR format, which is conveniently done by the "p4c_save_ir" BUILD rule.
// A sample workflow for tests that use this IR helper class is:
// 1) Write or locate a .p4 file that represents the case to be tested.
// 2) Add a BUILD rule to run the .p4 file through p4c_ir_json_saver and save
//    the IR data in a JSON file.
// 3) Make the test rule dependent on #2.
// 4) Use IRTestHelperJson to load the JSON file into IR data when the
//    test runs.
class IRTestHelperJson {
 public:
  // This enum defines the allowed transforms for TransformP4Control.
  enum IRControlTransforms {
    kHitAssignMapper,
    kMeterColorMapper,
  };

  IRTestHelperJson();
  ~IRTestHelperJson() {}

  // GenerateTestIR produces IR data from the input JSON file.  If successful,
  // the return value is true, and the IR data can be accessed via the
  // ir_top_level() accessor.  GenerateTestIR can only be called once; it
  // fails if called repeatedly.
  //
  // GenerateTestIR constructs an IR P4Program from the JSON file data.  It
  // runs the P4Program through a pre-processing pass to produce the P4 TypeMap,
  // ReferenceMap, and ToplevelBlock that backends expect to see.  These objects
  // are available to tests that need them via accessors.
  bool GenerateTestIR(const std::string& json_ir_file);

  // GenerateTestIRAndInspectProgram does the same things as GenerateTestIR.
  // In addition, it runs the generated P4Program through a ProgramInspector
  // to provide tests with more details about objects in the IR.  These details
  // are available through the program_inspector() accessor.  Since many tests
  // need access to specific P4Control nodes in the IR, this method also
  // produces a map of control-name to IR::P4Control nodes (see GetP4Control).
  bool GenerateTestIRAndInspectProgram(const std::string& json_ir_file);

  // Some tests need P4Info input to go along with the IR data.  When called
  // after a successful GenerateTestIR run, this method produces the P4Info
  // corresponding to the P4Program in the IR.
  bool GenerateP4Info(::p4::config::v1::P4Info* p4_info);

  // Maps the input control_name to an IR::P4Control node in the P4 program;
  // valid only after calling GenerateTestIRAndInspectProgram; returns nullptr
  // if control_name does not exist in the P4 program.
  const IR::P4Control* GetP4Control(const std::string& control_name) {
    return control_name_to_ir_node_[control_name];
  }

  // Runs the designated list of transforms on the input control_name, then
  // returns a pointer to the transformed control; valid only after calling
  // GenerateTestIRAndInspectProgram; returns nullptr if control_name does not
  // exist in the P4 program or an error occurs in one of the transforms.  The
  // caller must do any transform-required setup work before calling this
  // method, such as setting up P4ModelNames or calling the mutators below.
  // TransformP4Control runs the transforms in the order they appear within
  // transform_list.
  const IR::P4Control* TransformP4Control(
      const std::string& control_name,
      const std::vector<IRControlTransforms> transform_list);

  // These accessors should not be called before calling either GenerateTestIR
  // or GenerateTestIRAndInspectProgram.
  const IR::ToplevelBlock* ir_top_level() { return ir_top_level_; }
  P4::ReferenceMap* mid_end_refmap() { return &mid_end_refmap_; }
  P4::TypeMap* mid_end_typemap() { return &mid_end_typemap_; }

  // This accessor should not be called before GenerateTestIRAndInspectProgram.
  const ProgramInspector& program_inspector() const {
    return program_inspector_;
  }

  // Mutators for setting up TransformP4Control.
  void set_color_field_name(const std::string& field_name) {
    color_field_name_ = field_name;
  }

  // IRTestHelperJson is neither copyable nor movable.
  IRTestHelperJson(const IRTestHelperJson&) = delete;
  IRTestHelperJson& operator=(const IRTestHelperJson&) = delete;

 private:
  // Runs an abbreviated pseudo midend pass to generate ReferenceMap, TypeMap,
  // and ToplevelBlock objects.
  bool RunMapPass();

  // This member provides the p4c context for running tests.
  AutoCompileContext p4c_context_;

  // The ToplevelBlock refers to the IR data output from RunMapPass output.
  // This is the IR data that tests use.
  const IR::ToplevelBlock* ir_top_level_;

  // This P4Program is constructed from the JSON IR data.  This is the
  // IR data before GenerateTestIR transforms it into a ToplevelBlock.
  std::unique_ptr<IR::P4Program> program_;

  // The ReferenceMap and TypeMap are additional midend outputs.
  P4::ReferenceMap mid_end_refmap_;
  P4::TypeMap mid_end_typemap_;

  // These members are used by GenerateTestIRAndInspectProgram:
  //  program_inspector_ - Inspects program_ for IR nodes to test.
  //  control_name_to_ir_node_ - Maps control names to the IR nodes found by
  //      program_inspector_.
  ProgramInspector program_inspector_;
  std::map<std::string, const IR::P4Control*> control_name_to_ir_node_;

  // This member is required to run the MeterColorMapper transform from
  // TransformP4Control.
  std::string color_field_name_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_TEST_IR_TEST_HELPERS_H_
