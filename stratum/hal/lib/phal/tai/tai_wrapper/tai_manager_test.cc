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

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "stratum/hal/lib/phal/tai/tai_wrapper/host_interface.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/module.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/network_interface.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_manager.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_wrapper.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_object_mock.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_wrapper_mock.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_types_converter.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;

MATCHER(IsObjectSuported, "") {
  std::vector<TaiPath> supportedPathes{
      {{TAI_OBJECT_TYPE_MODULE, 0}},

      {{TAI_OBJECT_TYPE_MODULE, 0}, {TAI_OBJECT_TYPE_HOSTIF, 0}},

      {{TAI_OBJECT_TYPE_MODULE, 0}, {TAI_OBJECT_TYPE_NETWORKIF, 1}},

      {{TAI_OBJECT_TYPE_MODULE, 1}},
  };

  for (const auto& supportedPath : supportedPathes) {
    if (supportedPath == arg) return true;
  }
  return false;
}

class TaiManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    wrapper = new TaiWrapperMock();
    std::unique_ptr<TaiWrapperMock> wrapper_uniq(wrapper);
    manager = new TaiManager(std::move(wrapper_uniq));
  }
  void TearDown() override {
    delete manager;  // also deletes the wrapper
  }
  TaiManager* manager;
  TaiWrapperMock* wrapper;
};

TEST_F(TaiManagerTest, CorrectObjectCreation_Test) {
  const TaiPath callPath = {{TAI_OBJECT_TYPE_MODULE, 0},
                            {TAI_OBJECT_TYPE_HOSTIF, 0}};

  EXPECT_CALL(*wrapper, IsObjectValid(_))
      .Times(5)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  const TaiPath expectPath = {{TAI_OBJECT_TYPE_MODULE, 0},
                              {TAI_OBJECT_TYPE_HOSTIF, 0}};
  EXPECT_TRUE(manager->IsObjectValid({{TAI_OBJECT_TYPE_MODULE, 0}}));

  EXPECT_TRUE(manager->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 0}, {TAI_OBJECT_TYPE_HOSTIF, 0}}));

  EXPECT_TRUE(manager->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 0}, {TAI_OBJECT_TYPE_NETWORKIF, 1}}));

  EXPECT_TRUE(manager->IsObjectValid({{TAI_OBJECT_TYPE_MODULE, 1}}));

  EXPECT_FALSE(manager->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 0}, {TAI_OBJECT_TYPE_NETWORKIF, 0}}));
}

TEST_F(TaiManagerTest, SetFrequencyValueWithSuccess_Test) {
  const uint64 kFrequency = 45;

  std::shared_ptr<TaiObjectMock> object_mock =
      std::make_shared<TaiObjectMock>();

  tai_attr_metadata_t dummy_tai_metadata = {};

  EXPECT_CALL(*object_mock.get(), SetAttribute(_))
      .Times(1)
      .WillOnce(Return(TAI_STATUS_SUCCESS));

  EXPECT_CALL(*object_mock.get(), GetAlocatedAttributeObject(
                                      TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ))
      .Times(1)
      .WillOnce(Return(TaiAttribute(TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ,
                                    &dummy_tai_metadata)));

  EXPECT_CALL(*wrapper,
              GetObject(TaiPath({{TAI_OBJECT_TYPE_MODULE, 0},
                                 {TAI_OBJECT_TYPE_NETWORKIF, 1}})))
      .Times(1)
      .WillOnce(Return(object_mock));

  auto kStatus = manager->SetValue<uint64>(
      kFrequency, TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ, {0, 1});
  EXPECT_TRUE(kStatus.ok());
}

