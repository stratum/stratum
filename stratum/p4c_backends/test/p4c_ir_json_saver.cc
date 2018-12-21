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
#include "platforms/networking/hercules/p4c_backend/common/backend_extension_interface.h"
#include "platforms/networking/hercules/p4c_backend/common/backend_pass_manager.h"
#include "platforms/networking/hercules/p4c_backend/common/p4c_front_mid_real.h"
#include "platforms/networking/hercules/p4c_backend/switch/midend.h"

DEFINE_string(p4_to_json_in, "",
              "Input file with P4 program that generates IR data");
DEFINE_string(p4_to_json_out, "",
              "Output file for storing the IR data in JSON format");
DEFINE_bool(skip_p4c_cpp, false, "Disable the p4c C pre-processor pass");

DECLARE_string(p4c_fe_options);  // Options internal to open source p4c.

namespace google {
namespace hercules {
namespace p4c_backend {

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

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  google::hercules::p4c_backend::ConvertP4ToJson();
  return 0;
}
