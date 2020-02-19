/*
 * Copyright 2020-present Open Networking Foundation
 * Copyright 2020 PLVision
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

/*
  TaiInterfaceTest testing all TAI related classes like TaiWrapper, Module,
  HostInterface and NetworkInterface tests is based on TAI stub

  Each test should wait for some time(in our case this is 200 milliseconds) for
  modules initialization in a different thread
*/

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "stratum/hal/lib/phal/tai/tai_wrapper/host_interface.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/module.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/network_interface.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_wrapper.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

struct tai_param {
  tai_attr_id_t attr_id;
  std::string attr_name;
  std::string value_to_set;
  std::string expected_value;

  std::string removeExtraCharacters() const {
    auto str = attr_name;
    std::replace(str.begin(), str.end(), '-', '_');
    return str;
  }

  friend std::ostream& operator<<(std::ostream& os, const tai_param& dt);
};

std::ostream& operator<<(std::ostream& os, const tai_param& dt) {
  os << dt.attr_name << '(' << dt.attr_id << ") valueToSet: " << dt.value_to_set
     << " expectedValue: " << dt.expected_value;
  return os;
}

/************************** TaiWrapperTest ********************************/
class TaiWrapperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  TaiWrapper wrapper_;
};

TEST_F(TaiWrapperTest, TaiWrapperValidPath_Test) {
  EXPECT_TRUE(wrapper_.IsObjectValid({{TAI_OBJECT_TYPE_MODULE, 0}}));
  EXPECT_TRUE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 0}, {TAI_OBJECT_TYPE_HOSTIF, 0}}));
  EXPECT_TRUE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 0}, {TAI_OBJECT_TYPE_HOSTIF, 1}}));
  EXPECT_TRUE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 0}, {TAI_OBJECT_TYPE_NETWORKIF, 0}}));

  EXPECT_TRUE(wrapper_.IsObjectValid({{TAI_OBJECT_TYPE_MODULE, 1}}));
  EXPECT_TRUE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 1}, {TAI_OBJECT_TYPE_HOSTIF, 0}}));
  EXPECT_TRUE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 1}, {TAI_OBJECT_TYPE_HOSTIF, 1}}));
  EXPECT_TRUE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 1}, {TAI_OBJECT_TYPE_NETWORKIF, 0}}));

  EXPECT_TRUE(wrapper_.IsObjectValid({{TAI_OBJECT_TYPE_MODULE, 2}}));
  EXPECT_TRUE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 2}, {TAI_OBJECT_TYPE_HOSTIF, 0}}));
  EXPECT_TRUE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 2}, {TAI_OBJECT_TYPE_HOSTIF, 1}}));
  EXPECT_TRUE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 2}, {TAI_OBJECT_TYPE_NETWORKIF, 0}}));

  EXPECT_TRUE(wrapper_.IsObjectValid({{TAI_OBJECT_TYPE_MODULE, 3}}));
  EXPECT_TRUE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 3}, {TAI_OBJECT_TYPE_HOSTIF, 0}}));
  EXPECT_TRUE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 3}, {TAI_OBJECT_TYPE_HOSTIF, 1}}));
  EXPECT_TRUE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 3}, {TAI_OBJECT_TYPE_NETWORKIF, 0}}));
}

TEST_F(TaiWrapperTest, TaiWrapperInvalidPath_Test) {
  EXPECT_FALSE(wrapper_.IsObjectValid({{TAI_OBJECT_TYPE_NULL, 0}}));
  EXPECT_FALSE(wrapper_.IsObjectValid({{TAI_OBJECT_TYPE_MODULE, 5}}));
  EXPECT_FALSE(wrapper_.IsObjectValid({{TAI_OBJECT_TYPE_HOSTIF, 0}}));
  EXPECT_FALSE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_HOSTIF, 0}, {TAI_OBJECT_TYPE_HOSTIF, 1}}));
  EXPECT_FALSE(wrapper_.IsObjectValid({{TAI_OBJECT_TYPE_NETWORKIF, 3}}));
  EXPECT_FALSE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_NETWORKIF, 0}, {TAI_OBJECT_TYPE_MODULE, 0}}));
  EXPECT_FALSE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 3}, {TAI_OBJECT_TYPE_NETWORKIF, 1}}));
  EXPECT_FALSE(wrapper_.IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 3}, {TAI_OBJECT_TYPE_HOSTIF, 2}}));
}