TEST_F(TaiManagerTest, SetFrequencyValueWithInvalidAttributeValue_Test) {
  const uint64 kFrequency = 45;

  std::shared_ptr<TaiObjectMock> object_mock =
      std::make_shared<TaiObjectMock>();

  tai_attr_metadata_t dummy_tai_metadata = {};

  EXPECT_CALL(*object_mock.get(), SetAttribute(_))
      .Times(0);

  EXPECT_CALL(*object_mock.get(), GetAlocatedAttributeObject(
                                      TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ))
      .Times(1)
      .WillOnce(Return(TaiAttribute(TAI_NETWORK_INTERFACE_ATTR_START,
                                    &dummy_tai_metadata)));

  EXPECT_CALL(*wrapper,
              GetObject(TaiPath({{TAI_OBJECT_TYPE_MODULE, 0},
                                 {TAI_OBJECT_TYPE_NETWORKIF, 1}})))
      .Times(1)
      .WillOnce(Return(object_mock));

  auto kStatus = manager->SetValue<uint64>(
      kFrequency, TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ, {0, 1});
  EXPECT_FALSE(kStatus.ok());
}

TEST_F(TaiManagerTest, GetFrequencyValueWithSuccess_Test) {
  tai_attr_metadata_t dummy_tai_metadata = {.objecttype =
                                                TAI_OBJECT_TYPE_NETWORKIF};
  dummy_tai_metadata.attrvaluetype = TAI_ATTR_VALUE_TYPE_U64;
  TaiAttribute dummy_tai_attribute(TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ,
                                   &dummy_tai_metadata);

  const uint64 kFrequency = 2350000;
  dummy_tai_attribute.attr.value.u64 = kFrequency;

  std::shared_ptr<TaiObjectMock> object_mock =
      std::make_shared<TaiObjectMock>();

  EXPECT_CALL(*object_mock.get(), GetAttribute(dummy_tai_attribute.attr.id, _))
      .WillOnce(DoAll(Invoke([](tai_attr_id_t, tai_status_t* return_status) {
                        *return_status = TAI_STATUS_SUCCESS;
                      }),
                      Return(dummy_tai_attribute)));

  EXPECT_CALL(*wrapper,
              GetObject(TaiPath({{TAI_OBJECT_TYPE_MODULE, 0},
                                 {TAI_OBJECT_TYPE_NETWORKIF, 1}})))
      .Times(1)
      .WillOnce(Return(object_mock));

  auto valueOrStatus = manager->GetValue<uint64>(
      TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ, {0, 1});
  EXPECT_TRUE(valueOrStatus.ok());

  EXPECT_EQ(valueOrStatus.ConsumeValueOrDie(), kFrequency);
}

TEST_F(TaiManagerTest, SetModulationValueWithSuccess_Test) {
  const int32 kModulation = TAI_NETWORK_INTERFACE_MODULATION_FORMAT_DP_8_QAM;

  std::shared_ptr<TaiObjectMock> object_mock =
      std::make_shared<TaiObjectMock>();

  tai_attr_metadata_t dummy_tai_metadata = {};

  EXPECT_CALL(*object_mock.get(), SetAttribute(_))
      .Times(1)
      .WillOnce(Return(TAI_STATUS_SUCCESS));

  EXPECT_CALL(
      *object_mock.get(),
      GetAlocatedAttributeObject(TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT))
      .Times(1)
      .WillOnce(Return(TaiAttribute(
          TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT, &dummy_tai_metadata)));

  EXPECT_CALL(*wrapper,
              GetObject(TaiPath({{TAI_OBJECT_TYPE_MODULE, 0},
                                 {TAI_OBJECT_TYPE_NETWORKIF, 1}})))
      .Times(1)
      .WillOnce(Return(object_mock));

  auto kStatus = manager->SetValue<int32>(
      kModulation, TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT, {0, 1});
  EXPECT_TRUE(kStatus.ok());
}

