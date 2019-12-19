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

#include <grpcpp/grpcpp.h>
#include <memory>
#include <vector>
#include <utility>

#include "gflags/gflags.h"
#include "google/rpc/code.pb.h"
#include "stratum/glue/net_util/ports.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/hal/lib/common/phal_mock.h"
#include "stratum/hal/lib/phal/attribute_database_mock.h"
#include "stratum/lib/security/auth_policy_checker_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/public/lib/error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/integral_types.h"
#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"

namespace stratum {
namespace hal {
namespace phal {

using ::testing::_;
using ::testing::Eq;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::ByMove;
using ::testing::SetArgPointee;
using ::testing::WithArgs;

typedef ::grpc::ClientReader<SubscribeResponse>
    ClientStreamChannelReader;

class PhalDbServiceTest : public ::testing::TestWithParam<OperationMode> {
 protected:
  void SetUp() override {
    mode_ = GetParam();
    phal_mock_ = absl::make_unique<PhalMock>();
    auth_policy_checker_mock_ = absl::make_unique<AuthPolicyCheckerMock>();
    error_buffer_ = absl::make_unique<ErrorBuffer>();
    database_mock_ =
        absl::make_unique<AttributeDatabaseMock>();
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
  std::unique_ptr<AuthPolicyCheckerMock> auth_policy_checker_mock_;
  std::unique_ptr<ErrorBuffer> error_buffer_;
  std::unique_ptr<PhalDbService> phaldb_service_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<PhalDb::Stub> stub_;
  std::unique_ptr<PhalMock> phal_mock_;
  std::unique_ptr<AttributeDatabaseMock> database_mock_;
};

TEST_P(PhalDbServiceTest, SetupWarm) {
  EXPECT_OK(phaldb_service_->Setup(true));
}

TEST_P(PhalDbServiceTest, GetRequestStrSuccess) {
  ::grpc::ClientContext context;
  GetRequest req;
  GetResponse resp;

  // Returned PhalDB
  auto phaldb_resp = absl::make_unique<PhalDB>();
  auto card = phaldb_resp->add_cards();
  auto port = card->add_ports();
  port->set_physical_port_type(PHYSICAL_PORT_TYPE_SFP_CAGE);
  auto sfp = port->mutable_transceiver();
  sfp->set_id(0);
  sfp->set_description("port-0");
  sfp->set_hardware_state(HW_STATE_PRESENT);
  sfp->set_media_type(MEDIA_TYPE_SFP);
  sfp->set_connector_type(SFP_TYPE_SFP);
  sfp->set_module_type(SFP_MODULE_TYPE_10G_BASE_CR);
  sfp->mutable_info()->set_mfg_name("test vendor");
  sfp->mutable_info()->set_part_no("test part #");
  sfp->mutable_info()->set_serial_no("test1234");

  // Create mock query
  auto db_query_mock = absl::make_unique<QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  // Add Path
  std::vector<Path> paths = {{
    PathEntry("cards", 0),
    PathEntry("ports", 0),
    PathEntry("transceiver", -1, false, false, true)
  }};

  // Setup Mock DB calls

  auto database = database_mock_.get();


  EXPECT_CALL(*database, MakeQuery(_))
      // .WillOnce(Return(ByMove(std::move(db_query_mock))));
      .WillOnce(Return(std::move(absl::make_unique<QueryMock>())));

  EXPECT_CALL(*db_query, Get())
    .WillOnce(Return(ByMove(std::move(phaldb_resp))));

  ASSERT_OK(ParseProtoFromString("cards[0]/ports[0]/transceiver/", req.mutable_path()));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDbService", "Get", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  // Call and validate results.
  ::grpc::Status status = stub_->Get(&context, req, &resp);
  EXPECT_TRUE(status.ok());
}

TEST_P(PhalDbServiceTest, GetRequestPathSuccess) {
  ::grpc::ClientContext context;
  GetRequest req;
  GetResponse resp;

  // Returned PhalDB
  auto phaldb_resp = absl::make_unique<PhalDB>();
  auto card = phaldb_resp->add_cards();
  auto port = card->add_ports();
  port->set_physical_port_type(PHYSICAL_PORT_TYPE_SFP_CAGE);
  auto sfp = port->mutable_transceiver();
  sfp->set_id(0);
  sfp->set_description("port-0");
  sfp->set_hardware_state(HW_STATE_PRESENT);
  sfp->set_media_type(MEDIA_TYPE_SFP);
  sfp->set_connector_type(SFP_TYPE_SFP);
  sfp->set_module_type(SFP_MODULE_TYPE_10G_BASE_CR);
  sfp->mutable_info()->set_mfg_name("test vendor");
  sfp->mutable_info()->set_part_no("test part #");
  sfp->mutable_info()->set_serial_no("test1234");

  // Create mock query
  auto db_query_mock = absl::make_unique<QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  // Add Path
  std::vector<Path> paths = {{
    PathEntry("cards", 0),
    PathEntry("ports", 0),
    PathEntry("transceiver", -1, false, false, true)
  }};

  // Setup Mock DB calls

  auto database = database_mock_.get();


  EXPECT_CALL(*database, MakeQuery(_))
      .WillOnce(Return(ByMove(std::move(db_query_mock))));

  EXPECT_CALL(*db_query, Get())
    .WillOnce(Return(ByMove(std::move(phaldb_resp))));

  auto entry = req.mutable_path()->add_entries();
  entry->set_name("cards");
  entry->set_index(0);
  entry->set_indexed(true);
  entry = req.mutable_path()->add_entries();
  entry->set_name("ports");
  entry->set_index(0);
  entry->set_indexed(true);
  entry = req.mutable_path()->add_entries();
  entry->set_name("transceiver");
  entry->set_terminal_group(true);

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDbService", "Get", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  // Call and validate results.
  EXPECT_TRUE(stub_->Get(&context, req, &resp).ok());
}

TEST_P(PhalDbServiceTest, SetRequestStrSuccess) {
  ::grpc::ClientContext context;
  SetRequest req;
  SetResponse resp;

  // Setup Mock DB calls

  auto database = database_mock_.get();


  // AttributeValueMap for set
  AttributeValueMap attrs;

  // Create a path
  std::vector<PathEntry> path = {
    PathEntry("fan_trays", 0),
    PathEntry("fans", 0),
    PathEntry("speed_control", -1, false, false, true)
  };
  attrs[path] = 20;

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<int32>(attrs[path]), Eq(20));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDbService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  ASSERT_OK(ParseProtoFromString("fan_trays[0]/fans[0]/speed_control", update->mutable_path()));
  update->mutable_value()->set_int32_val(20);

