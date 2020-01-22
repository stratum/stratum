/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "tai.h"

#include "stratum/hal/lib/tai/host_interface.h"
#include "stratum/hal/lib/tai/module.h"
#include "stratum/hal/lib/tai/network_interface.h"
#include "stratum/hal/lib/tai/tai_wrapper.h"
#include "stratum/hal/lib/tai/tai_manager.h"

namespace stratum {
namespace hal {
namespace tai {

class TAIManagerTest : public ::testing::Test {
 protected:
  const std::pair<std::size_t, std::size_t> module_netif_pair{0, 0};
};

TEST_F(TAIManagerTest, CorrectObjectCreation_Test) {
  EXPECT_TRUE(
      TAIManager::Instance()->IsObjectValid({{TAI_OBJECT_TYPE_MODULE, 0}}));
  EXPECT_TRUE(TAIManager::Instance()->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 0}, {TAI_OBJECT_TYPE_HOSTIF, 0}}));
  EXPECT_TRUE(TAIManager::Instance()->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 0}, {TAI_OBJECT_TYPE_HOSTIF, 1}}));
  EXPECT_TRUE(TAIManager::Instance()->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 0}, {TAI_OBJECT_TYPE_NETWORKIF, 0}}));

  EXPECT_TRUE(
      TAIManager::Instance()->IsObjectValid({{TAI_OBJECT_TYPE_MODULE, 1}}));
  EXPECT_TRUE(TAIManager::Instance()->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 1}, {TAI_OBJECT_TYPE_HOSTIF, 0}}));
  EXPECT_TRUE(TAIManager::Instance()->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 1}, {TAI_OBJECT_TYPE_HOSTIF, 1}}));
  EXPECT_TRUE(TAIManager::Instance()->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 1}, {TAI_OBJECT_TYPE_NETWORKIF, 0}}));

  EXPECT_TRUE(
      TAIManager::Instance()->IsObjectValid({{TAI_OBJECT_TYPE_MODULE, 2}}));
  EXPECT_TRUE(TAIManager::Instance()->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 2}, {TAI_OBJECT_TYPE_HOSTIF, 0}}));
  EXPECT_TRUE(TAIManager::Instance()->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 2}, {TAI_OBJECT_TYPE_HOSTIF, 1}}));
  EXPECT_TRUE(TAIManager::Instance()->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 2}, {TAI_OBJECT_TYPE_NETWORKIF, 0}}));

  EXPECT_TRUE(
      TAIManager::Instance()->IsObjectValid({{TAI_OBJECT_TYPE_MODULE, 3}}));
  EXPECT_TRUE(TAIManager::Instance()->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 3}, {TAI_OBJECT_TYPE_HOSTIF, 0}}));
  EXPECT_TRUE(TAIManager::Instance()->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 3}, {TAI_OBJECT_TYPE_HOSTIF, 1}}));
  EXPECT_TRUE(TAIManager::Instance()->IsObjectValid(
      {{TAI_OBJECT_TYPE_MODULE, 3}, {TAI_OBJECT_TYPE_NETWORKIF, 0}}));
}

TEST_F(TAIManagerTest, GetSetFrequencyValue_Test) {
  const uint64 kFrequency = 45;
  SetRequest_Request set_request;
  set_request.mutable_port()->mutable_frequency()->set_value(kFrequency);
  EXPECT_TRUE(TAIManager::IsRequestSupported(set_request));

  auto kStatus =
      TAIManager::Instance()->SetValue(set_request, module_netif_pair);
  EXPECT_TRUE(kStatus.ok());

  DataRequest::Request request(DataRequest::Request::default_instance());

  request.mutable_frequency();
  auto valueOrStatus =
      TAIManager::Instance()->GetValue(request, module_netif_pair);
  EXPECT_TRUE(valueOrStatus.ok());
  EXPECT_EQ(valueOrStatus.ConsumeValueOrDie().mutable_frequency()->value(),
            kFrequency);
}

TEST_F(TAIManagerTest, GetSetOutputPowerValue_Test) {
  SetRequest_Request set_request;
  const float kValue = 12.34;
  set_request.clear_port();
  set_request.mutable_port()->mutable_output_power()->set_instant(kValue);

  EXPECT_TRUE(TAIManager::IsRequestSupported(set_request));

  auto kStatus =
      TAIManager::Instance()->SetValue(set_request, module_netif_pair);
  EXPECT_TRUE(kStatus.ok());

  // set get the value that was previously setted
  DataRequest::Request request(DataRequest::Request::default_instance());

  request.mutable_output_power();
  auto valueOrStatus =
      TAIManager::Instance()->GetValue(request, module_netif_pair);
  EXPECT_TRUE(valueOrStatus.ok());
}

TEST_F(TAIManagerTest, GetInputPowerValue_Test) {
  DataRequest::Request request(DataRequest::Request::default_instance());

  request.mutable_input_power();
  auto valueOrStatus =
      TAIManager::Instance()->GetValue(request, module_netif_pair);
  // in stub realization there is no default value for input power
  EXPECT_FALSE(valueOrStatus.ok());
}

}  // namespace tai
}  // namespace hal
}  // namespace stratum
