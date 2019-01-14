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

// This file provides a p4c binary to generate a file that stores a p4c
// Internal Representation (IR) in JSON format.  Unit tests can use these files
// as a source for test IR data, with help from an IRTestHelperJson.  The
// p4c_ir_json_saver binary requires two flags, as shown by the usage below:
//
//  p4c_ir_json_saver --p4_to_json_in=<P4 program input file name> \
//      --p4_to_json_out=<JSON output file name>
//
// Authors of tests that need IR data can choose to run p4c_ir_json_saver
// manually and save the JSON output with other test input files.  They can
// also invoke p4c_ir_json_saver from the BUILD file with the p4c_save_ir
// rule.  The input P4 program must follow the P4_16 spec.

#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "base/commandlineflags.h"
#include "base/init_google.h"
#include "base/logging.h"
#include "stratum/p4c_backends/common/backend_extension_interface.h"
#include "stratum/p4c_backends/common/backend_pass_manager.h"
#include "stratum/p4c_backends/common/p4c_front_mid_real.h"
#include "stratum/p4c_backends/fpm/midend.h"

DEFINE_string(p4_to_json_in, "",
              "Input file with P4 program that generates IR data");
DEFINE_string(p4_to_json_out, "",
              "Output file for storing the IR data in JSON format");
DEFINE_bool(skip_p4c_cpp, false, "Disable the p4c C pre-processor pass");

DECLARE_string(p4c_fe_options);  // Options internal to open source p4c.

namespace stratum {
namespace p4c_backends {

// P4cFrontMidJson overrides P4cFrontMidReal's midend pass.  It replaces the
// normal midend logic with a conversion of the midend output program's IR to
// a JSON output file.  The conversion is done after the midend pass because
// IRTestHelperJson expects to receive a JSON file with the post-midend,
// pre-backend transformations of the IR.
class P4cFrontMidJson : public P4cFrontMidReal {
 public:
  explicit P4cFrontMidJson(MidEndCreateCallback callback)
      : P4cFrontMidReal(std::move(callback)) {}
  ~P4cFrontMidJson() override {}

  // Runs the normal midend pass in the base class, then captures the JSON IR.
  IR::ToplevelBlock* RunMidEndPass() override {
    IR::ToplevelBlock* top_level = P4cFrontMidReal::RunMidEndPass();
    std::ofstream json_file;
    json_file.open(FLAGS_p4_to_json_out);
    CHECK(json_file.is_open()) << "Unable to open JSON output file "
                               << FLAGS_p4_to_json_out;
    JSONGenerator gen(json_file);
    gen << top_level->getProgram() << std::endl;
    json_file.close();

    return top_level;
  }

  // No P4 runtime info is required when generating a JSON IR.
  void GenerateP4Runtime(std::ostream* out1, std::ostream* out2) override {}
};

namespace {

// Runs the compiler with the P4cFrontMidJson midend for JSON output.
void ConvertP4ToJson() {
  CHECK(!FLAGS_p4_to_json_in.empty()) << "Unspecified P4 program input file";
  CHECK(!FLAGS_p4_to_json_out.empty()) << "Unspecified JSON IR output file";;

  // These options tell the p4c frontend to process the input file as P4_16.
  FLAGS_p4c_fe_options = "";
  if (FLAGS_skip_p4c_cpp) FLAGS_p4c_fe_options += "--nocpp ";
  FLAGS_p4c_fe_options += FLAGS_p4_to_json_in;

  // The JSON IR for test input needs to be generated with the same midend
  // that the Hercules backend uses for normal compiles.
  auto midend_callback = std::function<std::unique_ptr<MidEndInterface>(
      CompilerOptions* p4c_options)>(&MidEnd::CreateInstance);
  std::unique_ptr<P4cFrontMidJson> p4c_json_fe_me(
      new P4cFrontMidJson(midend_callback));
  std::vector<BackendExtensionInterface*> no_extensions = {};
  std::unique_ptr<BackendPassManager> backend(
      new BackendPassManager(p4c_json_fe_me.get(), no_extensions));
  backend->Compile();
}

}  // namespace

}  // namespace p4c_backends
}  // namespace stratum

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  stratum::p4c_backends::ConvertP4ToJson();
  return 0;
}
