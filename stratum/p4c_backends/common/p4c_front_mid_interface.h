// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file declares an interface to frontend and midend code dependencies
// in the p4c third-party code.  Backend modules can use it to support their
// frontend and midend passes.

#ifndef STRATUM_P4C_BACKENDS_COMMON_P4C_FRONT_MID_INTERFACE_H_
#define STRATUM_P4C_BACKENDS_COMMON_P4C_FRONT_MID_INTERFACE_H_

#include <iostream>

// These forward references are necessary because third-party p4c likes
// to give some types the same names as objects in the std namespace.  If
// all the p4c dependencies are included here, it is likely to cause
// compiler ambiguities where these conflicts exist.
namespace IR {
class P4Program;
class ToplevelBlock;
}  // namespace IR

namespace P4 {
class ReferenceMap;
class TypeMap;
}  // namespace P4

namespace stratum {
namespace p4c_backends {

class P4cFrontMidInterface {
 public:
  virtual ~P4cFrontMidInterface() {}

  // Does common p4c setup of the compiler's internal logging and signal
  // catching.
  virtual void Initialize() = 0;

  // Method for managing p4c's internal options.  A Stratum backend reserves
  // one gflag string for all of the p4c open source code's options.  Before
  // running the front end pass, the backend must parse this string into argc
  // and argv, then pass the result into ProcessCommandLineOptions.  Refer to
  // code in BackendPassManager for an example.  A sample command line appears
  // below:
  //  blaze-bin/<path-to-compiler/p4c-fpm \
  //      --p4_info_file=/tmp/p4c_tor_p4_info.txt \
  //      --p4c_fe_options="--p4-14 ~/tmp_new_p4/tor_cc69e56.p4"
  virtual int ProcessCommandLineOptions(int argc, char* const argv[]) = 0;

  // Methods to parse input file, then run frontend and midend passes.  A
  // backend must call these methods in the order listed below.  The
  // P4cFrontMidInterface implementation retains ownership of the returned
  // pointers, which refer to the compiler's internal representation of the
  // P4 spec.
  virtual const IR::P4Program* ParseP4File() = 0;
  virtual const IR::P4Program* RunFrontEndPass() = 0;
  virtual IR::ToplevelBlock* RunMidEndPass() = 0;

  // Generates P4 runtime protocol buffer output in serialized binary format.
  // A backend can call this method any time after the midend pass finishes.
  // The first ostream contains the serialized P4Info, and the second ostream
  // contains a serialized ::p4::WriteRequest with all static table entries
  // from the P4 program.
  virtual void GenerateP4Runtime(std::ostream* p4info_out,
                                 std::ostream* static_table_entries_out) = 0;

  // Retrieves the compiler's internal error count.  A backend should check for
  // a non-zero result after each compiler pass.
  virtual unsigned GetErrorCount() = 0;

  // Methods providing access to the midend's ReferenceMap and TypeMap.  The
  // midend pass must run before these are available.
  virtual P4::ReferenceMap* GetMidEndReferenceMap() = 0;
  virtual P4::TypeMap* GetMidEndTypeMap() = 0;

  // Returns true if the input program is a P4_14/V1 program; must be called
  // after ProcessCommandLineOptions.
  virtual bool IsV1Program() const = 0;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_COMMON_P4C_FRONT_MID_INTERFACE_H_
