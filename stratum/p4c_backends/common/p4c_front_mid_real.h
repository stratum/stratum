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

// This file declares a subclass of P4cFrontMidInterface that works with the
// real p4c frontend and midend passes.

#ifndef STRATUM_P4C_BACKENDS_COMMON_P4C_FRONT_MID_REAL_H_
#define STRATUM_P4C_BACKENDS_COMMON_P4C_FRONT_MID_REAL_H_

#include <functional>
#include <memory>

#include "stratum/p4c_backends/common/midend_interface.h"
#include "stratum/p4c_backends/common/p4c_front_mid_interface.h"
#include "external/com_github_p4lang_p4c/frontends/common/options.h"
#include "external/com_github_p4lang_p4c/frontends/p4/frontend.h"
#include "external/com_github_p4lang_p4c/lib/compile_context.h"

namespace stratum {
namespace p4c_backends {

// In the p4c implementation, the lex/yacc-generated parsers are not reentrant.
// Thus, it is only possible to run the frontend and midend once.  This
// limitation is noteworthy for unit tests.  It is not possible for a test
// to run a pass over a P4 spec file, generate an IR, use the IR as test data,
// and then repeat in the next test.
class P4cFrontMidReal : public P4cFrontMidInterface {
 public:
  // This callback type facilitates the use of alternate midends.  If the
  // default constructor creates the P4cFrontMidReal instance, then the midend
  // pass uses the p4c/backends/p4test midend as its implementation.  If the
  // constructor with callback parameter creates the instance, then the
  // callback runs during RunMidEndPass to create a custom midend.  This class
  // takes ownership of the callback's returned MidEndInterface pointer.
  typedef std::function<std::unique_ptr<MidEndInterface>(
      CompilerOptions* p4c_options)> MidEndCreateCallback;

  P4cFrontMidReal();
  explicit P4cFrontMidReal(MidEndCreateCallback callback);
  ~P4cFrontMidReal() override {}

  // This class provides real implementations of these base class interfaces.
  void Initialize() override;
  int ProcessCommandLineOptions(int argc, char* const argv[]) override;
  const IR::P4Program* ParseP4File() override;
  const IR::P4Program* RunFrontEndPass() override;
  IR::ToplevelBlock* RunMidEndPass() override;
  void GenerateP4Runtime(std::ostream* p4info_out,
                         std::ostream* static_table_entries_out) override;
  unsigned GetErrorCount() override;
  P4::ReferenceMap* GetMidEndReferenceMap() override;
  P4::TypeMap* GetMidEndTypeMap() override;
  bool IsV1Program() const override;

  // P4cFrontMidReal is neither copyable nor movable.
  P4cFrontMidReal(const P4cFrontMidReal&) = delete;
  P4cFrontMidReal& operator=(const P4cFrontMidReal&) = delete;

 private:
  // The midend can mutate the options, but takes no ownership.
  static std::unique_ptr<MidEndInterface> CreateDefaultMidend(
      CompilerOptions* p4c_options);

  // These members record the major compiler objects required for running the
  // p4c frontend and midend.  P4Program ownership remains with p4c internals.
  AutoCompileContext p4c_context_;
  CompilerOptions& p4c_options_;
  const IR::P4Program* p4_program_;
  std::unique_ptr<P4::FrontEnd> front_end_;
  std::unique_ptr<MidEndInterface> mid_end_;

  // This callback creates the midend.  For a P4cFrontMidReal created by the
  // default constructor, it refers to the CreateDefaultMidend method.
  // Otherwise, it refers to the constructor-injected callback.
  MidEndCreateCallback mid_end_callback_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_COMMON_P4C_FRONT_MID_REAL_H_
