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


#include "stratum/hal/lib/common/phaldb_service.h"

#include <grpcpp/grpcpp.h>
#include <memory>
#include <vector>
#include <utility>

#include "gflags/gflags.h"
#include "google/rpc/code.pb.h"
#include "stratum/glue/net_util/ports.h"
// Note: EXPECT_OK already defined in google protobuf status.h
#undef EXPECT_OK
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

typedef ::grpc::ClientReader<::stratum::hal::phal::SubscribeResponse>
    ClientStreamChannelReader;

class PhalDBServiceTest : public ::testing::TestWithParam<OperationMode> {
 protected:
  void SetUp() override {
    mode_ = GetParam();
    phal_mock_ = absl::make_unique<PhalMock>();
    auth_policy_checker_mock_ = absl::make_unique<AuthPolicyCheckerMock>();
    error_buffer_ = absl::make_unique<ErrorBuffer>();
    phaldb_service_ = absl::make_unique<PhalDBService>(mode_,
                                               phal_mock_.get(),
                                               auth_policy_checker_mock_.get(),
                                               error_buffer_.get());
    std::string url =
        "localhost:" + std::to_string(stratum::PickUnusedPortOrDie());
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(url, ::grpc::InsecureServerCredentials());
    builder.RegisterService(phaldb_service_.get());
    server_ = builder.BuildAndStart();
    ASSERT_NE(server_, nullptr);
    stub_ = ::stratum::hal::phal::PhalDBSvc::NewStub(
        ::grpc::CreateChannel(url, ::grpc::InsecureChannelCredentials()));
    ASSERT_NE(stub_, nullptr);

    database_mock_ =
        absl::make_unique<::stratum::hal::phal::AttributeDatabaseMock>();
  }

  void TearDown() override {
    phaldb_service_->Teardown();
    server_->Shutdown();
  }

  OperationMode mode_;
  std::unique_ptr<AuthPolicyCheckerMock> auth_policy_checker_mock_;
  std::unique_ptr<ErrorBuffer> error_buffer_;
  std::unique_ptr<PhalDBService> phaldb_service_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<::stratum::hal::phal::PhalDBSvc::Stub> stub_;
  std::unique_ptr<PhalMock> phal_mock_;
  std::unique_ptr<::stratum::hal::phal::AttributeDatabaseMock> database_mock_;
};

TEST_P(PhalDBServiceTest, SetupWarm) {
  auto status = phaldb_service_->Setup(true);
  EXPECT_TRUE(status.ok());
}

TEST_P(PhalDBServiceTest, GetRequestStrSuccess) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::GetRequest req;
  ::stratum::hal::phal::GetResponse resp;

  // Returned PhalDB
  auto phaldb_resp = absl::make_unique<::stratum::hal::phal::PhalDB>();
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
  auto db_query_mock = absl::make_unique<::stratum::hal::phal::QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  // Add Path
  std::vector<::stratum::hal::phal::Path> paths = {{
    ::stratum::hal::phal::PathEntry("cards", 0),
    ::stratum::hal::phal::PathEntry("ports", 0),
    ::stratum::hal::phal::PathEntry("transceiver", -1, false, false, true)
  }};

  // Setup Mock DB calls
  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB()).WillRepeatedly(Return(database));

  EXPECT_CALL(*database, DoMakeQuery(_))
      .WillOnce(Return(ByMove(std::move(db_query_mock))));

  EXPECT_CALL(*db_query, DoGet())
    .WillOnce(Return(ByMove(std::move(phaldb_resp))));

  req.set_str("cards[0]/ports[0]/transceiver/");

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDBService", "Get", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Get(&context, req, &resp);
  ASSERT_TRUE(status.ok());
}

TEST_P(PhalDBServiceTest, GetRequestPathSuccess) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::GetRequest req;
  ::stratum::hal::phal::GetResponse resp;

  // Returned PhalDB
  auto phaldb_resp = absl::make_unique<::stratum::hal::phal::PhalDB>();
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
  auto db_query_mock = absl::make_unique<::stratum::hal::phal::QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  // Add Path
  std::vector<::stratum::hal::phal::Path> paths = {{
    ::stratum::hal::phal::PathEntry("cards", 0),
    ::stratum::hal::phal::PathEntry("ports", 0),
    ::stratum::hal::phal::PathEntry("transceiver", -1, false, false, true)
  }};

  // Setup Mock DB calls
  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB()).WillRepeatedly(Return(database));

  EXPECT_CALL(*database, DoMakeQuery(_))
      .WillOnce(Return(ByMove(std::move(db_query_mock))));

  EXPECT_CALL(*db_query, DoGet())
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

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDBService", "Get", _))
      .WillOnce(Return(::util::OkStatus()));

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Get(&context, req, &resp);
  EXPECT_TRUE(status.ok());
}

