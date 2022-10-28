// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_TDI_NODE_MOCK_H_
#define STRATUM_HAL_LIB_TDI_TDI_NODE_MOCK_H_

#include <map>
#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "stratum/hal/lib/tdi/tdi_node.h"

namespace stratum {
namespace hal {
namespace tdi {

class TdiNodeMock : public TdiNode {
 public:
  MOCK_METHOD2(PushChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD2(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD1(
      PushForwardingPipelineConfig,
      ::util::Status(const ::p4::v1::ForwardingPipelineConfig& config));
  MOCK_CONST_METHOD1(
      VerifyForwardingPipelineConfig,
      ::util::Status(const ::p4::v1::ForwardingPipelineConfig& config));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_METHOD0(Freeze, ::util::Status());
  MOCK_METHOD0(Unfreeze, ::util::Status());
  MOCK_METHOD2(WriteForwardingEntries,
               ::util::Status(const ::p4::v1::WriteRequest& req,
                              std::vector<::util::Status>* results));
  MOCK_METHOD3(ReadForwardingEntries,
               ::util::Status(const ::p4::v1::ReadRequest& req,
                              WriterInterface<::p4::v1::ReadResponse>* writer,
                              std::vector<::util::Status>* details));
  MOCK_METHOD1(RegisterStreamMessageResponseWriter,
               ::util::Status(const std::shared_ptr<WriterInterface<
                                  ::p4::v1::StreamMessageResponse>>& writer));
  MOCK_METHOD1(HandleStreamMessageRequest,
               ::util::Status(const ::p4::v1::StreamMessageRequest& req));
};

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_NODE_MOCK_H_
