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

// The MidEndInterface defines a common set of methods to access a p4c midend.
// It allows custom midends and open source midends to be used interchangeably
// as long as a wrapper implementation exists.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_COMMON_MIDEND_INTERFACE_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_COMMON_MIDEND_INTERFACE_H_

#include "p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "p4lang_p4c/frontends/p4/typeMap.h"
#include "p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

// The MidEndInterface conforms to the Interface class requirements.
class MidEndInterface {
 public:
  virtual ~MidEndInterface() {}

  // Executes the midend pass on the input P4Program.  Midends typically
  // run once per instance to process a single program.  A nullptr return
  // indicates failure.
  virtual IR::ToplevelBlock* RunMidEndPass(const IR::P4Program& program) = 0;

  // Accessors to common midend objects - not valid until after RunMidEndPass.
  virtual IR::ToplevelBlock* top_level() = 0;
  virtual P4::ReferenceMap* reference_map() = 0;
  virtual P4::TypeMap* type_map() = 0;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_COMMON_MIDEND_INTERFACE_H_
