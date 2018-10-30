#ifndef PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_SUBSCRIBE_READER_WRITER_MOCK_H_
#define PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_SUBSCRIBE_READER_WRITER_MOCK_H_

#include "stratum/glue/gnmi/gnmi.grpc.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// A mock class for ServerReaderWriter stream used for gNMI subscriptions. Used
// to test if the GnmiPublisher correctly transmits data to the controller.
// All methods have to be mocked as they are defined as abtract methods in the
// template.
class SubscribeReaderWriterMock
    : public ::grpc::ServerReaderWriterInterface<::gnmi::SubscribeResponse,
                                                 ::gnmi::SubscribeRequest> {
 public:
  SubscribeReaderWriterMock() {
    EXPECT_CALL(*this, Write(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(true));
    EXPECT_CALL(*this, Read(::testing::_))
        .WillRepeatedly(::testing::Return(true));
  }

  MOCK_METHOD0(SendInitialMetadata, void());
  MOCK_METHOD2(Write,
               bool(const ::gnmi::SubscribeResponse&, ::grpc::WriteOptions));
  MOCK_METHOD1(NextMessageSize, bool(uint32_t*));
  MOCK_METHOD1(Read, bool(::gnmi::SubscribeRequest*));
};

#endif  // PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_SUBSCRIBE_READER_WRITER_MOCK_H_
