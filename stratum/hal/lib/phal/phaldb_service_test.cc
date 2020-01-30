// Copyright 2019 Google LLC
// Copyright 2019 Dell EMC
// Copyright 2019-present Open Networking Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stratum/hal/lib/phal/phaldb_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "gflags/gflags.h"
#include "gmock/gmock.h"
#include "google/rpc/code.pb.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/net_util/ports.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/hal/lib/common/phal_mock.h"
#include "stratum/hal/lib/phal/attribute_database_mock.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/security/auth_policy_checker_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {
namespace phal {

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::WithArgs;

typedef ::grpc::ClientReader<SubscribeResponse> ClientStreamChannelReader;

class PhalDbServiceTest : public ::testing::TestWithParam<OperationMode> {
 protected:
  void SetUp() override {
    mode_ = GetParam();
    phal_mock_ = absl::make_unique<PhalMock>();
    error_buffer_ = absl::make_unique<ErrorBuffer>();
    database_mock_ = absl::make_unique<AttributeDatabaseMock>();
    phaldb_service_ = absl::make_unique<PhalDbService>(database_mock_.get());
    std::string url =
        "localhost:" + std::to_string(stratum::PickUnusedPortOrDie());
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(url, ::grpc::InsecureServerCredentials());
    builder.RegisterService(phaldb_service_.get());
    server_ = builder.BuildAndStart();
    ASSERT_NE(server_, nullptr);
    stub_ = PhalDb::NewStub(
        ::grpc::CreateChannel(url, ::grpc::InsecureChannelCredentials()));
    ASSERT_NE(stub_, nullptr);
  }

  void TearDown() override {
    phaldb_service_->Teardown();
    server_->Shutdown();
  }

  OperationMode mode_;
  std::unique_ptr<ErrorBuffer> error_buffer_;
  std::unique_ptr<PhalDbService> phaldb_service_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<PhalDb::Stub> stub_;
  std::unique_ptr<PhalMock> phal_mock_;
  std::unique_ptr<AttributeDatabaseMock> database_mock_;

  const std::string valid_request_path_proto = R"PROTO(
    entries {
      name: "cards"
      index: 0
      indexed: true
    }
    entries {
      name: "ports"
      index: 0
      indexed: true
    }
    entries {
      name: "transceiver"
      terminal_group: true
    }
  )PROTO";

  const std::string invalid_request_path_proto = R"PROTO(
    entries {
      name: "does_not_exists"
      index: 0
      indexed: true
    }
  )PROTO";

  const std::string phaldb_get_response_proto = R"PROTO(
    cards {
      ports {
        physical_port_type: PHYSICAL_PORT_TYPE_SFP_CAGE
        transceiver {
          id: 0
          description: "port-0"
          hardware_state: HW_STATE_PRESENT
          media_type: MEDIA_TYPE_SFP
          connector_type: SFP_TYPE_SFP
          module_type: SFP_MODULE_TYPE_10G_BASE_CR
          info {
            mfg_name: "test_vendor"
            part_no: "test part #"
            serial_no: "test1234"
          }
        }
      }
    }
  )PROTO";
};

TEST_P(PhalDbServiceTest, SetupWarm) {
  EXPECT_OK(phaldb_service_->Setup(true));
}

TEST_P(PhalDbServiceTest, GetRequestSuccess) {
  ::grpc::ClientContext context;
  GetRequest req;
  GetResponse resp;

  // Returned PhalDB
  auto phaldb_resp = absl::make_unique<PhalDB>();
  ASSERT_OK(ParseProtoFromString(phaldb_get_response_proto, phaldb_resp.get()));

  // Create mock query
  auto db_query_mock = absl::make_unique<QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  // Setup Mock DB calls
  // TODO(max): check correct pb PathQuery to phal::Path translation
  EXPECT_CALL(*database_mock_.get(), MakeQuery(_))
      .WillOnce(Return(ByMove(
          ::util::StatusOr<std::unique_ptr<Query>>(std::move(db_query_mock)))));

  EXPECT_CALL(*db_query, Get())
      .WillOnce(Return(ByMove(std::move(phaldb_resp))));

  ASSERT_OK(ParseProtoFromString(valid_request_path_proto, req.mutable_path()));

  EXPECT_TRUE(stub_->Get(&context, req, &resp).ok());
}

TEST_P(PhalDbServiceTest, GetRequestFail) {
  ::grpc::ClientContext context;
  GetRequest req;
  GetResponse resp;

  // Create mock query
  auto db_query_mock = absl::make_unique<QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  // Setup Mock DB calls
  // TODO(max): check correct pb PathQuery to phal::Path translation
  EXPECT_CALL(*database_mock_.get(), MakeQuery(_))
      .WillOnce(Return(ByMove(
          ::util::StatusOr<std::unique_ptr<Query>>(std::move(db_query_mock)))));

  EXPECT_CALL(*db_query, Get())
      .WillOnce(Return(
          ByMove(::util::Status(util::error::NOT_FOUND, "Invalid path"))));

  ASSERT_OK(
      ParseProtoFromString(invalid_request_path_proto, req.mutable_path()));
  EXPECT_FALSE(stub_->Get(&context, req, &resp).ok());
}