TEST_F(TaiWrapperTest, TaiWrapperInitialization_Test) {
  EXPECT_TRUE(wrapper_.IsModuleIdValid(0));
  EXPECT_TRUE(wrapper_.IsModuleIdValid(1));
  EXPECT_TRUE(wrapper_.IsModuleIdValid(2));
  EXPECT_TRUE(wrapper_.IsModuleIdValid(3));

  // module id 0
  std::shared_ptr<Module> module = wrapper_.GetModule(0).lock();
  EXPECT_TRUE(module->IsHostInterfaceValid(0));
  EXPECT_TRUE(module->IsHostInterfaceValid(1));
  EXPECT_TRUE(module->IsNetworkInterfaceValid(0));

  // module id 1
  module = wrapper_.GetModule(1).lock();
  EXPECT_TRUE(module->IsHostInterfaceValid(0));
  EXPECT_TRUE(module->IsHostInterfaceValid(1));
  EXPECT_TRUE(module->IsNetworkInterfaceValid(0));

  // module id 2
  module = wrapper_.GetModule(2).lock();
  EXPECT_TRUE(module->IsHostInterfaceValid(0));
  EXPECT_TRUE(module->IsHostInterfaceValid(1));
  EXPECT_TRUE(module->IsNetworkInterfaceValid(0));

  // module id 3
  module = wrapper_.GetModule(3).lock();
  EXPECT_TRUE(module->IsHostInterfaceValid(0));
  EXPECT_TRUE(module->IsHostInterfaceValid(1));
  EXPECT_TRUE(module->IsNetworkInterfaceValid(0));
}

TEST_F(TaiWrapperTest, TaiGetObjectByPath_Test) {
  std::shared_ptr<TaiObject> object =
      wrapper_.GetObject({TAI_OBJECT_TYPE_MODULE, 0}).lock();
  EXPECT_NE(object, nullptr);

  object = wrapper_
               .GetObject({{TAI_OBJECT_TYPE_MODULE, 0},
                           {TAI_OBJECT_TYPE_NETWORKIF, 0}})
               .lock();
  EXPECT_NE(object, nullptr);

  object = wrapper_
               .GetObject({{TAI_OBJECT_TYPE_MODULE, 1},
                           {TAI_OBJECT_TYPE_NETWORKIF, 0}})
               .lock();
  EXPECT_NE(object, nullptr);

  object =
      wrapper_
          .GetObject({{TAI_OBJECT_TYPE_MODULE, 2}, {TAI_OBJECT_TYPE_HOSTIF, 0}})
          .lock();
  EXPECT_NE(object, nullptr);

  object =
      wrapper_
          .GetObject({{TAI_OBJECT_TYPE_MODULE, 3}, {TAI_OBJECT_TYPE_HOSTIF, 1}})
          .lock();
  EXPECT_NE(object, nullptr);
}

/**************************** TaiModuleTest ***********************************/
class TaiModuleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  TaiWrapper wrapper_;
};

TEST_F(TaiModuleTest, TaiModuleSetReadWriteAttributes_Test) {
  const std::shared_ptr<Module> module = wrapper_.GetModule(0).lock();

  std::string result{"unknown"};
  tai_serialize_option_t option(TaiAttribute::DefaultDeserializeOption());
  TaiAttribute attribute =
      module->GetAlocatedAttributeObject(TAI_MODULE_ATTR_ADMIN_STATUS);
  int ret = tai_deserialize_attribute_value(result.c_str(), attribute.kMeta,
                                            &attribute.attr.value, &option);
  EXPECT_GE(ret, 0);
  int stat = module->SetAttribute(&attribute.attr);
  ASSERT_EQ(stat, TAI_STATUS_SUCCESS);

  result.clear();
  attribute = module->GetAttribute(TAI_MODULE_ATTR_ADMIN_STATUS, &stat);
  ASSERT_EQ(stat, TAI_STATUS_SUCCESS);
  EXPECT_EQ("\"unknown\"", attribute.SerializeAttribute());
}