TEST_F(TaiManagerTest, TryToSetModulationValueWithInvalidObjectId_Test) {
  const int32 kModulation = TAI_NETWORK_INTERFACE_MODULATION_FORMAT_DP_8_QAM;

  std::weak_ptr<TaiObjectMock> dummy_weak_ptr;
  EXPECT_CALL(*wrapper,
              GetObject(TaiPath({{TAI_OBJECT_TYPE_MODULE, 6},
                                 {TAI_OBJECT_TYPE_NETWORKIF, 1}})))
      .Times(1)
      .WillOnce(Return(dummy_weak_ptr));

  auto kStatus = manager->SetValue<int32>(
      kModulation, TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT, {6, 1});
  EXPECT_FALSE(kStatus.ok());
}

TEST_F(TaiManagerTest, GetModulationValueWithSuccess_Test) {
  tai_attr_metadata_t dummy_tai_metadata = {.objecttype =
                                                TAI_OBJECT_TYPE_NETWORKIF};
  dummy_tai_metadata.attrvaluetype = TAI_ATTR_VALUE_TYPE_S32;
  TaiAttribute dummy_tai_attribute(TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT,
                                   &dummy_tai_metadata);

  const int32 kModulation = TAI_NETWORK_INTERFACE_MODULATION_FORMAT_DP_8_QAM;
  dummy_tai_attribute.attr.value.s32 = kModulation;

  std::shared_ptr<TaiObjectMock> object_mock =
      std::make_shared<TaiObjectMock>();

  EXPECT_CALL(*object_mock.get(), GetAttribute(dummy_tai_attribute.attr.id, _))
      .WillOnce(DoAll(Invoke([](tai_attr_id_t, tai_status_t* return_status) {
                        *return_status = TAI_STATUS_SUCCESS;
                      }),
                      Return(dummy_tai_attribute)));

  EXPECT_CALL(*wrapper,
              GetObject(TaiPath({{TAI_OBJECT_TYPE_MODULE, 0},
                                 {TAI_OBJECT_TYPE_NETWORKIF, 1}})))
      .Times(1)
      .WillOnce(Return(object_mock));

  auto valueOrStatus = manager->GetValue<int32>(
      TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT, {0, 1});
  EXPECT_TRUE(valueOrStatus.ok());

  EXPECT_EQ(valueOrStatus.ConsumeValueOrDie(),
            TypesConverter::ModulationToOperationalMode(kModulation));
}

TEST_F(TaiManagerTest, SetOutputPowerValueWithSuccess_Test) {
  std::shared_ptr<TaiObjectMock> object_mock =
      std::make_shared<TaiObjectMock>();

  tai_attr_metadata_t dummy_tai_metadata = {};

  EXPECT_CALL(*object_mock.get(), SetAttribute(_))
      .Times(1)
      .WillOnce(Return(TAI_STATUS_SUCCESS));

  EXPECT_CALL(*object_mock.get(), GetAlocatedAttributeObject(
                                      TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER))
      .Times(1)
      .WillOnce(Return(TaiAttribute(TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER,
                                    &dummy_tai_metadata)));

  EXPECT_CALL(*wrapper,
              GetObject(TaiPath({{TAI_OBJECT_TYPE_MODULE, 0},
                                 {TAI_OBJECT_TYPE_NETWORKIF, 1}})))
      .Times(1)
      .WillOnce(Return(object_mock));

  auto kStatus = manager->SetValue<float>(
      12.34f, TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER, {0, 1});
  EXPECT_TRUE(kStatus.ok());
}