  // Invoke the RPC and validate the results.
  // Call and validate results.
  EXPECT_TRUE(stub_->Set(&context, req, &resp).ok());
}

TEST_P(PhalDbServiceTest, SetRequestInvalidStrFail) {
  ::grpc::ClientContext context;
  SetRequest req;
  SetResponse resp;

  // Setup Mock DB calls

  auto database = database_mock_.get();


  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDbService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  // Invalid request string
  ASSERT_OK(ParseProtoFromString("/fan_trays[0]/fans[0]/speed_control", update->mutable_path()));
  update->mutable_value()->set_int32_val(20);

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Set(&context, req, &resp);
  EXPECT_FALSE(status.ok());
}

TEST_P(PhalDbServiceTest, SetRequestPathSuccess) {
  ::grpc::ClientContext context;
  SetRequest req;
  SetResponse resp;

  // Setup Mock DB calls

  auto database = database_mock_.get();


  // AttributeValueMap for set
  AttributeValueMap attrs;

  // Create a path
  std::vector<PathEntry> path = {
    PathEntry("fan_trays", 0),
    PathEntry("fans", 0),
    PathEntry("rpm", -1, false, false, true)
  };
  attrs[path] = 1000.0;

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<double>(attrs[path]), Eq(1000.0));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDbService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();

