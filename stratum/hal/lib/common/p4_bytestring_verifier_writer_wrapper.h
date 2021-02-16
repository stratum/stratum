// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_P4_BYTESTRING_VERIFIER_WRITER_WRAPPER_H_
#define STRATUM_HAL_LIB_COMMON_P4_BYTESTRING_VERIFIER_WRITER_WRAPPER_H_

#include <memory>
#include <utility>

#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/p4/utils.h"

namespace stratum {
namespace hal {

namespace {
bool IsCanonicalP4runtimeByteString(const std::string str) {
  return ByteStringToP4RuntimeByteString(str) == str;
}

::util::Status VerifyTableEntry(const ::p4::v1::TableEntry& entry) {
  for (const auto& match : entry.match()) {
    switch (match.field_match_type_case()) {
      case ::p4::v1::FieldMatch::kExact:
        if (!IsCanonicalP4runtimeByteString(match.exact().value())) {
          RETURN_ERROR(ERR_INVALID_PARAM)
              << "Match field bytestring of " << match.ShortDebugString()
              << " is malformed.";
        }
        break;
      case ::p4::v1::FieldMatch::kTernary:
        if (!IsCanonicalP4runtimeByteString(match.ternary().value()) ||
            !IsCanonicalP4runtimeByteString(match.ternary().mask())) {
          RETURN_ERROR(ERR_INVALID_PARAM)
              << "Match field bytestring of " << match.ShortDebugString()
              << " is malformed.";
        }
        break;
      case ::p4::v1::FieldMatch::kLpm:
        if (!IsCanonicalP4runtimeByteString(match.lpm().value())) {
          RETURN_ERROR(ERR_INVALID_PARAM)
              << "Match field bytestring of " << match.ShortDebugString()
              << " is malformed.";
        }
        break;
      case ::p4::v1::FieldMatch::kRange:
        if (!IsCanonicalP4runtimeByteString(match.range().low()) ||
            !IsCanonicalP4runtimeByteString(match.range().high())) {
          RETURN_ERROR(ERR_INVALID_PARAM)
              << "Match field bytestring of " << match.ShortDebugString()
              << " is malformed.";
        }
        break;
      default:
        continue;
    }
  }

  for (const auto& action_param : entry.action().action().params()) {
    if (!IsCanonicalP4runtimeByteString(action_param.value())) {
      RETURN_ERROR(ERR_INVALID_PARAM)
          << "Action param " << action_param.ShortDebugString()
          << " is malformed.";
    }
  }

  return ::util::OkStatus();
}
}  // namespace

inline ::util::Status IsInCanonicalByteStringFormat(
    const ::p4::v1::WriteRequest& req) {
  return ::util::OkStatus();
}

// Wrapper
class P4BytestringVerifierWrapper
    : public WriterInterface<::p4::v1::ReadResponse> {
 public:
  explicit P4BytestringVerifierWrapper(
      WriterInterface<::p4::v1::ReadResponse>* writer)
      : writer_(writer) {}
  bool Write(const ::p4::v1::ReadResponse& resp) override {
    if (!writer_) return false;

    for (const auto& entity : resp.entities()) {
      switch (entity.entity_case()) {
        case ::p4::v1::Entity::kTableEntry: {
          ::util::Status status = VerifyTableEntry(entity.table_entry());
          if (!status.ok()) {
            return false;
          }
          break;
        }
        default:
          continue;
      }
    }

    return writer_->Write(resp);
  }

 private:
  WriterInterface<::p4::v1::ReadResponse>* writer_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_P4_BYTESTRING_VERIFIER_WRITER_WRAPPER_H_
