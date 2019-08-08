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

// This file implements the p4c BackendPassManager.
#include "stratum/p4c_backends/common/backend_pass_manager.h"

#include <wordexp.h>
#include <sstream>
#include <string>

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/utils.h"
#include "stratum/lib/macros.h"

#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"

DEFINE_string(p4c_fe_options, "",
              "Options passed to p4c frontend with p4c-specified syntax. "
              "For example, to process P4 spec file tor.p4 according to "
              "the P4 2014 spec, use --p4c_fe_options=\"--p4-14 tor.p4\".");
DEFINE_string(p4_info_file, "", "Output file where P4Info will be stored");

namespace stratum {
namespace p4c_backends {

BackendPassManager::BackendPassManager(
    P4cFrontMidInterface* fe_me_interface,
    const std::vector<BackendExtensionInterface*>& extensions)
    : fe_me_interface_(ABSL_DIE_IF_NULL(fe_me_interface)),
      extensions_(extensions) {}

int BackendPassManager::Compile() {
  if (FLAGS_p4c_fe_options.empty()) {
    LOG(ERROR) << "Missing options for p4c frontend";
    return 1;
  }

  // The options for the third-party front and midend code are all embedded
  // within a single gflag.  The code below uses wordexp to parse
  // FLAGS_p4c_fe_options into an argc/argv pair for the p4c CompilerOptions.
  const std::string p4c_command =
                          std::string(google::ProgramInvocationShortName()) +
                          std::string(" ") + FLAGS_p4c_fe_options;
  LOG(INFO) << "p4c compiler options: " << p4c_command;
  wordexp_t parsed_options = {0};
  int ret = wordexp(p4c_command.c_str(), &parsed_options,
                    WRDE_NOCMD | WRDE_SHOWERR);
  if (ret != 0) {
    LOG(ERROR) << "Unable to parse p4c frontend options to argc/argv";
    wordfree(&parsed_options);
    return 1;
  }

  fe_me_interface_->Initialize();
  int options_result = fe_me_interface_->ProcessCommandLineOptions(
      parsed_options.we_wordc, parsed_options.we_wordv);
  wordfree(&parsed_options);
  if (options_result) {
    LOG(ERROR) << "Options processing failed in p4c";
    return 1;
  }

  auto program = fe_me_interface_->ParseP4File();
  if (program == nullptr || fe_me_interface_->GetErrorCount() > 0) {
    LOG(ERROR) << "p4c failed to parse the input p4 file";
    return 1;
  }

  program = fe_me_interface_->RunFrontEndPass();
  if (program == nullptr || fe_me_interface_->GetErrorCount() > 0) {
    LOG(ERROR) << "The p4c front-end pass failed";
    return 1;
  }

  // The Stratum backend always produces the P4 runtime data. The data is
  // normally needed for subsequent backend use, after which it may or may not
  // be written to files based on flag input.  The stream output from p4c is
  // in serialized binary wire format.
  std::stringstream p4_info_stream;
  std::stringstream static_entries_stream;
  fe_me_interface_->GenerateP4Runtime(&p4_info_stream, &static_entries_stream);
  if (fe_me_interface_->GetErrorCount()) {
    LOG(ERROR) << "P4 runtime generation failed";
    return 1;
  }

  ::p4::config::v1::P4Info p4_info;
  if (!p4_info.ParseFromString(p4_info_stream.str())) {
    LOG(ERROR) << "P4Info serialized output from compiler does not parse";
    return 1;
  }

  auto top_level = fe_me_interface_->RunMidEndPass();
  if (fe_me_interface_->GetErrorCount() > 0) {
    LOG(ERROR) << "The p4c mid-end pass failed";
    return 1;
  }

  // The lack of a top-level pointer after the mid-end pass most likely means
  // that the P4 program has no 'main' module, which is common in many of the
  // open source P4 program samples.  The behavior here depends on whether
  // there are backend extensions to run.  If there are no extensions, then
  // there is no significant work left to do on the P4 program.  This is
  // probably a sample file test to exercise the front and mid-end code, and
  // the BackendPassManager returns with just a warning.  Otherwise, the
  // extensions can't run without a top_level, so this produces an error.
  if (top_level == nullptr) {
    if (extensions_.empty()) {
      LOG(WARNING)
          << "Ignoring missing top-level program.  Does \'main\' exist?";
      return 0;
    } else {
      LOG(ERROR) << "P4 program needs top-level program to complete back-end "
                    "processing.  Does \'main\' exist?";
      return 1;
    }
  }

  ::p4::v1::WriteRequest static_entries;
  if (!static_entries.ParseFromString(static_entries_stream.str())) {
    LOG(ERROR) << "P4 static table entries serialized output from compiler "
               << "does not parse";
    return 1;
  }

  // Run all extensions.
  for (auto extension : extensions_) {
    extension->Compile(*top_level, static_entries, p4_info,
                       fe_me_interface_->GetMidEndReferenceMap(),
                       fe_me_interface_->GetMidEndTypeMap());
  }

  if (fe_me_interface_->GetErrorCount()) {
    LOG(ERROR) << "Backend extensions failed";
    return 1;
  }

  if (!FLAGS_p4_info_file.empty()) {
    if (!WriteProtoToTextFile(p4_info, FLAGS_p4_info_file).ok())
      LOG(ERROR) << "Failed to write p4Info to " << FLAGS_p4_info_file;
  }

  return fe_me_interface_->GetErrorCount() > 0;
}

}  // namespace p4c_backends
}  // namespace stratum