  // Path entry
  auto entry = update->mutable_path()->add_entries();
  entry->set_name("cards");
  entry->set_index(0);
  entry->set_indexed(true);

  entry = update->mutable_path()->add_entries();
  entry->set_name("ports");
  entry->set_index(0);
  entry->set_indexed(true);

  entry = update->mutable_path()->add_entries();
  entry->set_name("transceiver");
  entry->set_terminal_group(true);

  // value
  update->mutable_value()->set_double_val(1000.0);

  // Invoke the RPC and validate the results.
  // Call and validate results.
  EXPECT_TRUE(stub_->Set(&context, req, &resp).ok());
}

TEST_P(PhalDbServiceTest, SetRequestStringSuccess) {
  ::grpc::ClientContext context;
  SetRequest req;
  SetResponse resp;

  // Setup Mock DB calls

  auto database = database_mock_.get();


  // AttributeValueMap for set
  AttributeValueMap attrs;

  // Create a path
  std::vector<PathEntry> path = {
    PathEntry("fan_trays", 0),
    PathEntry("fans", 0),
    PathEntry("model", -1, false, false, true)
  };
  attrs[path] = std::string("model1234");

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<std::string>(attrs[path]), Eq("model1234"));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDbService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  ASSERT_OK(ParseProtoFromString("fan_trays[0]/fans[0]/model", update->mutable_path()));
  update->mutable_value()->set_string_val("model1234");

  // Invoke the RPC and validate the results.
  // Call and validate results.
  EXPECT_TRUE(stub_->Set(&context, req, &resp).ok());
}

TEST_P(PhalDbServiceTest, SetRequestBoolSuccess) {
  ::grpc::ClientContext context;
  SetRequest req;
  SetResponse resp;

  // Setup Mock DB calls

  auto database = database_mock_.get();


  // AttributeValueMap for set
  AttributeValueMap attrs;

  // Create a path
  std::vector<PathEntry> path = {
    PathEntry("cards", 0),
    PathEntry("ports", 0),
    PathEntry("transceiver"),
    PathEntry("data_ready", -1, false, false, true)
  };
  attrs[path] = true;

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<bool>(attrs[path]), Eq(true));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDbService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  ASSERT_OK(ParseProtoFromString("fan_trays[0]/fans[0]/data_ready", update->mutable_path()));
  update->mutable_value()->set_bool_val(true);

  // Invoke the RPC and validate the results.
  // Call and validate results.
  EXPECT_TRUE(stub_->Set(&context, req, &resp).ok());
}

TEST_P(PhalDbServiceTest, SetRequestFloatSuccess) {
  ::grpc::ClientContext context;
  SetRequest req;
  SetResponse resp;

  // Setup Mock DB calls

  auto database = database_mock_.get();


  // AttributeValueMap for set
  AttributeValueMap attrs;

  // Create a path
  std::vector<PathEntry> path = {
    PathEntry("cards", 0),
    PathEntry("ports", 0),
    PathEntry("transceiver"),
    PathEntry("float", -1, false, false, true)
  };
  attrs[path] = 10.0f;

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<float>(attrs[path]), Eq(10.0f));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDbService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  ASSERT_OK(ParseProtoFromString("fan_trays[0]/fans[0]/float", update->mutable_path()));
  update->mutable_value()->set_float_val(10.0f);

  // Invoke the RPC and validate the results.
  // Call and validate results.
  EXPECT_TRUE(stub_->Set(&context, req, &resp).ok());
}

