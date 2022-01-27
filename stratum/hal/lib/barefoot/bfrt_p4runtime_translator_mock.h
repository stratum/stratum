// Copyright 2022-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_P4RUNTIME_TRANSLATOR_MOCK_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_P4RUNTIME_TRANSLATOR_MOCK_H_

#include <string>

#include "gmock/gmock.h"
#include "stratum/hal/lib/barefoot/bfrt_p4runtime_translator.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtP4RuntimeTranslatorMock : public BfrtP4RuntimeTranslator {
 public:
  MOCK_METHOD2(PushChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD1(PushForwardingPipelineConfig,
               ::util::Status(const ::p4::config::v1::P4Info& p4info));
  MOCK_METHOD2(TranslateEntity,
               ::util::StatusOr<::p4::v1::Entity>(
                   const ::p4::v1::Entity& entity, bool to_sdk));
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
  MOCK_METHOD1(TranslateP4Info, ::util::StatusOr<::p4::config::v1::P4Info>(
      const ::p4::config::v1::P4Info& p4info));
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_P4RUNTIME_TRANSLATOR_MOCK_H_
