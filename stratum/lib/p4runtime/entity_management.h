// Copyright 2020 Google LLC
// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_TOOLS_BENCHMARK_ENTITY_MANAGEMENT_H_
#define STRATUM_TOOLS_BENCHMARK_ENTITY_MANAGEMENT_H_

#include <string>

#include "p4/config/v1/p4info.pb.h"
// #include "p4/v1/p4runtime.pb.h"
#include "absl/container/flat_hash_map.h"
#include "stratum/glue/status/status.h"

namespace stratum {
namespace p4runtime {

// Build a replacement map from the given p4info object for later use with
// HydrateP4RuntimeProtoFromString. Match fields and action parameters are added
// under their fully qualified name (FQN) of their parent entity.
::util::Status BuildP4RTEntityIdReplacementMap(
    const ::p4::config::v1::P4Info& p4_info,
    absl::flat_hash_map<std::string, std::string>* replacements);

// Replace the P4RT entity names with their numeric IDs in the given
// pseudo-protobuf string and parse as P4RT proto object.
::util::Status HydrateP4RuntimeProtoFromString(
    const absl::flat_hash_map<std::string, std::string>& replacements,
    std::string proto_string, ::google::protobuf::Message* message);

// One-shot version of HydrateP4RuntimeProtoFromString that also builds the
// mapping. Consider using BuildP4RTEntityIdReplacementMap if you want to
// hydrate more than one entry or require faster processing.
::util::Status HydrateP4RuntimeProtoFromString(
    const ::p4::config::v1::P4Info& p4_info, std::string proto_string,
    ::google::protobuf::Message* message);

// For testing only.
template <typename T>
T HydrateP4RuntimeProtoFromStringOrDie(
    const absl::flat_hash_map<std::string, std::string>& replacements,
    std::string proto_string) {
  T message;
  CHECK_OK(
      HydrateP4RuntimeProtoFromString(replacements, proto_string, &message));
  return message;
}

// For testing only.
template <typename T>
T HydrateP4RuntimeProtoFromStringOrDie(const ::p4::config::v1::P4Info& p4_info,
                                       std::string proto_string) {
  absl::flat_hash_map<std::string, std::string> replacements;
  CHECK_OK(BuildP4RTEntityIdReplacementMap(p4_info, &replacements));
  T message;
  CHECK_OK(
      HydrateP4RuntimeProtoFromString(replacements, proto_string, &message));
  return message;
}

::util::Status DehydrateP4RuntimeProtoString(
    const ::p4::config::v1::P4Info& p4_info, std::string* proto_string);

}  // namespace p4runtime
}  // namespace stratum

#endif  // STRATUM_TOOLS_BENCHMARK_ENTITY_MANAGEMENT_H_