// TODO(unknown): Make test polymorph to test all types
TEST_P(PhalDbServiceTest, SetRequestSuccess) {
  ::grpc::ClientContext context;
  SetRequest req;
  SetResponse resp;

  // Setup Mock DB calls
  auto database = database_mock_.get();

  // AttributeValueMap for set
  AttributeValueMap attrs;

  // Create a path
  std::vector<PathEntry> path = {PathEntry("fan_trays", 0),
                                 PathEntry("fans", 0),
                                 PathEntry("rpm", -1, false, false, true)};
  attrs[path] = 1000.0;

  // TODO(max): check correct pb PathQuery to phal::Path translation
  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<double>(attrs[path]), Eq(1000.0));

  // Create Set request
  auto update = req.add_updates();
  ASSERT_OK(
      ParseProtoFromString(valid_request_path_proto, update->mutable_path()));
  update->mutable_value()->set_double_val(1000.0);

  // Invoke the RPC and validate the results.
  // Call and validate results.
  EXPECT_TRUE(stub_->Set(&context, req, &resp).ok());
}

TEST_P(PhalDbServiceTest, SetRequestEmptyPathFail) {
  ::grpc::ClientContext context;
  SetRequest req;
  SetResponse resp;

  // Setup Mock DB calls
  auto database = database_mock_.get();

  // Create Set request
  auto update = req.add_updates();
  ASSERT_OK(ParseProtoFromString("", update->mutable_path()));
  update->mutable_value()->set_int32_val(20);

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Set(&context, req, &resp);
  EXPECT_FALSE(status.ok());
}

TEST_P(PhalDbServiceTest, SubscribeRequestSuccess) {
  ::grpc::ClientContext context;
  SubscribeRequest req;
  SubscribeResponse resp;

  // Returned PhalDB
  PhalDB phaldb_resp;
  ASSERT_OK(ParseProtoFromString(phaldb_get_response_proto, &phaldb_resp));

  auto database = database_mock_.get();

  // Create mock query
  auto db_query_mock = absl::make_unique<QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  std::vector<Path> paths = {
      {PathEntry("cards", 0), PathEntry("ports", 0),
       PathEntry("transceiver", -1, false, false, true)}};

  auto poll_interval = absl::Milliseconds(500);

  // Setup Mock DB calls
  // TODO(max): check correct pb PathQuery to phal::Path translation
  EXPECT_CALL(*database, MakeQuery(_))
      .WillOnce(Return(ByMove(
          ::util::StatusOr<std::unique_ptr<Query>>(std::move(db_query_mock)))));

  EXPECT_CALL(*db_query, Subscribe(_ /*writer*/, poll_interval))
      .WillRepeatedly(Invoke([&](std::unique_ptr<ChannelWriter<PhalDB>> writer,
                                 absl::Duration polling_interval) {
        RETURN_IF_ERROR(writer->TryWrite(phaldb_resp));
        return ::util::OkStatus();
      }));

  // Prepare request
  ASSERT_OK(ParseProtoFromString(valid_request_path_proto, req.mutable_path()));
  req.set_polling_interval(absl::ToInt64Nanoseconds(poll_interval));

  // invoke the RPC
  auto reader = stub_->Subscribe(&context, req);

  // Read PhalDB response from Mock above
  ASSERT_TRUE(reader->Read(&resp));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      phaldb_resp, resp.phal_db()));

  // Finish off the reader so the server shuts down.
  // The only way to shut down an unary stream is to cancel the ClientContext.
  // See: https://stackoverflow.com/q/53468002
  context.TryCancel();
  ASSERT_FALSE(reader->Read(&resp));
  ::grpc::Status status = reader->Finish();
  EXPECT_EQ(status.error_code(), ERR_CANCELLED);
}

TEST_P(PhalDbServiceTest, SubscribeRequestFail) {
  ::grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::milliseconds(500));
  SubscribeRequest req;
  SubscribeResponse resp;

  // Returned PhalDB
  PhalDB phaldb_resp;
  ASSERT_OK(ParseProtoFromString(phaldb_get_response_proto, &phaldb_resp));

  auto database = database_mock_.get();

  // Create mock query
  auto db_query_mock = absl::make_unique<QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  std::vector<Path> paths = {
      {PathEntry("cards", 0), PathEntry("ports", 0),
       PathEntry("transceiver", -1, false, false, true)}};

  auto poll_interval = absl::Milliseconds(500);

  // Setup Mock DB calls
  // TODO(max): check correct pb PathQuery to phal::Path translation
  EXPECT_CALL(*database, MakeQuery(_))
      .WillOnce(Return(ByMove(
          ::util::StatusOr<std::unique_ptr<Query>>(std::move(db_query_mock)))));

  EXPECT_CALL(*db_query, Subscribe(_, poll_interval))
      .WillRepeatedly(
          Return(ByMove(::util::Status(util::error::CANCELLED, "some error"))));

  // Prepare request
  ASSERT_OK(ParseProtoFromString(valid_request_path_proto, req.mutable_path()));
  req.set_polling_interval(absl::ToInt64Nanoseconds(poll_interval));

  // invoke the RPC
  auto reader = stub_->Subscribe(&context, req);

  // Read PhalDB response from Mock above
  EXPECT_FALSE(reader->Read(&resp));

  // Finish off the reader so the server shuts down
  ::grpc::Status status = reader->Finish();
  EXPECT_EQ(status.error_code(), ERR_CANCELLED);
}

// TODO(max): Check if we actually care about mode
INSTANTIATE_TEST_SUITE_P(PhalDbServiceTestWithMode, PhalDbServiceTest,
                         ::testing::Values(OPERATION_MODE_STANDALONE,
                                           OPERATION_MODE_COUPLED,
                                           OPERATION_MODE_SIM));

}  // namespace phal
}  // namespace hal
}  // namespace stratum
