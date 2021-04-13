// Copyright 2020 Google LLC
// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/p4runtime/entity_management.h"

#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace tools {
namespace benchmark {

using ::p4::config::v1::P4Info;

::util::Status BuildP4RTEntityIdReplacementMap(
    const P4Info& p4_info,
    absl::flat_hash_map<std::string, std::string>* replacements) {
  CHECK_RETURN_IF_FALSE(replacements);

  for (const auto& table : p4_info.tables()) {
    CHECK_RETURN_IF_FALSE(gtl::InsertIfNotPresent(
        replacements, absl::StrFormat("{%s}", table.preamble().name()),
        absl::StrFormat("%u", table.preamble().id())));

    // Add match fields as FQ name to lookup table.
    for (const auto& match : table.match_fields()) {
      std::string fqn =
          absl::StrFormat("{%s.%s}", table.preamble().name(), match.name());
      CHECK_RETURN_IF_FALSE(gtl::InsertIfNotPresent(
          replacements, fqn, absl::StrFormat("%u", match.id())));
    }
  }

  for (const auto& reg : p4_info.registers()) {
    CHECK_RETURN_IF_FALSE(gtl::InsertIfNotPresent(
        replacements, absl::StrFormat("{%s}", reg.preamble().name()),
        absl::StrFormat("%u", reg.preamble().id())));
  }

  for (const auto& action : p4_info.actions()) {
    CHECK_RETURN_IF_FALSE(gtl::InsertIfNotPresent(
        replacements, absl::StrFormat("{%s}", action.preamble().name()),
        absl::StrFormat("%u", action.preamble().id())));

    // Add action parameters as FQ name to lookup table.
    for (const auto& param : action.params()) {
      std::string fqn =
          absl::StrFormat("{%s.%s}", action.preamble().name(), param.name());
      CHECK_RETURN_IF_FALSE(gtl::InsertIfNotPresent(
          replacements, fqn, absl::StrFormat("%u", param.id())));
    }
  }

  return ::util::OkStatus();
}

::util::Status HydrateP4RuntimeProtoFromString(
    const absl::flat_hash_map<std::string, std::string>& replacements,
    std::string proto_string, ::google::protobuf::Message* message) {
  absl::StrReplaceAll(replacements, &proto_string);
  RETURN_IF_ERROR(ParseProtoFromString(proto_string, message));

  return ::util::OkStatus();
}

::util::Status HydrateP4RuntimeProtoFromString(
    const P4Info& p4_info, std::string proto_string,
    ::google::protobuf::Message* message) {
  absl::flat_hash_map<std::string, std::string> replacements;
  RETURN_IF_ERROR(BuildP4RTEntityIdReplacementMap(p4_info, &replacements));
  return HydrateP4RuntimeProtoFromString(replacements, proto_string, message);
}

}  // namespace benchmark
}  // namespace tools
}  // namespace stratum