TEST_P(PhalDbServiceTest, SetRequestInt64Success) {
  ::grpc::ClientContext context;
  SetRequest req;
  SetResponse resp;

  // Setup Mock DB calls

  auto database = database_mock_.get();


  // AttributeValueMap for set
  AttributeValueMap attrs;

  // Create a path
  std::vector<PathEntry> path = {
    PathEntry("cards", 0),
    PathEntry("ports", 0),
    PathEntry("transceiver"),
    PathEntry("int64", -1, false, false, true)
  };
  attrs[path] = static_cast<int64>(10);

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<int64>(attrs[path]), Eq(static_cast<int64>(10)));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDbService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  ASSERT_OK(ParseProtoFromString("fan_trays[0]/fans[0]/int64", update->mutable_path()));
  update->mutable_value()->set_int64_val(static_cast<int64>(10));

  // Invoke the RPC and validate the results.
  // Call and validate results.
  EXPECT_TRUE(stub_->Set(&context, req, &resp).ok());
}

TEST_P(PhalDbServiceTest, SetRequestUInt32Success) {
  ::grpc::ClientContext context;
  SetRequest req;
  SetResponse resp;

  // Setup Mock DB calls

  auto database = database_mock_.get();


  // AttributeValueMap for set
  AttributeValueMap attrs;

  // Create a path
  std::vector<PathEntry> path = {
    PathEntry("cards", 0),
    PathEntry("ports", 0),
    PathEntry("transceiver"),
    PathEntry("uint32", -1, false, false, true)
  };
  attrs[path] = static_cast<uint32>(10);

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<uint32>(attrs[path]), Eq(static_cast<uint32>(10)));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDbService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  ASSERT_OK(ParseProtoFromString("fan_trays[0]/fans[0]/uint32", update->mutable_path()));
  update->mutable_value()->set_uint32_val(static_cast<uint32>(10));

  // Invoke the RPC and validate the results.
  // Call and validate results.
  EXPECT_TRUE(stub_->Set(&context, req, &resp).ok());
}

TEST_P(PhalDbServiceTest, SetRequestUInt64Success) {
  ::grpc::ClientContext context;
  SetRequest req;
  SetResponse resp;

  // Setup Mock DB calls

  auto database = database_mock_.get();


  // AttributeValueMap for set
  AttributeValueMap attrs;

  // Create a path
  std::vector<PathEntry> path = {
    PathEntry("cards", 0),
    PathEntry("ports", 0),
    PathEntry("transceiver"),
    PathEntry("uint64", -1, false, false, true)
  };
  attrs[path] = static_cast<uint64>(10);

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<uint64>(attrs[path]), Eq(static_cast<uint64>(10)));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDbService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  ASSERT_OK(ParseProtoFromString("fan_trays[0]/fans[0]/uint64", update->mutable_path()));
  update->mutable_value()->set_uint64_val(static_cast<uint64>(10));

  // Invoke the RPC and validate the results.
  // Call and validate results.
  EXPECT_TRUE(stub_->Set(&context, req, &resp).ok());
}

TEST_P(PhalDbServiceTest, SetRequestBytesSuccess) {
  ::grpc::ClientContext context;
  SetRequest req;
  SetResponse resp;

  // Setup Mock DB calls

  auto database = database_mock_.get();


  // AttributeValueMap for set
  AttributeValueMap attrs;

  // Create a path
  std::vector<PathEntry> path = {
    PathEntry("cards", 0),
    PathEntry("ports", 0),
    PathEntry("transceiver"),
    PathEntry("bytes", -1, false, false, true)
  };
  attrs[path] = std::string("bytes");

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<std::string>(attrs[path]), Eq("bytes"));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDbService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  ASSERT_OK(ParseProtoFromString("fan_trays[0]/fans[0]/bytes", update->mutable_path()));
  update->mutable_value()->set_bytes_val("bytes");

  // Invoke the RPC and validate the results.
  // Call and validate results.
  EXPECT_TRUE(stub_->Set(&context, req, &resp).ok());
}

