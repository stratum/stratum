/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STRATUM_HAL_LIB_COMMON_SUBSCRIBE_READER_WRITER_MOCK_H_
#define STRATUM_HAL_LIB_COMMON_SUBSCRIBE_READER_WRITER_MOCK_H_

#include "gnmi/gnmi.grpc.pb.h"
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

#endif  // STRATUM_HAL_LIB_COMMON_SUBSCRIBE_READER_WRITER_MOCK_H_
