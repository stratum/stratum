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

// Does a comprehensive p4c test with p4lang_p4c test files.  It
// verifies two things:
// 1) The open source frontend and midend code functions properly in the
//    Stratum build environment.
// 2) The Stratum common backend code runs the frontend and midend libraries
//    in a valid sequence.
// Thorough testing of the Stratum backend functions is done by other tests.

#include <string>

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "gtest/gtest.h"

// This flag enables tests of P4_16 sample files in the open source code.
// It is normally disabled because:
//  - Two sample files cause crashes as the open-source P4RuntimeAnalyzer
//    processes value sets (appears to be fixed by p4c pull request #1404).
//  - The tests take several minutes to compile all the sample files.
//  - The main value of these tests is verifying new integrations of p4c
//    updates from github.
// To enable the sample file tests, add "--test_arg=--test_p4_16_samples" to
// the blaze test command line.
DEFINE_bool(test_p4_16_samples, true,
            "Enables test of P4_16 open source sample files");

DEFINE_string(p4c_binary_path,
              "stratum/p4c_backends/test/test_p4c",
              "Path to binary of compiler that will be tested.");

DEFINE_string(test_tmpdir, "", "Temp directory to be used for tests.");

DEFINE_string(test_srcdir, "external/com_github_p4lang_p4c", "Source directory of the P4 test files to feed to the p4c compiler.");

namespace stratum {
namespace p4c_backends {

// These helper functions for setting up test file vectors hide in an unnamed
// namespace.  The //p4lang_p4c build produces the objects in the
// p4c_test_files namespace.
namespace {

// Build a list of file paths to P4 test files. Bazel includes them as data deps in filegroup "testdata_p4_16_samples".
// We could (should?) build this list dynamically at runtime by reading the filesystem, but doing so is
// not portable until C++17 std::filesystem or would involve boost.
// TODO: include more test files
std::vector<std::string> CreateP4v16FileList() {
  return {
    "testdata/p4_16_samples/alias.p4",
    "testdata/p4_16_samples/arch1.p4",
    "testdata/p4_16_samples/arch2.p4",
    "testdata/p4_16_samples/extern.p4",
    "testdata/p4_16_samples/issue803-2.p4",
    };
}

}  // namespace

// This test fixture's parameter gives the P4 sample file for p4c input.
class P4cSampleFilesTest : public testing::TestWithParam<std::string> {
 public:
  // Per-test-case setup creates a set of the test files that are expected
  // to cause compiler failures.
  static void SetUpTestCase() {
    expected_failures_ = new std::set<std::string>;

    // In the open source p4c tests, these tests are identified by
    // "P4_XFAIL_TESTS" in backends/p4test/CMakeLists.txt.
    expected_failures_->insert("testdata/p4_16_samples/cast-call.p4");

    // These tests fail due to a bug in the p4RuntimeSerializer, which Stratum
    // tests detect.  They don't fail in p4lang_p4c tests because the test
    // backend doesn't use p4RuntimeSerializer.
    expected_failures_->insert("testdata/p4_16_samples/issue396.p4");
    expected_failures_->insert("testdata/p4_16_samples/issue561.p4");
    expected_failures_->insert("testdata/p4_16_samples/uninit.p4");
  }
  static void TearDownTestCase() {
    delete expected_failures_;
    expected_failures_ = nullptr;
  }

 protected:
  // Sets up the option string that gets passed to the p4c frontend and
  // midend.  These options are a single gflag.
  std::string SetUpP4cOptions(const std::string& p4_file) {
    // The first option is a dummy output file.
    std::string p4c_options = "--pp ";
    p4c_options += FLAGS_test_tmpdir + "/P4cSampleFilesTest-out.p4 ";

    // The compiler needs to tell the C preprocessor where it can find the
    // included .p4 files for various supported models.
    // Common headers like "core.p4" are automatically included.
    const std::string kP4RuntimeBase = FLAGS_test_srcdir;
    p4c_options += "-I";
    p4c_options += kP4RuntimeBase + "/p4include ";
    p4c_options += "-I";
    p4c_options += kP4RuntimeBase + "/backends/ebpf/p4include ";

    // The input .p4 file goes last.
    p4c_options += kP4RuntimeBase + "/" + p4_file;
    return p4c_options;
  }

  // Sets up the command line for running p4c via system(...), expecting
  // p4c_fe_options_ to be populated before entry.
  std::string SetUpP4cCommand() {
    std::string command = FLAGS_p4c_binary_path;
    command += " --p4c_fe_options=";
    command += "\"";
    command += p4c_fe_options_;
    command += "\"";
    return command;
  }

  static bool ExpectedFailure(const std::string& p4_file) {
    if (expected_failures_->find(p4_file) != expected_failures_->end())
      return true;
    return false;
  }

  std::string p4c_fe_options_;
  static std::set<std::string>* expected_failures_;
};

std::set<std::string>* P4cSampleFilesTest::expected_failures_ = nullptr;

// Since p4c has some non-reentrant code, the lex+yacc generated parsers
// in particular, the tests below need to run the p4c binary in a separate
// shell via system().  It is unsafe to repeatedly run the compiler by
// calling BackendPassManager::Compile().

// Tests all the p4_16 sample files.
// P4_16 file tests are disabled due to warning vs. error discrepancies in
// the Stratum vs p4lang_p4c variation of these tests.  Example: Stratum
// treats the lack of a P4 "main" as an error, whereas the p4lang_p4c tests
// call it a warning.
TEST_P(P4cSampleFilesTest, RunP4cTest) {
  if (!FLAGS_test_p4_16_samples) return;
  const std::string p4_test_file = GetParam();
  p4c_fe_options_ = SetUpP4cOptions(p4_test_file);
  std::string command = SetUpP4cCommand();
  int result = system(command.c_str());
  LOG(INFO) << "Compiler result is " << result;
  EXPECT_TRUE(result == 0 || ExpectedFailure(p4_test_file));
}

INSTANTIATE_TEST_SUITE_P(
    P4c16TestSamples,
    P4cSampleFilesTest,
    ::testing::ValuesIn(CreateP4v16FileList())
);

}  // namespace p4c_backends
}  // namespace stratum