TEST_P(PhalDBServiceTest, SetRequestStrSuccess) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SetRequest req;
  ::stratum::hal::phal::SetResponse resp;

  // Setup Mock DB calls
  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB()).WillRepeatedly(Return(database));

  // AttributeValueMap for set
  ::stratum::hal::phal::AttributeValueMap attrs;

  // Create a path
  std::vector<::stratum::hal::phal::PathEntry> path = {
    ::stratum::hal::phal::PathEntry("fan_trays", 0),
    ::stratum::hal::phal::PathEntry("fans", 0),
    ::stratum::hal::phal::PathEntry("speed_control", -1, false, false, true)
  };
  attrs[path] = 20;

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<int32>(attrs[path]), Eq(20));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDBService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  update->set_str("fan_trays[0]/fans[0]/speed_control");
  update->mutable_value()->set_int32_val(20);

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Set(&context, req, &resp);
  EXPECT_TRUE(status.ok());
}

TEST_P(PhalDBServiceTest, SetRequestInvalidStrFail) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SetRequest req;
  ::stratum::hal::phal::SetResponse resp;

  // Setup Mock DB calls
  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB()).WillRepeatedly(Return(database));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDBService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  // Invalid request string
  update->set_str("/fan_trays[0]/fans[0]/speed_control");
  update->mutable_value()->set_int32_val(20);

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Set(&context, req, &resp);
  EXPECT_FALSE(status.ok());
}

TEST_P(PhalDBServiceTest, SetRequestPathSuccess) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SetRequest req;
  ::stratum::hal::phal::SetResponse resp;

  // Setup Mock DB calls
  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB()).WillRepeatedly(Return(database));

  // AttributeValueMap for set
  ::stratum::hal::phal::AttributeValueMap attrs;

  // Create a path
  std::vector<::stratum::hal::phal::PathEntry> path = {
    ::stratum::hal::phal::PathEntry("fan_trays", 0),
    ::stratum::hal::phal::PathEntry("fans", 0),
    ::stratum::hal::phal::PathEntry("rpm", -1, false, false, true)
  };
  attrs[path] = 1000.0;

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<double>(attrs[path]), Eq(1000.0));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDBService", "Set", _))
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
  auto status = stub_->Set(&context, req, &resp);
  EXPECT_TRUE(status.ok());
}

TEST_P(PhalDBServiceTest, SetRequestStringSuccess) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SetRequest req;
  ::stratum::hal::phal::SetResponse resp;

  // Setup Mock DB calls
  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB()).WillRepeatedly(Return(database));

  // AttributeValueMap for set
  ::stratum::hal::phal::AttributeValueMap attrs;

  // Create a path
  std::vector<::stratum::hal::phal::PathEntry> path = {
    ::stratum::hal::phal::PathEntry("fan_trays", 0),
    ::stratum::hal::phal::PathEntry("fans", 0),
    ::stratum::hal::phal::PathEntry("model", -1, false, false, true)
  };
  attrs[path] = std::string("model1234");

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<std::string>(attrs[path]), Eq("model1234"));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDBService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  update->set_str("fan_trays[0]/fans[0]/model");
  update->mutable_value()->set_string_val("model1234");

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Set(&context, req, &resp);
  EXPECT_TRUE(status.ok());
}

TEST_P(PhalDBServiceTest, SetRequestBoolSuccess) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SetRequest req;
  ::stratum::hal::phal::SetResponse resp;

  // Setup Mock DB calls
  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB()).WillRepeatedly(Return(database));

  // AttributeValueMap for set
  ::stratum::hal::phal::AttributeValueMap attrs;

  // Create a path
  std::vector<::stratum::hal::phal::PathEntry> path = {
    ::stratum::hal::phal::PathEntry("cards", 0),
    ::stratum::hal::phal::PathEntry("ports", 0),
    ::stratum::hal::phal::PathEntry("transceiver"),
    ::stratum::hal::phal::PathEntry("data_ready", -1, false, false, true)
  };
  attrs[path] = true;

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<bool>(attrs[path]), Eq(true));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDBService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  update->set_str("fan_trays[0]/fans[0]/data_ready");
  update->mutable_value()->set_bool_val(true);

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Set(&context, req, &resp);
  EXPECT_TRUE(status.ok());
}

