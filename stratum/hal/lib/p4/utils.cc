// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// Implementation of p4_utils functions.

#include "stratum/hal/lib/p4/utils.h"

#include <arpa/inet.h>

#include <algorithm>

#include "absl/strings/str_format.h"
#include "absl/strings/substitute.h"
#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {

// Decodes a P4 object ID into a human-readable form.  The high-order byte of
// the 32-bit ID is a resource type, as specified by the P4Ids::Prefix enum.
std::string PrintP4ObjectID(int object_id) {
  int base_id = object_id & 0xffffff;
  ::p4::config::v1::P4Ids::Prefix resource_type =
      static_cast<::p4::config::v1::P4Ids::Prefix>((object_id >> 24) & 0xff);
  std::string resource_name =
      ::p4::config::v1::P4Ids::Prefix_Name(resource_type);
  if (resource_name.empty()) resource_name = "INVALID";
  return absl::StrFormat("%s/0x%x (0x%x)", resource_name.c_str(), base_id,
                         object_id);
}

// This unnamed namespace hides a function that forms a status string to refer
// to a P4 object.
namespace {

std::string AddP4ObjectReferenceString(const std::string& log_p4_object) {
  std::string referenced_object;
  if (!log_p4_object.empty()) {
    referenced_object =
        absl::Substitute(" referenced by P4 object $0", log_p4_object.c_str());
  }
  return referenced_object;
}

}  // namespace

::util::StatusOr<const P4TableMapValue*> GetTableMapValueWithDescriptorCase(
    const P4PipelineConfig& p4_pipeline_config,
    const std::string& table_map_key,
    P4TableMapValue::DescriptorCase descriptor_case,
    const std::string& log_p4_object) {
  auto* map_value =
      gtl::FindOrNull(p4_pipeline_config.table_map(), table_map_key);
  if (map_value != nullptr) {
    if (map_value->descriptor_case() != descriptor_case) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "P4PipelineConfig descriptor for " << table_map_key
             << AddP4ObjectReferenceString(log_p4_object)
             << " does not have the expected descriptor case: "
             << map_value->ShortDebugString();
    }
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "P4PipelineConfig table map has no descriptor for "
           << table_map_key << AddP4ObjectReferenceString(log_p4_object);
  }

  return map_value;
}

std::string Uint64ToByteStream(uint64 val) {
  uint64 tmp = (htonl(1) == (1))
                   ? val
                   : (static_cast<uint64>(htonl(val)) << 32) | htonl(val >> 32);
  std::string bytes = "";
  bytes.assign(reinterpret_cast<char*>(&tmp), sizeof(uint64));
  // Strip leading zeroes.
  while (bytes.size() > 1 && bytes[0] == '\x00') {
    bytes = bytes.substr(1);
  }
  return bytes;
}

std::string Uint32ToByteStream(uint32 val) {
  uint32 tmp = htonl(val);
  std::string bytes = "";
  bytes.assign(reinterpret_cast<char*>(&tmp), sizeof(uint32));
  // Strip leading zeroes.
  while (bytes.size() > 1 && bytes[0] == '\x00') {
    bytes = bytes.substr(1);
  }
  return bytes;
}

std::string P4RuntimeByteStringToPaddedByteString(std::string byte_string,
                                                  size_t num_bytes) {
  if (byte_string.size() > num_bytes) {
    byte_string.erase(0, byte_string.size() - num_bytes);
  } else {
    byte_string.insert(0, num_bytes - byte_string.size(), '\x00');
  }
  DCHECK_EQ(num_bytes, byte_string.size());

  return byte_string;
}

std::string ByteStringToP4RuntimeByteString(std::string bytes) {
  // Remove leading zeros.
  bytes.erase(0, std::min(bytes.find_first_not_of('\x00'), bytes.size() - 1));
  return bytes;
}

::util::Status IsValidMeterConfig(const ::p4::v1::MeterConfig& meter_config) {
  if (meter_config.cir() > meter_config.pir()) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Meter configuration " << meter_config.ShortDebugString()
        << " is invalid: committed rate cannot be greater than peak rate.";
  }
  if (meter_config.cburst() > meter_config.pburst()) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Meter configuration " << meter_config.ShortDebugString()
        << " is invalid: committed burst size cannot be greater than peak burst"
        << " size.";
  }
  if (meter_config.cburst() == 0) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Meter configuration " << meter_config.ShortDebugString()
        << " is invalid: committed burst size cannot be zero.";
  }
  if (meter_config.pburst() == 0) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Meter configuration " << meter_config.ShortDebugString()
        << " is invalid: peak burst size cannot be zero.";
  }

  return ::util::OkStatus();
}

std::string P4RuntimeGrpcStatusToString(const ::grpc::Status& status) {
  std::stringstream ss;
  if (!status.error_details().empty()) {
    ss << "(overall error code: "
       << ::google::rpc::Code_Name(ToGoogleRpcCode(status.error_code()))
       << ", overall error message: "
       << (status.error_message().empty() ? "None" : status.error_message())
       << "). Error details: ";
    ::google::rpc::Status details;
    if (!details.ParseFromString(status.error_details())) {
      ss << "Failed to parse ::google::rpc::Status from GRPC status details.";
    } else {
      for (int i = 0; i < details.details_size(); ++i) {
        ::p4::v1::Error detail;
        if (details.details(i).UnpackTo(&detail)) {
          ss << "\n(error #" << i + 1 << ": error code: "
             << ::google::rpc::Code_Name(ToGoogleRpcCode(detail.code()))
             << ", error message: "
             << (detail.message().empty() ? "None" : detail.message()) << ") ";
        }
      }
    }
  } else {
    ss << "(error code: "
       << ::google::rpc::Code_Name(ToGoogleRpcCode(status.error_code()))
       << ", error message: "
       << (status.error_message().empty() ? "None" : status.error_message())
       << ").";
  }

  return ss.str();
}

}  // namespace hal
}  // namespace stratum
