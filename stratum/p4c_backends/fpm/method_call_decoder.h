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

// The MethodCallDecoder processes IR::MethodCallStatement or
// IR::MethodCallExpression nodes from the P4 program and attempts to convert
// them into P4ActionInstructions messages.  It is intended for general use
// where method calls appear within P4Control blocks or in P4Action bodies,
// so it is unaware of the method call's context.  Normal usage when an action
// or control visitor encounters an IR::MethodCallStatement/Expression is to
// construct a MethodCallDecoder, call the applicable Decode method to process
// the statement or expression, and then evaluate the output to see whether
// it is valid within the current control or action context.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_METHOD_CALL_DECODER_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_METHOD_CALL_DECODER_H_

#include <string>

#include "stratum/hal/lib/p4/p4_table_map.host.pb.h"
#include "p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"

namespace stratum {
namespace p4c_backends {

class MethodCallDecoder {
 public:
  // The constructor requires p4c's TypeMap and ReferenceMap as injected
  // dependencies, with the caller retaining ownership of all pointers.
  // The shared instance of P4ModelNames should be setup with built-in
  // externs from the P4 model before calling the constructor.
  MethodCallDecoder(P4::ReferenceMap* ref_map, P4::TypeMap* type_map);
  virtual ~MethodCallDecoder() {}

  // The Decode methods process one MethodCallStatement or MethodCallExpression
  // from the P4 program.  If successful, Decode methods return true and store
  // P4ActionInstructions for the input, which the caller can access by calling
  // the method_op() and tunnel_op() accessors.  When a Decode fails, it
  // returns false and supplies an error string via the error_message()
  // accessor.  MethodCallDecoder itself does not use p4c's ErrorReporter
  // since some callers may need to try other decoding strategies on the input
  // method call before declaring an error condition.
  bool DecodeStatement(const IR::MethodCallStatement& method_call);
  bool DecodeExpression(const IR::MethodCallExpression& method_call);

  // These accessors return an output message after a Decode method returns
  // successfully.  The content is not valid before a decode runs or after
  // a decode returns an error.  A successful Decode operation produces
  // data for one of the accessors, but not both.
  const hal::P4ActionDescriptor::P4ActionInstructions& method_op() const {
    return method_op_;
  }
  const hal::P4ActionDescriptor::P4TunnelAction& tunnel_op() const {
    return tunnel_op_;
  }

  // This accessor returns a string that the caller can pass to p4c's
  // ErrorReporter if it chooses.  The result string will be empty unless
  // Decode has returned a failure indication.
  const std::string& error_message() const { return error_message_; }

  // MethodCallDecoder is neither copyable nor movable.
  MethodCallDecoder(const MethodCallDecoder&) = delete;
  MethodCallDecoder& operator=(const MethodCallDecoder&) = delete;

 private:
  // These methods decode the clone3/clone extern functions, returning
  // true if successful.
  bool DecodeClone3(const P4::ExternFunction& clone_extern);
  bool DecodeClone(const P4::ExternFunction& clone_extern);

  // These methods decode the counter and meter extern methods, returning true
  // if successful.
  bool DecodeDirectCounter(const P4::ExternMethod& counter_extern);
  bool DecodeCounter(const P4::ExternMethod& counter_extern);
  bool DecodeDirectMeter(const P4::ExternMethod& meter_extern);
  bool DecodeMeter(const P4::ExternMethod& meter_extern);

  // Decodes built-in methods, such as setValid/setInvalid.
  bool DecodeBuiltIn(const P4::BuiltInMethod& built_in);

  // These members record the injected constructor parameters.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;

  // Becomes true after Decode has run at least once.
  bool decode_done_;

  // The Decode method stores its output in these members.
  // TODO(teverman): Consolidate both types of output into a single
  // hal::P4ActionDescriptor.
  hal::P4ActionDescriptor::P4ActionInstructions method_op_;
  hal::P4ActionDescriptor::P4TunnelAction tunnel_op_;

  // When Decode fails, it populates this string with an error message that
  // the caller can pass on to p4c's ErrorReporter if desired.
  std::string error_message_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_METHOD_CALL_DECODER_H_