TEST_P(PhalDBServiceTest, SetRequestFloatSuccess) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SetRequest req;
  ::stratum::hal::phal::SetResponse resp;

  // Setup Mock DB calls
  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB()).WillRepeatedly(Return(database));

  // AttributeValueMap for set
  ::stratum::hal::phal::AttributeValueMap attrs;

  // Create a path
  std::vector<::stratum::hal::phal::PathEntry> path = {
    ::stratum::hal::phal::PathEntry("cards", 0),
    ::stratum::hal::phal::PathEntry("ports", 0),
    ::stratum::hal::phal::PathEntry("transceiver"),
    ::stratum::hal::phal::PathEntry("float", -1, false, false, true)
  };
  attrs[path] = 10.0f;

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<float>(attrs[path]), Eq(10.0f));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDBService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  update->set_str("fan_trays[0]/fans[0]/float");
  update->mutable_value()->set_float_val(10.0f);

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Set(&context, req, &resp);
  EXPECT_TRUE(status.ok());
}

TEST_P(PhalDBServiceTest, SetRequestInt64Success) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SetRequest req;
  ::stratum::hal::phal::SetResponse resp;

  // Setup Mock DB calls
  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB()).WillRepeatedly(Return(database));

  // AttributeValueMap for set
  ::stratum::hal::phal::AttributeValueMap attrs;

  // Create a path
  std::vector<::stratum::hal::phal::PathEntry> path = {
    ::stratum::hal::phal::PathEntry("cards", 0),
    ::stratum::hal::phal::PathEntry("ports", 0),
    ::stratum::hal::phal::PathEntry("transceiver"),
    ::stratum::hal::phal::PathEntry("int64", -1, false, false, true)
  };
  attrs[path] = static_cast<int64>(10);

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<int64>(attrs[path]), Eq(static_cast<int64>(10)));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDBService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  update->set_str("fan_trays[0]/fans[0]/int64");
  update->mutable_value()->set_int64_val(static_cast<int64>(10));

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Set(&context, req, &resp);
  EXPECT_TRUE(status.ok());
}

TEST_P(PhalDBServiceTest, SetRequestUInt32Success) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SetRequest req;
  ::stratum::hal::phal::SetResponse resp;

  // Setup Mock DB calls
  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB()).WillRepeatedly(Return(database));

  // AttributeValueMap for set
  ::stratum::hal::phal::AttributeValueMap attrs;

  // Create a path
  std::vector<::stratum::hal::phal::PathEntry> path = {
    ::stratum::hal::phal::PathEntry("cards", 0),
    ::stratum::hal::phal::PathEntry("ports", 0),
    ::stratum::hal::phal::PathEntry("transceiver"),
    ::stratum::hal::phal::PathEntry("uint32", -1, false, false, true)
  };
  attrs[path] = static_cast<uint32>(10);

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<uint32>(attrs[path]), Eq(static_cast<uint32>(10)));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDBService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  update->set_str("fan_trays[0]/fans[0]/uint32");
  update->mutable_value()->set_uint32_val(static_cast<uint32>(10));

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Set(&context, req, &resp);
  EXPECT_TRUE(status.ok());
}

TEST_P(PhalDBServiceTest, SetRequestUInt64Success) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SetRequest req;
  ::stratum::hal::phal::SetResponse resp;

  // Setup Mock DB calls
  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB()).WillRepeatedly(Return(database));

  // AttributeValueMap for set
  ::stratum::hal::phal::AttributeValueMap attrs;

  // Create a path
  std::vector<::stratum::hal::phal::PathEntry> path = {
    ::stratum::hal::phal::PathEntry("cards", 0),
    ::stratum::hal::phal::PathEntry("ports", 0),
    ::stratum::hal::phal::PathEntry("transceiver"),
    ::stratum::hal::phal::PathEntry("uint64", -1, false, false, true)
  };
  attrs[path] = static_cast<uint64>(10);

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<uint64>(attrs[path]), Eq(static_cast<uint64>(10)));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDBService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  update->set_str("fan_trays[0]/fans[0]/uint64");
  update->mutable_value()->set_uint64_val(static_cast<uint64>(10));

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Set(&context, req, &resp);
  EXPECT_TRUE(status.ok());
}

TEST_P(PhalDBServiceTest, SetRequestBytesSuccess) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SetRequest req;
  ::stratum::hal::phal::SetResponse resp;

  // Setup Mock DB calls
  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB()).WillRepeatedly(Return(database));

  // AttributeValueMap for set
  ::stratum::hal::phal::AttributeValueMap attrs;

  // Create a path
  std::vector<::stratum::hal::phal::PathEntry> path = {
    ::stratum::hal::phal::PathEntry("cards", 0),
    ::stratum::hal::phal::PathEntry("ports", 0),
    ::stratum::hal::phal::PathEntry("transceiver"),
    ::stratum::hal::phal::PathEntry("bytes", -1, false, false, true)
  };
  attrs[path] = std::string("bytes");

  EXPECT_CALL(*database, Set(_))
      .WillOnce(DoAll(SaveArg<0>(&attrs), Return(::util::OkStatus())));

  // Check the path and value
  EXPECT_THAT(absl::get<std::string>(attrs[path]), Eq("bytes"));

  EXPECT_CALL(*auth_policy_checker_mock_, Authorize("PhalDBService", "Set", _))
      .WillOnce(Return(::util::OkStatus()));

  // Create Set request
  auto update = req.add_updates();
  update->set_str("fan_trays[0]/fans[0]/bytes");
  update->mutable_value()->set_bytes_val("bytes");

  // Invoke the RPC and validate the results.
  // Call and validate results.
  auto status = stub_->Set(&context, req, &resp);
  EXPECT_TRUE(status.ok());
}

