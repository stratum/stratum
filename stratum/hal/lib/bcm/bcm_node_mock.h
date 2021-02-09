// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BCM_BCM_NODE_MOCK_H_
#define STRATUM_HAL_LIB_BCM_BCM_NODE_MOCK_H_

#include <vector>

#include "gmock/gmock.h"
#include "stratum/hal/lib/bcm/bcm_node.h"

namespace stratum {
namespace hal {
namespace bcm {

class BcmNodeMock : public BcmNode {
 public:
  MOCK_METHOD2(PushChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD2(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD1(
      PushForwardingPipelineConfig,
      ::util::Status(const ::p4::v1::ForwardingPipelineConfig& config));
  MOCK_METHOD1(
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
  MOCK_METHOD1(
      RegisterStreamMessageResponseWriter,
      ::util::Status(
          std::function<void(const ::p4::v1::StreamMessageResponse& resp)>
              callback));
  MOCK_METHOD1(HandleStreamMessageRequest,
               ::util::Status(const ::p4::v1::StreamMessageRequest& req));
  MOCK_METHOD1(UpdatePortState, ::util::Status(uint32 port_id));
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_NODE_MOCK_H_
