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

// This file implements the Stratum p4c backend's MethodCallDecoder.

#include "stratum/p4c_backends/fpm/method_call_decoder.h"

#include "stratum/p4c_backends/fpm/field_name_inspector.h"
#include "stratum/p4c_backends/fpm/p4_model_names.pb.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "stratum/glue/logging.h"
#include "absl/debugging/leak_check.h"
#include "absl/strings/substitute.h"
#include "external/com_github_p4lang_p4c/frontends/p4/methodInstance.h"

namespace stratum {
namespace p4c_backends {

MethodCallDecoder::MethodCallDecoder(P4::ReferenceMap* ref_map,
                                     P4::TypeMap* type_map)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)),
      decode_done_(false) {}

bool MethodCallDecoder::DecodeStatement(
    const IR::MethodCallStatement& method_call) {
  return DecodeExpression(*method_call.methodCall);
}

bool MethodCallDecoder::DecodeExpression(
    const IR::MethodCallExpression& method_call) {
  if (decode_done_) {
    error_message_ = "This MethodCallDecoder instance has already processed "
                     "a MethodCallStatement";
    LOG(ERROR) << error_message_;
    return false;
  }

  decode_done_ = true;
  bool decode_ok = false;
  P4::MethodInstance* method_i = nullptr;
  {
    absl::LeakCheckDisabler disable_ir_method_instance_leak_checks;
    method_i = P4::MethodInstance::resolve(&method_call, ref_map_, type_map_);
  }
  const auto& p4_model_names = GetP4ModelNames();

  if (method_i->is<P4::ActionCall>()) {
    // This is a frontend/midend bug per bmv2 backend ConvertActions.
    BUG("%1%: action call should have been inlined", &method_call);
  } else if (method_i->is<P4::BuiltInMethod>()) {
    decode_ok = DecodeBuiltIn(*method_i->to<P4::BuiltInMethod>());
  } else if (method_i->is<P4::ExternMethod>()) {
    // TODO(unknown): More extern method call implementation is needed below.
    auto extern_method = method_i->to<P4::ExternMethod>();
    const std::string extern_name =
        extern_method->originalExternType->name.name.c_str();
    if (extern_name == p4_model_names.direct_counter_extern_name()) {
      decode_ok = DecodeDirectCounter(*extern_method);
    } else if (extern_name == p4_model_names.counter_extern_name()) {
      decode_ok = DecodeCounter(*extern_method);
    } else if (extern_name == p4_model_names.direct_meter_extern_name()) {
      decode_ok = DecodeDirectMeter(*extern_method);
    } else if (extern_name == p4_model_names.meter_extern_name()) {
      decode_ok = DecodeMeter(*extern_method);
    }
    if (!decode_ok) {
      error_message_ = absl::Substitute(
          "Ignoring extern method: $0", extern_name.c_str());
    }
  } else if (method_i->is<P4::ExternFunction>()) {
    auto extern_function = method_i->to<P4::ExternFunction>();
    const std::string function_name(extern_function->method->name.toString());
    if (function_name == p4_model_names.drop_extern_name()) {
      method_op_.add_primitives(P4_ACTION_OP_DROP);
      decode_ok = true;
    } else if (function_name == p4_model_names.clone_extern_name()) {
      decode_ok = DecodeClone(*extern_function);
    } else if (function_name == p4_model_names.clone3_extern_name()) {
      decode_ok = DecodeClone3(*extern_function);
    } else {
      // TODO(unknown): Other extern function implementations needed may
      // include hash, resubmit, recirculate, random, and truncate.
      error_message_ = absl::Substitute(
          "Ignoring extern function: $0", function_name.c_str());
      LOG(WARNING) << error_message_;
    }
  }
  return decode_ok;
}

// TODO(unknown): Is clone3's third argument relevant, or can it be ignored
// so that clone3/clone can be processed similarly?
bool MethodCallDecoder::DecodeClone3(const P4::ExternFunction& clone_extern) {
  method_op_.add_primitives(P4_ACTION_OP_CLONE);
  // TODO(unknown): Decode clone3 parameters.
  return true;
}

bool MethodCallDecoder::DecodeClone(const P4::ExternFunction& clone_extern) {
  method_op_.add_primitives(P4_ACTION_OP_CLONE);
  // TODO(unknown): Decode clone parameters.
  return true;
}

// The Stratum switch stack gets enough information from P4Info to support
// direct counters and meters, so no additional P4 pipeline config output is
// needed, and p4c can treat them as a NOP.
bool MethodCallDecoder::DecodeDirectCounter(
    const P4::ExternMethod& counter_extern) {
  const std::string& method_name = counter_extern.method->name.name.c_str();
  if (method_name != GetP4ModelNames().direct_counter_count_method_name())
    return false;
  method_op_.add_primitives(P4_ACTION_OP_NOP);
  return true;
}

// TODO(unknown): Non-direct meters and counters still need work.
bool MethodCallDecoder::DecodeCounter(const P4::ExternMethod& counter_extern) {
  return false;
}

bool MethodCallDecoder::DecodeDirectMeter(
    const P4::ExternMethod& meter_extern) {
  const std::string& method_name = meter_extern.method->name.name.c_str();
  if (method_name != GetP4ModelNames().direct_meter_read_method_name())
    return false;
  method_op_.add_primitives(P4_ACTION_OP_NOP);
  return true;
}

bool MethodCallDecoder::DecodeMeter(const P4::ExternMethod& meter_extern) {
  return false;
}

bool MethodCallDecoder::DecodeBuiltIn(const P4::BuiltInMethod& built_in) {
  if (built_in.name == IR::Type_Header::setValid) {
    tunnel_op_.set_header_op(P4_HEADER_SET_VALID);
  } else if (built_in.name == IR::Type_Header::setInvalid) {
    tunnel_op_.set_header_op(P4_HEADER_SET_INVALID);
  } else {
    // TODO(unknown): More built-in method call implementation is needed below.
    error_message_ = absl::Substitute("Ignoring built-in method $0",
                                      built_in.name.name.c_str());
    return false;
  }

  FieldNameInspector header_inspector;
  header_inspector.ExtractName(*built_in.appliedTo);
  tunnel_op_.set_header_name(header_inspector.field_name());
  VLOG(1) << "Tunnel encap/decap " << tunnel_op_.ShortDebugString();

  return true;
}

}  // namespace p4c_backends
}  // namespace stratum
