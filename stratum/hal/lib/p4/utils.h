// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// This file declares some utility functions for P4 objects.

#ifndef STRATUM_HAL_LIB_P4_UTILS_H_
#define STRATUM_HAL_LIB_P4_UTILS_H_

#include <string>

#include "grpcpp/grpcpp.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"

namespace stratum {
namespace hal {

// Decodes a P4 object ID into a human-readable form.
std::string PrintP4ObjectID(int object_id);

// Attempts to find a P4TableMapValue in p4_pipeline_config with the given
// table_map_key.  If an entry for the key is present, the entry's oneof
// descriptor is compared with descriptor_case.  The return status is a
// P4TableMapValue pointer if an entry with table_map_key exists and the
// entry matches the descriptor_case.  Otherwise, the return status is non-OK.
// The log_p4_object is a string that GetTableMapValueWithDescriptorCase
// optionally inserts into the error status message when non-empty.  For
// example, if the caller is looking for a match field's field descriptor,
// then the caller can provide the table name associated with the match
// field in log_p4_object.
::util::StatusOr<const P4TableMapValue*> GetTableMapValueWithDescriptorCase(
    const P4PipelineConfig& p4_pipeline_config,
    const std::string& table_map_key,
    P4TableMapValue::DescriptorCase descriptor_case,
    const std::string& log_p4_object);

// These two functions take an unsigned 64/32 bit integer and encode it as a
// byte stream in network order. Leading zeros are stripped off.
// TODO(max): move to stratum/lib/utils.h where ByteStreamToUint() is?
std::string Uint64ToByteStream(uint64 val);
std::string Uint32ToByteStream(uint32 val);

// Pads a P4Runtime byte string with zeros up to the given width. Surplus bytes
// will be truncated at the front. The returned string will always be exactly as
// long as requested.
std::string P4RuntimeByteStringToPaddedByteString(std::string byte_string,
                                                  size_t num_bytes);

// Converts a byte string to a canonical P4Runtime byte string.
std::string ByteStringToP4RuntimeByteString(std::string bytes);

// Validates that the P4 MeterConfig is a valid trTCM according to RFC 2698.
// See: https://datatracker.ietf.org/doc/html/rfc2698
::util::Status IsValidMeterConfig(const ::p4::v1::MeterConfig& meter_config);

// Helper to convert a gRPC status with error details to a string. Assumes
// ::grpc::Status includes a binary error detail which is encoding a serialized
// version of ::google::rpc::Status proto in which the details are captured
// using proto any messages.
std::string P4RuntimeGrpcStatusToString(const ::grpc::Status& status);

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_UTILS_H_
