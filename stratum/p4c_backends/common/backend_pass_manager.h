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

// This file declares a BackendPassManager class to oversee the execution
// of a Hercules-specific p4c backend.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_COMMON_BACKEND_PASS_MANAGER_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_COMMON_BACKEND_PASS_MANAGER_H_

#include "stratum/p4c_backends/common/backend_extension_interface.h"
#include "stratum/p4c_backends/common/p4c_front_mid_interface.h"

namespace stratum {
namespace p4c_backends {

// The BackendPassManager is constructed with 2 parameters, an implementation
// of P4cFrontMidInterface and a list of extensions to run.  BackendPassManager
// uses fe_me_interface to run the prerequisite parsing, frontend, and backend
// passes of p4c, and then it uses the outputs of these passes to invoke the
// provided backend extensions.
class BackendPassManager {
 public:
  BackendPassManager(P4cFrontMidInterface* fe_me_interface,
                     const std::vector<BackendExtensionInterface*>& extensions);
  virtual ~BackendPassManager() {}

  // Runs standard third-party frontend and midend passes, followed by
  // backend extensions in the order they appear in the constructor vector.
  // The p4c convention is to return 0 for success, 1 for errors.
  int Compile();

  // BackendPassManager is neither copyable nor movable.
  BackendPassManager(const BackendPassManager&) = delete;
  BackendPassManager& operator=(const BackendPassManager&) = delete;

 private:
  // These members store the injected parameters.  The BackendPassManager client
  // retains ownership.
  P4cFrontMidInterface* fe_me_interface_;
  const std::vector<BackendExtensionInterface*> extensions_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_COMMON_BACKEND_PASS_MANAGER_H_
