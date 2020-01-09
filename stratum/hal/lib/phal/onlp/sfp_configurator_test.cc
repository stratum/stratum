// Copyright 2019 Dell EMC
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

#include "stratum/hal/lib/phal/onlp/sfp_configurator.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/adapter.h"
#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/dummy_threadpool.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/hal/lib/phal/onlp/onlpphal_mock.h"
#include "stratum/hal/lib/phal/onlp/switch_configurator.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using ::stratum::test_utils::StatusIs;
using test_utils::EqualsProto;
using ::testing::_;
using ::testing::A;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

class OnlpSfpConfiguratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create switch configurator
    ASSERT_OK(SetupSwitchConfigurator());

    // Create a sfp configurator
    ASSERT_OK(SetupSfpConfigurator());
  }

  // Setup the Attribute Database
  // note: we construct the root group and sibblings to the
  //       transceiver node, we don't actually turn it into
  //       an attribute database as it is not needed for these
  //       test cases and would require us to be friend of the
  //       AttributeDatabase class.
  ::util::Status SetupSwitchConfigurator() {
    // Create the root group
    root_ = AttributeGroup::From(PhalDB::descriptor());
    auto mutable_root = root_->AcquireMutable();

    // Create fake card[0]/port[0]/transceiver
    ASSIGN_OR_RETURN(auto card, mutable_root->AddRepeatedChildGroup("cards"));
    auto mutable_card = card->AcquireMutable();
    ASSIGN_OR_RETURN(auto port, mutable_card->AddRepeatedChildGroup("ports"));
    auto mutable_port = port->AcquireMutable();
    ASSIGN_OR_RETURN(sfp_, mutable_port->AddChildGroup("transceiver"));

    // Create switch configurator
    onlp_wrapper_mock_ = absl::make_unique<OnlpWrapperMock>();
    onlpphal_mock_ = absl::make_unique<OnlpPhalMock>();
    ASSIGN_OR_RETURN(switch_configurator_,
                     OnlpSwitchConfigurator::Make(onlpphal_mock_.get(),
                                                  onlp_wrapper_mock_.get()));

    return ::util::OkStatus();
  }

  ::util::Status SetupAttributeDatabase() {
    // Setup the call backs
    SetupAddSfp(2, 9);

    // Setup OidList for Default switch configurator
    std::vector<OnlpOid> sfp_oids = {
        ONLP_SFP_ID_CREATE(static_cast<unsigned int>(port_))};
    EXPECT_CALL(*onlp_wrapper_mock_, GetOidList(ONLP_OID_TYPE_FLAG_SFP))
        .WillOnce(Return(::util::StatusOr<std::vector<OnlpOid>>(sfp_oids)));

    // Don't populate other nodes
    std::vector<OnlpOid> empty_oidlist = {};
    EXPECT_CALL(*onlp_wrapper_mock_, GetOidList(ONLP_OID_TYPE_FLAG_FAN))
        .WillOnce(
            Return(::util::StatusOr<std::vector<OnlpOid>>(empty_oidlist)));
    EXPECT_CALL(*onlp_wrapper_mock_, GetOidList(ONLP_OID_TYPE_FLAG_PSU))
        .WillOnce(
            Return(::util::StatusOr<std::vector<OnlpOid>>(empty_oidlist)));
    EXPECT_CALL(*onlp_wrapper_mock_, GetOidList(ONLP_OID_TYPE_FLAG_LED))
        .WillOnce(
            Return(::util::StatusOr<std::vector<OnlpOid>>(empty_oidlist)));
    EXPECT_CALL(*onlp_wrapper_mock_, GetOidList(ONLP_OID_TYPE_FLAG_THERMAL))
        .WillOnce(
            Return(::util::StatusOr<std::vector<OnlpOid>>(empty_oidlist)));

    EXPECT_CALL(*onlpphal_mock_, RegisterSfpConfigurator(_, _, _))
        .WillOnce(
            DoAll(SaveArg<2>(&sfp_configurator_), Return(::util::OkStatus())));

    // Now init the Attribute DB
    ASSIGN_OR_RETURN(database_, AttributeDatabase::MakePhalDB(
                                    std::move(switch_configurator_)));

    return ::util::OkStatus();
  }

  ::util::Status SetupSfpConfigurator() {
    SetupAddSfp(1, 2);

    // Create Datasourcec cache policy
    ASSIGN_OR_RETURN(auto cache, CachePolicyFactory::CreateInstance(
                                     CachePolicyConfig::TIMED_CACHE, 2));

    // Create a new data source
    ASSIGN_OR_RETURN(
        auto datasource,
        OnlpSfpDataSource::Make(port_, onlp_wrapper_mock_.get(), cache));

    // Create new sfp configurator instance
    ASSIGN_OR_RETURN(auto configurator,
                     OnlpSfpConfigurator::Make(card_id_, port_id_, datasource,
                                               sfp_, onlp_wrapper_mock_.get()));

    // Save a pointer to the configurator so we can access it
    configurator_ = configurator.get();

    // Move configurator to attribute group
    auto mutable_sfp = sfp_->AcquireMutable();
    mutable_sfp->AddRuntimeConfigurator(std::move(configurator));

    return ::util::OkStatus();
  }

  ::util::Status SetupAddSfp(int num_get_oid = 1, int num_get_sfp = 1) {
    if (num_get_oid > 0) {
      // Oid info
      ONLP_OID_STATUS_FLAG_SET(&mock_oid_info_, PRESENT);
      EXPECT_CALL(*onlp_wrapper_mock_, GetOidInfo(ONLP_SFP_ID_CREATE(port_)))
          .Times(num_get_oid)
          .WillRepeatedly(Return(OidInfo(mock_oid_info_)));
    }

    if (num_get_sfp > 0) {
      // Sfp Info
      ONLP_OID_STATUS_FLAG_SET(&mock_sfp_info_, PRESENT);
      mock_sfp_info_.type = ONLP_SFP_TYPE_SFP;

      mock_sfp_info_.sff.sfp_type = SFF_SFP_TYPE_SFP;
      strncpy(mock_sfp_info_.sff.vendor, "vendor-1234",
              sizeof(mock_sfp_info_.sff.vendor));
      strncpy(mock_sfp_info_.sff.model, "part-1234",
              sizeof(mock_sfp_info_.sff.model));
      strncpy(mock_sfp_info_.sff.serial, "serial-1234",
              sizeof(mock_sfp_info_.sff.serial));

      // SFF Diag info
      mock_sfp_info_.dom.voltage = 12;
      mock_sfp_info_.dom.temp = 33;
      mock_sfp_info_.dom.nchannels = SFF_DOM_CHANNEL_COUNT_MAX;
      for (int i = 0; i < mock_sfp_info_.dom.nchannels; i++) {
        mock_sfp_info_.dom.channels[i].fields =
            (SFF_DOM_FIELD_FLAG_RX_POWER | SFF_DOM_FIELD_FLAG_TX_POWER |
             SFF_DOM_FIELD_FLAG_RX_POWER_OMA | SFF_DOM_FIELD_FLAG_VOLTAGE);
        mock_sfp_info_.dom.channels[i].bias_cur = 2;
        mock_sfp_info_.dom.channels[i].rx_power = 10;
        mock_sfp_info_.dom.channels[i].rx_power_oma = 10;
        mock_sfp_info_.dom.channels[i].tx_power = 10;
      }

      // tuneable number of get sfp calls
      EXPECT_CALL(*onlp_wrapper_mock_, GetSfpInfo(ONLP_SFP_ID_CREATE(port_)))
          .Times(num_get_sfp)
          .WillRepeatedly(Return(SfpInfo(mock_sfp_info_)));
    }

    return ::util::OkStatus();
  }

  int card_id_ = 0;
  int port_id_ = 0;
  int slot_ = 1;
  int port_ = 1;
  std::unique_ptr<OnlpPhalMock> onlpphal_mock_;
  std::unique_ptr<OnlpWrapperMock> onlp_wrapper_mock_;
  OnlpSfpConfigurator* configurator_;
  onlp_oid_hdr_t mock_oid_info_;
  onlp_sfp_info_t mock_sfp_info_;
  std::unique_ptr<AttributeDatabase> database_;
  std::unique_ptr<AttributeGroup> root_;
  std::unique_ptr<SwitchConfigurator> switch_configurator_;
  SfpConfigurator* sfp_configurator_{nullptr};
  AttributeGroup* sfp_;
};