TEST_F(TaiModuleTest, TaiModuleSetAttributeByName_Test) {
  const std::shared_ptr<Module> module = wrapper_.GetModule(0).lock();

  std::string result{"down"};
  tai_serialize_option_t option(TaiAttribute::DefaultDeserializeOption());
  TaiAttribute attribute = module->GetAlocatedAttributeObject("admin-status");
  int ret = tai_deserialize_attribute_value(result.c_str(), attribute.kMeta,
                                            &attribute.attr.value, &option);
  EXPECT_GE(ret, 0);
  int stat = module->SetAttribute(&attribute.attr);
  ASSERT_EQ(stat, TAI_STATUS_SUCCESS);

  result.clear();
  attribute = module->GetAttribute(attribute.attr.id, &stat);
  ASSERT_EQ(stat, TAI_STATUS_SUCCESS);
  EXPECT_EQ("\"down\"", attribute.SerializeAttribute());
}

/************************** TaiHostInterfaceTest ******************************/
class TaiHostInterfaceTest : public ::testing::TestWithParam<tai_param> {
 protected:
  void SetUp() override {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

 public:
  static std::vector<tai_param> host_parameters() {
    return {{TAI_HOST_INTERFACE_ATTR_SIGNAL_RATE, "signal-rate", "100-gbe",
             "\"100-gbe\""},
            {TAI_HOST_INTERFACE_ATTR_FEC_TYPE, "fec-type", "rs", "\"rs\""},
            {TAI_HOST_INTERFACE_ATTR_LOOPBACK_TYPE, "loopback-type", "deep",
             "\"deep\""}};
  }

  TaiWrapper wrapper_;
};

INSTANTIATE_TEST_CASE_P(
    TaiHostInterfaceParametersTest, TaiHostInterfaceTest,
    testing::ValuesIn(TaiHostInterfaceTest::host_parameters()),
    [](const ::testing::TestParamInfo<tai_param>& param) {
      return param.param.removeExtraCharacters();
    });

TEST_P(TaiHostInterfaceTest, TaiHostInterfaceSetAttributes_Test) {
  std::shared_ptr<HostInterface> hostif =
      wrapper_.GetModule(0).lock()->GetHostInterface(0).lock();

  auto param = GetParam();
  TaiAttribute tai_attr = hostif->GetAlocatedAttributeObject(param.attr_id);
  tai_serialize_option_t option(TaiAttribute::DefaultDeserializeOption());
  int ret = tai_deserialize_attribute_value(param.value_to_set.c_str(),
                                            tai_attr.kMeta,
                                            &tai_attr.attr.value, &option);
  EXPECT_GE(ret, 0);
  int stat = hostif->SetAttribute(&tai_attr.attr);
  ASSERT_EQ(stat, TAI_STATUS_SUCCESS);

  tai_attr = hostif->GetAttribute(param.attr_id, &stat);
  ASSERT_EQ(stat, TAI_STATUS_SUCCESS);
  EXPECT_EQ(param.expected_value, tai_attr.SerializeAttribute());
}

TEST_P(TaiHostInterfaceTest, TaiHostInterfaceSetAttributeByName_Test) {
  std::shared_ptr<HostInterface> hostif =
      wrapper_.GetModule(0).lock()->GetHostInterface(0).lock();

  auto param = GetParam();
  TaiAttribute tai_attr = hostif->GetAlocatedAttributeObject(param.attr_name);
  tai_serialize_option_t option(TaiAttribute::DefaultDeserializeOption());
  int ret = tai_deserialize_attribute_value(param.value_to_set.c_str(),
                                            tai_attr.kMeta,
                                            &tai_attr.attr.value, &option);
  EXPECT_GE(ret, 0);

  int stat = hostif->SetAttribute(&tai_attr.attr);
  ASSERT_EQ(stat, TAI_STATUS_SUCCESS);

  tai_attr = hostif->GetAttribute(param.attr_id, &stat);
  ASSERT_EQ(stat, TAI_STATUS_SUCCESS);
  EXPECT_EQ(param.expected_value, tai_attr.SerializeAttribute());
}

/************************** TaiNetworkInterfaceTest ***************************/
class TaiNetworkInterfaceTest : public ::testing::TestWithParam<tai_param> {
 protected:
  void SetUp() override {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

 public:
  static std::vector<tai_param> network_parameters() {
    return {
        {TAI_NETWORK_INTERFACE_ATTR_TX_DIS, "tx-dis", "true", "true"},
        {TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER, "output-power", "12.5",
         "12.500000"},
        {TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ, "tx-laser-freq", "235",
         "235"},
        {TAI_NETWORK_INTERFACE_ATTR_TX_FINE_TUNE_LASER_FREQ,
         "tx-fine-tune-laser-freq", "123", "123"},
        {TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT, "modulation-format",
         "64-qam", "\"64-qam\""},
        {TAI_NETWORK_INTERFACE_ATTR_DIFFERENTIAL_ENCODING,
         "differential-encoding", "false", "false"},
        {TAI_NETWORK_INTERFACE_ATTR_PULSE_SHAPING_TX, "pulse-shaping-tx",
         "true", "true"},
        {TAI_NETWORK_INTERFACE_ATTR_PULSE_SHAPING_RX, "pulse-shaping-rx",
         "true", "true"},
        {TAI_NETWORK_INTERFACE_ATTR_PULSE_SHAPING_TX_BETA,
         "pulse-shaping-tx-beta", "25.42", "25.420000"},
        {TAI_NETWORK_INTERFACE_ATTR_PULSE_SHAPING_RX_BETA,
         "pulse-shaping-rx-beta", "23.51", "23.510000"},
        {TAI_NETWORK_INTERFACE_ATTR_VOA_RX, "voa-rx", "11.95", "11.950000"},
        {TAI_NETWORK_INTERFACE_ATTR_LOOPBACK_TYPE, "loopback-type", "shallow",
         "\"shallow\""},
        {TAI_NETWORK_INTERFACE_ATTR_PRBS_TYPE, "prbs-type", "prbs23",
         "\"prbs23\""},
        {TAI_NETWORK_INTERFACE_ATTR_CH1_FREQ, "ch1-freq", "34", "34"},
    };
  }

 protected:
  TaiWrapper wrapper_;
};

INSTANTIATE_TEST_CASE_P(
    TaiNetworkInterfaceTest, TaiNetworkInterfaceTest,
    testing::ValuesIn(TaiNetworkInterfaceTest::network_parameters()),
    [](const ::testing::TestParamInfo<tai_param>& param) {
      return param.param.removeExtraCharacters();
    });

TEST_P(TaiNetworkInterfaceTest, TaiNetworkInterfaceSetAttributes_Test) {
  std::shared_ptr<NetworkInterface> netif =
      wrapper_.GetModule(0).lock()->GetNetworkInterface(0).lock();

  auto param = GetParam();
  TaiAttribute tai_attr = netif->GetAlocatedAttributeObject(param.attr_id);
  tai_serialize_option_t option(TaiAttribute::DefaultDeserializeOption());
  int ret = tai_deserialize_attribute_value(param.value_to_set.c_str(),
                                            tai_attr.kMeta,
                                            &tai_attr.attr.value, &option);
  EXPECT_GE(ret, 0);
  int stat = netif->SetAttribute(&tai_attr.attr);
  ASSERT_EQ(stat, TAI_STATUS_SUCCESS);

  tai_attr = netif->GetAttribute(param.attr_id, &stat);
  ASSERT_EQ(stat, TAI_STATUS_SUCCESS);
  EXPECT_EQ(param.expected_value, tai_attr.SerializeAttribute());
}

TEST_P(TaiNetworkInterfaceTest, TaiNetworkInterfaceSetAttributeByName_Test) {
  std::shared_ptr<NetworkInterface> netif =
      wrapper_.GetModule(0).lock()->GetNetworkInterface(0).lock();

  auto param = GetParam();
  TaiAttribute tai_attr = netif->GetAlocatedAttributeObject(param.attr_name);
  tai_serialize_option_t option(TaiAttribute::DefaultDeserializeOption());
  int ret = tai_deserialize_attribute_value(param.value_to_set.c_str(),
                                            tai_attr.kMeta,
                                            &tai_attr.attr.value, &option);
  EXPECT_GE(ret, 0);

  int stat = netif->SetAttribute(&tai_attr.attr);
  ASSERT_EQ(stat, TAI_STATUS_SUCCESS);

  tai_attr = netif->GetAttribute(param.attr_id, &stat);
  ASSERT_EQ(stat, TAI_STATUS_SUCCESS);
  EXPECT_EQ(param.expected_value, tai_attr.SerializeAttribute());
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
