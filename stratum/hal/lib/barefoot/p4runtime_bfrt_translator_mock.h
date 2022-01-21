// Copyright 2022-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_P4RUNTIME_BFRT_TRANSLATOR_MOCK_H_
#define STRATUM_HAL_LIB_BAREFOOT_P4RUNTIME_BFRT_TRANSLATOR_MOCK_H_

#include <string>

#include "gmock/gmock.h"
#include "stratum/hal/lib/barefoot/p4runtime_bfrt_translator.h"

namespace stratum {
namespace hal {
namespace barefoot {

class P4RuntimeBfrtTranslatorMock : public P4RuntimeBfrtTranslator {
 public:
  MOCK_METHOD2(PushChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD1(PushForwardingPipelineConfig,
               ::util::Status(const ::p4::config::v1::P4Info& p4info));
  MOCK_METHOD1(TranslateWriteRequest,
               ::util::StatusOr<::p4::v1::WriteRequest>(
                   const ::p4::v1::WriteRequest& request));
  MOCK_METHOD1(TranslateReadRequest, ::util::StatusOr<::p4::v1::ReadRequest>(
                                         const ::p4::v1::ReadRequest& request));
  MOCK_METHOD1(TranslateReadResponse,
               ::util::StatusOr<::p4::v1::ReadResponse>(
                   const ::p4::v1::ReadResponse& response));
  MOCK_METHOD1(TranslateStreamMessageRequest,
               ::util::StatusOr<::p4::v1::StreamMessageRequest>(
                   const ::p4::v1::StreamMessageRequest& request));
  MOCK_METHOD1(TranslateStreamMessageResponse,
               ::util::StatusOr<::p4::v1::StreamMessageResponse>(
                   const ::p4::v1::StreamMessageResponse& response));
  MOCK_METHOD0(GetLowLevelP4Info, ::util::StatusOr<::p4::config::v1::P4Info>());
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_P4RUNTIME_BFRT_TRANSLATOR_MOCK_H_