TEST_F(TaiManagerTest, SetOutputPowerValueWithErrorFromTai_Test) {
  std::shared_ptr<TaiObjectMock> object_mock =
      std::make_shared<TaiObjectMock>();

  tai_attr_metadata_t dummy_tai_metadata = {};

  EXPECT_CALL(*object_mock.get(), SetAttribute(_))
      .Times(1)
      .WillOnce(Return(TAI_STATUS_FAILURE));  // error code from tai

  EXPECT_CALL(*object_mock.get(), GetAlocatedAttributeObject(
                                      TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER))
      .Times(1)
      .WillOnce(Return(TaiAttribute(TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER,
                                    &dummy_tai_metadata)));

  EXPECT_CALL(*wrapper,
              GetObject(TaiPath({{TAI_OBJECT_TYPE_MODULE, 0},
                                 {TAI_OBJECT_TYPE_NETWORKIF, 1}})))
      .Times(1)
      .WillOnce(Return(object_mock));

  auto kStatus = manager->SetValue<float>(
      12.34f, TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER, {0, 1});
  EXPECT_FALSE(kStatus.ok());
}

TEST_F(TaiManagerTest, GetOutputPowerValueWithSuccess_Test) {
  std::shared_ptr<TaiObjectMock> object_mock =
      std::make_shared<TaiObjectMock>();

  tai_attr_metadata_t dummy_tai_metadata = {.objecttype =
                                                TAI_OBJECT_TYPE_NETWORKIF};
  dummy_tai_metadata.attrvaluetype = TAI_ATTR_VALUE_TYPE_FLT;
  TaiAttribute dummy_tai_attribute(TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER,
                                   &dummy_tai_metadata);

  const float kOutputValue = 5.5f;
  dummy_tai_attribute.attr.value.flt = kOutputValue;

  EXPECT_CALL(*object_mock.get(), GetAttribute(dummy_tai_attribute.attr.id, _))
      .WillOnce(DoAll(Invoke([](tai_attr_id_t, tai_status_t* return_status) {
                        *return_status = TAI_STATUS_SUCCESS;
                      }),
                      Return(dummy_tai_attribute)));

  EXPECT_CALL(*wrapper,
              GetObject(TaiPath({{TAI_OBJECT_TYPE_MODULE, 0},
                                 {TAI_OBJECT_TYPE_NETWORKIF, 1}})))
      .Times(1)
      .WillOnce(Return(object_mock));

  auto valueOrStatus = manager->GetValue<float>(
      TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER, {0, 1});
  ASSERT_TRUE(valueOrStatus.ok());

  EXPECT_EQ(valueOrStatus.ConsumeValueOrDie(), kOutputValue);
}

TEST_F(TaiManagerTest, GetInputPowerValueWithSuccess_Test) {
  std::shared_ptr<TaiObjectMock> object_mock =
      std::make_shared<TaiObjectMock>();

  tai_attr_metadata_t dummy_tai_metadata = {.objecttype =
                                                TAI_OBJECT_TYPE_NETWORKIF};
  dummy_tai_metadata.attrvaluetype = TAI_ATTR_VALUE_TYPE_FLT;
  TaiAttribute dummy_tai_attribute(
      TAI_NETWORK_INTERFACE_ATTR_CURRENT_INPUT_POWER, &dummy_tai_metadata);

  const float kInputValue = 5.5f;
  dummy_tai_attribute.attr.value.flt = kInputValue;

  EXPECT_CALL(*object_mock.get(), GetAttribute(dummy_tai_attribute.attr.id, _))
      .WillOnce(DoAll(Invoke([](tai_attr_id_t, tai_status_t* return_status) {
                        *return_status = TAI_STATUS_SUCCESS;
                      }),
                      Return(dummy_tai_attribute)));

  EXPECT_CALL(*wrapper,
              GetObject(TaiPath({{TAI_OBJECT_TYPE_MODULE, 0},
                                 {TAI_OBJECT_TYPE_NETWORKIF, 1}})))
      .Times(1)
      .WillOnce(Return(object_mock));

  auto valueOrStatus = manager->GetValue<float>(
      TAI_NETWORK_INTERFACE_ATTR_CURRENT_INPUT_POWER, {0, 1});
  // in stub realization there is no default value for input power
  ASSERT_TRUE(valueOrStatus.ok());

  EXPECT_EQ(valueOrStatus.ConsumeValueOrDie(), kInputValue);
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
