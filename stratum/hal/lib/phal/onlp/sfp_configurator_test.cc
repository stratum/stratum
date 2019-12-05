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

#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/dummy_threadpool.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/hal/lib/phal/onlp/onlpphal_mock.h"
#include "stratum/hal/lib/phal/onlp/switch_configurator.h"

// Testing header files
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

int main(int argc, char** argv) {
  ::google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  InitGoogle(argv[0], &argc, &argv, true);
  return RUN_ALL_TESTS();
}

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
using ::testing::StrictMock;

class OnlpSfpConfiguratorTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Create the attribute database
    ASSERT_OK(SetupAttributeDatabase());

    // Create a sfp configurator
    ASSERT_OK(SetupSfpConfigurator());
  }

  // Setup the Attribute Database
  // note: we construct the root group and sibblings to the
  //       transceiver node, we don't actually turn it into
  //       an attribute database as it is not needed for these
  //       test cases and would require us to be friend of the
  //       AttributeDatabase class.
  ::util::Status SetupAttributeDatabase() {
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
    onlpphal_.InitializeOnlpInterface();
    onlp_interface_ = onlpphal_.GetOnlpInterface();
    ASSIGN_OR_RETURN(auto switch_configurator,
                     OnlpSwitchConfigurator::Make(&onlpphal_, onlp_interface_));

    return ::util::OkStatus();
  }

  ::util::Status SetupAttributeDatabase1() {
    // Create the root group
    std::unique_ptr<AttributeGroup> root =
        AttributeGroup::From(PhalDB::descriptor());
    auto mutable_root = root->AcquireMutable();

    // Create switch configurator
    onlpphal_.InitializeOnlpInterface();
    onlp_interface_ = onlpphal_.GetOnlpInterface();
    ASSIGN_OR_RETURN(auto switch_configurator,
                     OnlpSwitchConfigurator::Make(&onlpphal_, onlp_interface_));

    // Create fake PhalInitconfig
    PhalInitConfig phal_config;
    auto card = phal_config.add_cards();
    auto port = card->add_ports();
    port->set_id(id_);
    port->set_physical_port_type(PHYSICAL_PORT_TYPE_SFP_CAGE);
    auto mutable_cache = port->mutable_cache_policy();
    mutable_cache->set_type(TIMED_CACHE);
    mutable_cache->set_timed_value(1);

    // Save it to a phal init config file
    RETURN_IF_ERROR(WriteProtoToTextFile(phal_config, phal_config_path_));

    // Now init the Attribute DB
    ASSIGN_OR_RETURN(database_, AttributeDatabase::MakePhalDB(
                                    std::move(switch_configurator)));

    return ::util::OkStatus();
  }

  ::util::Status SetupSfpConfigurator() {
    SetupAddSfp(1, 2);

    // Create Datasourcec cache policy
    ASSIGN_OR_RETURN(auto cache, CachePolicyFactory::CreateInstance(
                                     ::stratum::hal::TIMED_CACHE, 2));

    // Create a new data source
    ASSIGN_OR_RETURN(auto datasource,
                     OnlpSfpDataSource::Make(id_, onlp_interface_, cache));

    // Create new sfp configurator instance
    ASSIGN_OR_RETURN(
        auto configurator,
        OnlpSfpConfigurator::Make(id_, datasource, sfp_, onlp_interface_));

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
      onlp_oid_hdr_t mock_oid_info;
      mock_oid_info.status = ONLP_OID_STATUS_FLAG_PRESENT;
      EXPECT_CALL(*onlp_interface_, GetOidInfo(ONLP_SFP_ID_CREATE(id_)))
          .Times(num_get_oid)
          .WillRepeatedly(Return(OidInfo(mock_oid_info)));
    }

    if (num_get_sfp > 0) {
      // Sfp Info
      onlp_sfp_info_t mock_sfp_info;
      mock_sfp_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
      mock_sfp_info.type = ONLP_SFP_TYPE_SFP;

      // SFF Diag info
      mock_sfp_info.dom.voltage = 12;
      mock_sfp_info.dom.nchannels = SFF_DOM_CHANNEL_COUNT_MAX;
      for (int i = 0; i < mock_sfp_info.dom.nchannels; i++) {
        mock_sfp_info.dom.channels[i].fields =
            (SFF_DOM_FIELD_FLAG_RX_POWER | SFF_DOM_FIELD_FLAG_TX_POWER |
             SFF_DOM_FIELD_FLAG_RX_POWER_OMA | SFF_DOM_FIELD_FLAG_VOLTAGE);
        mock_sfp_info.dom.channels[i].bias_cur = 2;
        mock_sfp_info.dom.channels[i].rx_power = 10;
        mock_sfp_info.dom.channels[i].rx_power_oma = 10;
        mock_sfp_info.dom.channels[i].tx_power = 10;
      }

      // tuneable number of get sfp calls
      EXPECT_CALL(*onlp_interface_, GetSfpInfo(ONLP_SFP_ID_CREATE(id_)))
          .Times(num_get_sfp)
          .WillRepeatedly(Return(SfpInfo(mock_sfp_info)));
    }

    return ::util::OkStatus();
  }

  int id_ = 1;
  OnlpPhalMock onlpphal_;
  MockOnlpWrapper* onlp_interface_;
  OnlpSfpConfigurator* configurator_;
  std::unique_ptr<AttributeDatabase> database_;
  std::unique_ptr<AttributeGroup> root_;
  AttributeGroup* sfp_;
  string phal_config_path_ = "phal_init_config.pb.txt";
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
  std::string error_message =
      "sfp id " + std::to_string(id_) + " already added";
  EXPECT_THAT(configurator_->AddSfp(),
              StatusIs(_, _, HasSubstr(error_message)));
}

TEST_F(OnlpSfpConfiguratorTest, RemoveSfpError) {
  // second add should fail
  std::string error_message =
      "sfp id " + std::to_string(id_) + " has not been added";
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

}  // namespace
}  // namespace onlp
}  // namespace phal
}  // namespace hal

}  // namespace stratum
