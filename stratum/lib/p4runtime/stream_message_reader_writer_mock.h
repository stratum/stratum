// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_SUBSCRIBE_READER_WRITER_MOCK_H_
#define STRATUM_HAL_LIB_COMMON_SUBSCRIBE_READER_WRITER_MOCK_H_

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "p4/v1/p4runtime.pb.h"

// A mock class for ServerReaderWriter stream used for P4Runtime subscriptions.
// Used to test if the GnmiPublisher correctly transmits data to the controller.
// All methods have to be mocked as they are defined as abtract methods in the
// template.
class StreamMessageReaderWriterMock
    : public ::grpc::ServerReaderWriterInterface<
          ::p4::v1::StreamMessageResponse, ::p4::v1::StreamMessageRequest> {
 public:
  StreamMessageReaderWriterMock() {
    EXPECT_CALL(*this, Write(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(true));
    EXPECT_CALL(*this, Read(::testing::_))
        .WillRepeatedly(::testing::Return(true));
  }

  MOCK_METHOD0(SendInitialMetadata, void());
  MOCK_METHOD2(Write, bool(const ::p4::v1::StreamMessageResponse&,
                           ::grpc::WriteOptions));
  MOCK_METHOD1(NextMessageSize, bool(uint32_t*));
  MOCK_METHOD1(Read, bool(::p4::v1::StreamMessageRequest*));
};

#endif  // STRATUM_HAL_LIB_COMMON_SUBSCRIBE_READER_WRITER_MOCK_H_
