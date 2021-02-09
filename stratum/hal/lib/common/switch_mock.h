// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_SWITCH_MOCK_H_
#define STRATUM_HAL_LIB_COMMON_SWITCH_MOCK_H_

#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "stratum/hal/lib/common/switch_interface.h"

namespace stratum {
namespace hal {

// A mock class for SwitchInterface.
class SwitchMock : public SwitchInterface {
 public:
  MOCK_METHOD1(PushChassisConfig, ::util::Status(const ChassisConfig& config));
  MOCK_METHOD1(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config));
  MOCK_METHOD2(
      PushForwardingPipelineConfig,
      ::util::Status(uint64 node_id,
                     const ::p4::v1::ForwardingPipelineConfig& config));
  MOCK_METHOD2(
      SaveForwardingPipelineConfig,
      ::util::Status(uint64 node_id,
                     const ::p4::v1::ForwardingPipelineConfig& config));
  MOCK_METHOD1(CommitForwardingPipelineConfig, ::util::Status(uint64 node_id));
  MOCK_METHOD2(
      VerifyForwardingPipelineConfig,
      ::util::Status(uint64 node_id,
                     const ::p4::v1::ForwardingPipelineConfig& config));
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
  MOCK_METHOD2(
      RegisterStreamMessageResponseWriter,
      ::util::Status(
          uint64 node_id,
          std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>>
              writer));
  MOCK_METHOD1(UnregisterStreamMessageResponseWriter,
               ::util::Status(uint64 node_id));
  MOCK_METHOD2(HandleStreamMessageRequest,
               ::util::Status(uint64 node_id,
                              const ::p4::v1::StreamMessageRequest& request));
  MOCK_METHOD1(
      RegisterEventNotifyWriter,
      ::util::Status(std::shared_ptr<WriterInterface<GnmiEventPtr>> writer));
  MOCK_METHOD0(UnregisterEventNotifyWriter, ::util::Status());
  MOCK_METHOD4(RetrieveValue,
               ::util::Status(uint64 node_id, const DataRequest& request,
                              WriterInterface<DataResponse>* writer,
                              std::vector<::util::Status>* details));
  MOCK_METHOD0(VerifyState, ::util::StatusOr<std::vector<std::string>>());
  MOCK_METHOD3(SetValue,
               ::util::Status(uint64 node_id, const SetRequest& request,
                              std::vector<::util::Status>* details));
  SwitchMock() {
    ::testing::DefaultValue<::util::Status>::Set(::util::OkStatus());
  }
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_SWITCH_MOCK_H_