TEST_P(PhalDbServiceTest, SubscribeRequestSuccess) {
  ::grpc::ClientContext context;
  SubscribeRequest req;
  SubscribeResponse resp;

  // Returned PhalDB
  PhalDB phaldb_resp;
  auto card = phaldb_resp.add_cards();
  auto port = card->add_ports();
  port->set_physical_port_type(PHYSICAL_PORT_TYPE_SFP_CAGE);
  auto sfp = port->mutable_transceiver();
  sfp->set_id(0);
  sfp->set_description("port-0");
  sfp->set_hardware_state(HW_STATE_PRESENT);
  sfp->set_media_type(MEDIA_TYPE_SFP);
  sfp->set_connector_type(SFP_TYPE_SFP);
  sfp->set_module_type(SFP_MODULE_TYPE_10G_BASE_CR);
  sfp->mutable_info()->set_mfg_name("test vendor");
  sfp->mutable_info()->set_part_no("test part #");
  sfp->mutable_info()->set_serial_no("test1234");


  auto database = database_mock_.get();

  // Create mock query
  auto db_query_mock = absl::make_unique<QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  std::vector<Path> paths = {{
    PathEntry("cards", 0),
    PathEntry("ports", 0),
    PathEntry("transceiver", -1, false, false, true)
  }};

  // Setup Mock DB calls
  EXPECT_CALL(*database, MakeQuery(_))
      .WillOnce(Return(ByMove(std::move(db_query_mock))));

  EXPECT_CALL(*auth_policy_checker_mock_,
      Authorize("PhalDbService", "Subscribe", _))
          .WillOnce(Return(::util::OkStatus()));

  EXPECT_CALL(*db_query, Subscribe(_, absl::Seconds(1)))
      .WillRepeatedly(Return(
        ::util::StatusOr<PhalDB*>(&phaldb_resp)));

  // Add Path
  auto entry = req.mutable_path()->add_entries();
  entry->set_name("cards");
  entry->set_index(0);
  entry->set_indexed(true);
  entry = req.mutable_path()->add_entries();
  entry->set_name("ports");
  entry->set_index(0);
  entry->set_indexed(true);
  entry = req.mutable_path()->add_entries();
  entry->set_name("transceiver");
  entry->set_terminal_group(true);

  req.set_polling_interval(1);

  // invoke the RPC
  std::unique_ptr<ClientStreamChannelReader>
    reader(stub_->Subscribe(&context, req));

  // Read PhalDB response from Mock above
  ASSERT_TRUE(reader->Read(&resp));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
        phaldb_resp, resp.phal_db()));

  // Finish off the reader so the server shuts down
  ::grpc::Status status = reader->Finish();
  EXPECT_EQ(status.error_message(), "Subscribe read returned zero bytes.");
}

