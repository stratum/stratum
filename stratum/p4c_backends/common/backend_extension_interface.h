// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file declares an interface that runs a platform-specific backend pass
// as an extension of the third-party p4c compiler.

#ifndef STRATUM_P4C_BACKENDS_COMMON_BACKEND_EXTENSION_INTERFACE_H_
#define STRATUM_P4C_BACKENDS_COMMON_BACKEND_EXTENSION_INTERFACE_H_

#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"

// These forward references are necessary because third-party p4c likes
// to give some of their types the same names as objects in the std namespace.
// If all the p4c dependencies are included here, it can cause compiler
// ambiguities throughout the std namespace where these conflicts exist.
namespace IR {
class ToplevelBlock;
}  // namespace IR

namespace P4 {
class ReferenceMap;
class TypeMap;
}  // namespace P4

namespace stratum {
namespace p4c_backends {

class BackendExtensionInterface {
 public:
  virtual ~BackendExtensionInterface() {}

  // A backend extension has a single API to run its phase of the compilation.
  // Parameters are:
  //  top_level - refers to the top-level block in the compiler's internal
  //      representation.
  //  static_table_entries - is a ::p4::WriteRequest that contains updates for
  //      all "const entries" properties in the P4 program's tables.
  //  p4_info - the P4Info generated by the compiler between the frontend and
  //      midend passes.  The p4_info is const; backends should not alter
  //      the overall runtime API that the P4Info specifies.
  //  ref_map - points to the p4c ReferenceMap generated by the midend.
  //  type_map - points to the p4c TypeMap generated by the midend
  virtual void Compile(const IR::ToplevelBlock& top_level,
                       const ::p4::v1::WriteRequest& static_table_entries,
                       const ::p4::config::v1::P4Info& p4_info,
                       P4::ReferenceMap* ref_map, P4::TypeMap* type_map) = 0;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_COMMON_BACKEND_EXTENSION_INTERFACE_H_