TEST_P(PhalDBServiceTest, SubscribeRequestSuccess) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SubscribeRequest req;
  ::stratum::hal::phal::SubscribeResponse resp;

  // Returned PhalDB
  ::stratum::hal::phal::PhalDB phaldb_resp;
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

  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB())
      .WillRepeatedly(Return(database));

  // Create mock query
  auto db_query_mock = absl::make_unique<::stratum::hal::phal::QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  std::vector<::stratum::hal::phal::Path> paths = {{
    ::stratum::hal::phal::PathEntry("cards", 0),
    ::stratum::hal::phal::PathEntry("ports", 0),
    ::stratum::hal::phal::PathEntry("transceiver", -1, false, false, true)
  }};

  // Setup Mock DB calls
  EXPECT_CALL(*database, DoMakeQuery(_))
      .WillOnce(Return(ByMove(std::move(db_query_mock))));

  EXPECT_CALL(*auth_policy_checker_mock_,
      Authorize("PhalDBService", "Subscribe", _))
          .WillOnce(Return(::util::OkStatus()));

  EXPECT_CALL(*db_query, DoSubscribe(absl::Seconds(1)))
      .WillRepeatedly(Return(
        ::util::StatusOr<::stratum::hal::phal::PhalDB*>(&phaldb_resp)));

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

TEST_P(PhalDBServiceTest, SubscribeRequestFail) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SubscribeRequest req;
  ::stratum::hal::phal::SubscribeResponse resp;

  // Returned PhalDB
  ::stratum::hal::phal::PhalDB phaldb_resp;
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

  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB())
      .WillRepeatedly(Return(database));

  // Create mock query
  auto db_query_mock = absl::make_unique<::stratum::hal::phal::QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  std::vector<::stratum::hal::phal::Path> paths = {{
    ::stratum::hal::phal::PathEntry("cards", 0),
    ::stratum::hal::phal::PathEntry("ports", 0),
    ::stratum::hal::phal::PathEntry("transceiver", -1, false, false, true)
  }};

  // Setup Mock DB calls
  EXPECT_CALL(*database, DoMakeQuery(_))
      .WillOnce(Return(ByMove(std::move(db_query_mock))));

  EXPECT_CALL(*auth_policy_checker_mock_,
      Authorize("PhalDBService", "Subscribe", _))
          .WillOnce(Return(::util::OkStatus()));

  ::util::Status err_status = MAKE_ERROR(ERR_CANCELLED) << "cancel it";
  EXPECT_CALL(*db_query, DoSubscribe(absl::Seconds(1)))
      .WillRepeatedly(Return(
        ::util::StatusOr<::stratum::hal::phal::PhalDB*>(err_status)));

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

TEST_P(PhalDBServiceTest, SubscribeRequestStringFail) {
  ::grpc::ClientContext context;
  ::stratum::hal::phal::SubscribeRequest req;
  ::stratum::hal::phal::SubscribeResponse resp;

  // Returned PhalDB
  ::stratum::hal::phal::PhalDB phaldb_resp;
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

  auto phal_ptr = phal_mock_.get();
  auto database = database_mock_.get();
  EXPECT_CALL(*phal_ptr, GetPhalDB())
      .WillRepeatedly(Return(database));

  // Create mock query
  auto db_query_mock = absl::make_unique<::stratum::hal::phal::QueryMock>();
  // Need to get pointer before it gets moved
  auto db_query = db_query_mock.get();

  EXPECT_CALL(*auth_policy_checker_mock_,
      Authorize("PhalDBService", "Subscribe", _))
          .WillOnce(Return(::util::OkStatus()));

  EXPECT_CALL(*db_query, DoSubscribe(absl::Seconds(1)))
      .WillRepeatedly(Return(
        ::util::StatusOr<::stratum::hal::phal::PhalDB*>(&phaldb_resp)));

  // this query string should fail
  req.set_str("cards[0]/ports[f]/transceiver/");
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

INSTANTIATE_TEST_SUITE_P(PhalDBServiceTestWithMode, PhalDBServiceTest,
                        ::testing::Values(OPERATION_MODE_STANDALONE,
                                          OPERATION_MODE_COUPLED,
                                          OPERATION_MODE_SIM));


}  // namespace hal
}  // namespace stratum