TEST_P(PhalDbServiceTest, SubscribeRequestFail) {
  ::grpc::ClientContext context;
  SubscribeRequest req;
  SubscribeResponse resp;

  // Returned PhalDB
  PhalDB phaldb_resp;
  auto card = phaldb_resp.add_cards();
  auto port = card->add_ports();
  port->set_physical_port_type(PHYSICAL_PORT_TYPE_SFP_CAGE);
  auto sfp = port->mutable_transceiver();
  sfp->set_id(0);
  sfp->set_description("port-0");
  sfp->set_hardware_state(HW_STATE_PRESENT);
  sfp->set_media_type(MEDIA_TYPE_SFP);
  sfp->set_connector_type(SFP_TYPE_SFP);
  sfp->set_module_type(SFP_MODULE_TYPE_10G_BASE_CR);
  sfp->mutable_info()->set_mfg_name("test vendor");
  sfp->mutable_info()->set_part_no("test part #");
  sfp->mutable_info()->set_serial_no("test1234");


  auto database = database_mock_.get();

  // Create mock query
  auto db_query_mock = absl::make_unique<QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  std::vector<Path> paths = {{
    PathEntry("cards", 0),
    PathEntry("ports", 0),
    PathEntry("transceiver", -1, false, false, true)
  }};

  // Setup Mock DB calls
  EXPECT_CALL(*database, MakeQuery(_))
      .WillOnce(Return(ByMove(std::move(db_query_mock))));

  EXPECT_CALL(*auth_policy_checker_mock_,
      Authorize("PhalDbService", "Subscribe", _))
          .WillOnce(Return(::util::OkStatus()));

  ::util::Status err_status = MAKE_ERROR(ERR_CANCELLED) << "cancel it";
  EXPECT_CALL(*db_query, Subscribe(_, absl::Seconds(1)))
      .WillRepeatedly(Return(
        ::util::StatusOr<PhalDB*>(err_status)));

  // Add Path
  auto entry = req.mutable_path()->add_entries();
  entry->set_name("cards");
  entry->set_index(0);
  entry->set_indexed(true);
  entry = req.mutable_path()->add_entries();
  entry->set_name("ports");
  entry->set_index(0);
  entry->set_indexed(true);
  entry = req.mutable_path()->add_entries();
  entry->set_name("transceiver");
  entry->set_terminal_group(true);

  req.set_polling_interval(1);

  // invoke the RPC
  std::unique_ptr<ClientStreamChannelReader>
    reader(stub_->Subscribe(&context, req));

  // Read PhalDB response from Mock above
  EXPECT_FALSE(reader->Read(&resp));

  // Finish off the reader so the server shuts down
  ::grpc::Status status = reader->Finish();
  EXPECT_EQ(status.error_code(), ERR_CANCELLED);
}

TEST_P(PhalDbServiceTest, SubscribeRequestStringFail) {
  ::grpc::ClientContext context;
  SubscribeRequest req;
  SubscribeResponse resp;

  // Returned PhalDB
  PhalDB phaldb_resp;
  auto card = phaldb_resp.add_cards();
  auto port = card->add_ports();
  port->set_physical_port_type(PHYSICAL_PORT_TYPE_SFP_CAGE);
  auto sfp = port->mutable_transceiver();
  sfp->set_id(0);
  sfp->set_description("port-0");
  sfp->set_hardware_state(HW_STATE_PRESENT);
  sfp->set_media_type(MEDIA_TYPE_SFP);
  sfp->set_connector_type(SFP_TYPE_SFP);
  sfp->set_module_type(SFP_MODULE_TYPE_10G_BASE_CR);
  sfp->mutable_info()->set_mfg_name("test vendor");
  sfp->mutable_info()->set_part_no("test part #");
  sfp->mutable_info()->set_serial_no("test1234");


  auto database = database_mock_.get();

  // Create mock query
  auto db_query_mock = absl::make_unique<QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  EXPECT_CALL(*auth_policy_checker_mock_,
      Authorize("PhalDbService", "Subscribe", _))
          .WillOnce(Return(::util::OkStatus()));

  EXPECT_CALL(*db_query, Subscribe(_, absl::Seconds(1)))
      .WillRepeatedly(Return(
        ::util::StatusOr<PhalDB*>(&phaldb_resp)));

  // this query string should fail
  ASSERT_OK(ParseProtoFromString("cards[0]/ports[f]/transceiver/", req.mutable_path()));
  req.set_polling_interval(1);

  // invoke the RPC
  std::unique_ptr<ClientStreamChannelReader>
    reader(stub_->Subscribe(&context, req));

  // Read PhalDB response from Mock above
  EXPECT_FALSE(reader->Read(&resp));

  // Finish off the reader so the server shuts down
  ::grpc::Status status = reader->Finish();
  EXPECT_EQ(status.error_code(), ::util::error::INVALID_ARGUMENT);
}

INSTANTIATE_TEST_SUITE_P(PhalDbServiceTestWithMode, PhalDbServiceTest,
                        ::testing::Values(OPERATION_MODE_STANDALONE,
                                          OPERATION_MODE_COUPLED,
                                          OPERATION_MODE_SIM));


}  // namespace phal
}  // namespace hal
}  // namespace stratum