namespace {

TEST_F(OnlpSfpConfiguratorTest, CreateAndDestroy) {
  // Setup would have created it

  // now destroy the database which will in turn delete the configurator
  database_ = nullptr;
}

TEST_F(OnlpSfpConfiguratorTest, AddSfp) {
  // Add an Sfp transceiver
  EXPECT_OK(configurator_->AddSfp());
}

TEST_F(OnlpSfpConfiguratorTest, AddAndRemoveSfp) {
  // Add an Sfp transceiver
  EXPECT_OK(configurator_->AddSfp());

  // Now remove it
  EXPECT_OK(configurator_->RemoveSfp());
}

TEST_F(OnlpSfpConfiguratorTest, AddSfpTwice) {
  // First Add of an Sfp transceiver should be ok
  EXPECT_OK(configurator_->AddSfp());

  // second add should fail
  std::string error_message = "cards[" + std::to_string(card_id_) + "]/ports[" +
                              std::to_string(port_id_) + "]: sfp already added";
  EXPECT_THAT(configurator_->AddSfp(),
              StatusIs(_, _, HasSubstr(error_message)));
}

TEST_F(OnlpSfpConfiguratorTest, RemoveSfpError) {
  // second add should fail
  std::string error_message = "cards[" + std::to_string(card_id_) + "]/ports[" +
                              std::to_string(port_id_) +
                              "]: sfp has not been added";
  EXPECT_THAT(configurator_->RemoveSfp(),
              StatusIs(_, _, HasSubstr(error_message)));
}

TEST_F(OnlpSfpConfiguratorTest, HandlEventPresent) {
  // Handle Event
  EXPECT_OK(configurator_->HandleEvent(HW_STATE_PRESENT));
}

TEST_F(OnlpSfpConfiguratorTest, HandlEventNotPresent) {
  // Add an Sfp transceiver
  EXPECT_OK(configurator_->AddSfp());

  // Handle Event NOT_PRESENT
  EXPECT_OK(configurator_->HandleEvent(HW_STATE_NOT_PRESENT));
}

TEST_F(OnlpSfpConfiguratorTest, DBConcurrency) {
  // Path
  std::vector<phal::Path> paths = {
      {phal::PathEntry("cards", 0), phal::PathEntry("ports", 0),
       phal::PathEntry("transceiver", -1, false, false, true)}};

  // Create the attribute database
  ASSERT_OK(SetupAttributeDatabase());

  // initial AddSfp
  EXPECT_OK(sfp_configurator_->HandleEvent(HW_STATE_PRESENT));

  // create adaptor
  auto adapter = absl::make_unique<phal::Adapter>(database_.get());

  // Issue a Get (same as GetFrontPanelPortInfo would do)
  LOG(INFO) << "Issue Get 1";
  auto res = adapter->Get(paths);
  EXPECT_TRUE(res.ok());

  // Could check the returned msg
  {
    auto get_resp = res.ConsumeValueOrDie();
    LOG(INFO) << "get_resp 1" << std::endl << get_resp.get()->DebugString();
  }

  // Create writer and reader channels
  std::shared_ptr<Channel<phal::PhalDB>> channel =
      Channel<phal::PhalDB>::Create(128);

  auto writer = ChannelWriter<phal::PhalDB>::Create(channel);

  auto reader = ChannelReader<phal::PhalDB>::Create(channel);

  // Now start a subscribe on the transceiver
  LOG(INFO) << "Issue Subscribe";
  EXPECT_OK(adapter->Subscribe(paths, std::move(writer), absl::Seconds(2)));

  // Now wait for subscribe to go off
  PhalDB sub_resp;
  LOG(INFO) << "Issue Read 1";
  EXPECT_OK(reader->Read(&sub_resp, absl::Seconds(10)));

  // Inspect msg
  LOG(INFO) << "sub_resp 1" << std::endl << sub_resp.DebugString();

  // Remove the sfp
  LOG(INFO) << "Issue HandleEvent(HW_STATE_NOT_PRESENT";
  ONLP_OID_STATUS_FLAG_CLR(&mock_oid_info_, PRESENT);
  ONLP_OID_STATUS_FLAG_CLR(&mock_sfp_info_, PRESENT);
  EXPECT_OK(sfp_configurator_->HandleEvent(HW_STATE_NOT_PRESENT));

  // Issue another Get (same as GetFrontPanelPortInfo would do)
  LOG(INFO) << "Issue Get 2";
  res = adapter->Get(paths);
  EXPECT_TRUE(res.ok());

  // Could check the returned msg
  {
    auto get_resp = res.ConsumeValueOrDie();
    LOG(INFO) << "get_resp 2" << std::endl << get_resp.get()->DebugString();
  }

  // Now wait for subscribe to go off again (should explode)
  LOG(INFO) << "Issue Read 2";
  EXPECT_OK(reader->Read(&sub_resp, absl::Seconds(10)));

  // Inspect msg
  LOG(INFO) << "sub_resp 2" << std::endl << sub_resp.DebugString();

  // Add the sfp
  LOG(INFO) << "Issue HandleEvent(HW_STATE_PRESENT)";
  ONLP_OID_STATUS_FLAG_SET(&mock_oid_info_, PRESENT);
  ONLP_OID_STATUS_FLAG_SET(&mock_sfp_info_, PRESENT);
  EXPECT_OK(sfp_configurator_->HandleEvent(HW_STATE_PRESENT));

  // Issue another Get (same as GetFrontPanelPortInfo would do)
  LOG(INFO) << "Issue Get 3";
  res = adapter->Get(paths);
  EXPECT_TRUE(res.ok());

  // Could check the returned msg
  {
    auto get_resp = res.ConsumeValueOrDie();
    LOG(INFO) << "get_resp 3" << std::endl << get_resp.get()->DebugString();
  }

  // Now wait for subscribe to go off again (should explode)
  LOG(INFO) << "Issue Read 3";
  EXPECT_OK(reader->Read(&sub_resp, absl::Seconds(10)));

  // Inspect msg
  LOG(INFO) << "sub_resp 3" << std::endl << sub_resp.DebugString();
}

}  // namespace
}  // namespace onlp
}  // namespace phal
}  // namespace hal

}  // namespace stratum
