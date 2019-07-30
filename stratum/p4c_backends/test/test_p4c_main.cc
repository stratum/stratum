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

// This main program runs a p4c binary for testing.  The test p4c binary
// currently runs without any backend extensions.

#include <memory>
#include <vector>

#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/p4c_backends/common/backend_extension_interface.h"
#include "stratum/p4c_backends/common/backend_pass_manager.h"
#include "stratum/p4c_backends/common/p4c_front_mid_real.h"

using stratum::p4c_backends::BackendExtensionInterface;
using stratum::p4c_backends::BackendPassManager;
using stratum::p4c_backends::P4cFrontMidReal;

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  stratum::InitStratumLogging();
  std::unique_ptr<P4cFrontMidReal> p4c_real_fe_me(new P4cFrontMidReal);
  std::vector<BackendExtensionInterface*> no_extensions = {};
  std::unique_ptr<BackendPassManager> backend(
      new BackendPassManager(p4c_real_fe_me.get(), no_extensions));
  backend->Compile();
}
