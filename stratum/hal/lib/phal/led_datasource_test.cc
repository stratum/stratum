// Copyright 2018 Google LLC
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


#include "third_party/stratum/hal/lib/phal/led_datasource.h"

#include "third_party/stratum/glue/status/status_test_util.h"
#include "third_party/stratum/hal/lib/phal/phal.pb.h"
#include "third_party/stratum/hal/lib/phal/system_interface_mock.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

namespace stratum {
namespace hal {
namespace phal {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::status::StatusIs;

namespace {
// Helper functions that helps to set LED color and LED state.
const ::util::Status SetColor(LedDataSource* led_datasource,
                                    LedColor led_color) {
  return led_datasource->GetLedColor()->Set(
      LedColor_descriptor()->FindValueByName(LedColor_Name(led_color)));
}
const ::util::Status SetState(LedDataSource* led_datasource,
                                    LedState led_state) {
  return led_datasource->GetLedState()->Set(
      LedState_descriptor()->FindValueByName(LedState_Name(led_state)));
}
}  // namespace

// This test verifies that LEDDatasource can initialize and control Bicolor FPGA
// LED light.
TEST(LEDDatasourceTest, InitializeAndControlBicolorFpgaLedSucceed) {
  hal::LedConfig led_config;
  protobuf::TextFormat::ParseFromString(
      " led_index : 1"
      " led_type : BICOLOR_FPGA_G_R"
      " led_control_path : \"test_control_path_1\""
      " led_control_path : \"test_control_path_2\"",
      &led_config);
  MockSystemInterface mock_system;

  ASSERT_OK_AND_ASSIGN(std::shared_ptr<LedDataSource> led_datasource,
                       LedDataSource::Make(led_config, &mock_system, nullptr));

  // Turns the LED light off.
  std::string string_in_control_path_1;
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  std::string string_in_control_path_2;
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::OFF));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::GREEN));

  EXPECT_EQ(string_in_control_path_1, "1");
  EXPECT_EQ(string_in_control_path_2, "1");

  // Turns the LED light green.
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));

  EXPECT_OK(SetState(led_datasource.get(), LedState::SOLID));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::GREEN));
  EXPECT_EQ(string_in_control_path_1, "0");
  EXPECT_EQ(string_in_control_path_2, "1");

  // Turns the LED light red.
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::SOLID));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::RED));

  EXPECT_EQ(string_in_control_path_1, "1");
  EXPECT_EQ(string_in_control_path_2, "0");
}

TEST(LEDDatasourceTest, MakeLedDataSourceFailInConfiguredControlPath) {
  hal::LedConfig led_config;
  protobuf::TextFormat::ParseFromString(
      " led_index : 1"
      " led_type : BICOLOR_FPGA_G_R"
      " led_control_path : \"test_control_path_1\""
      " led_control_path : \"test_control_path_2\""
      " led_control_path : \"test_control_path_3\"",
      &led_config);
  MockSystemInterface mock_system;
  EXPECT_THAT(LedDataSource::Make(led_config, &mock_system, nullptr),
              StatusIs(_, ERR_INVALID_PARAM,
                       HasSubstr("Control path size mismatch")));
}

TEST(LEDDatasourceTest, MakeLedDataSourceFailInLedType) {
  hal::LedConfig led_config;
  protobuf::TextFormat::ParseFromString(
      " led_index : 1"
      " led_type : UNKNOWN"
      " led_control_path : \"test_control_path_1\""
      " led_control_path : \"test_control_path_2\""
      " led_control_path : \"test_control_path_3\"",
      &led_config);
  MockSystemInterface mock_system;

  // Failed to create led_datasource due to mismatch in number of configured
  // led_control_path.
  EXPECT_THAT(
      LedDataSource::Make(led_config, &mock_system, nullptr),
      StatusIs(_, ERR_INVALID_PARAM, HasSubstr("Fail to initialize LED map")));
}

// This test verifies that LEDDatasource can initialize and control tricolor
// FPGA LED light.
TEST(LEDDatasourceTest, InitializeAndControlTricolorFpgaLedSucceed) {
  hal::LedConfig led_config;
  protobuf::TextFormat::ParseFromString(
      " led_index : 1"
      " led_type : TRICOLOR_FPGA_GR_GY"
      " led_control_path : \"test_control_path_1\""
      " led_control_path : \"test_control_path_2\""
      " led_control_path : \"test_control_path_3\"",
      &led_config);
  MockSystemInterface mock_system;

  // Failed to create led_datasource due to mismatch in size of configured
  // led_control_path.
  EXPECT_THAT(LedDataSource::Make(led_config, &mock_system, nullptr),
              StatusIs(_, ERR_INVALID_PARAM,
                       HasSubstr("Control path size mismatch")));

  // Correct the config and try again.
  protobuf::TextFormat::ParseFromString(
      " led_index : 1"
      " led_type : TRICOLOR_FPGA_GR_GY"
      " led_control_path : \"test_control_path_1\""
      " led_control_path : \"test_control_path_2\""
      " led_control_path : \"test_control_path_3\""
      " led_control_path : \"test_control_path_4\"",
      &led_config);

  ASSERT_OK_AND_ASSIGN(std::shared_ptr<LedDataSource> led_datasource,
                       LedDataSource::Make(led_config, &mock_system, nullptr));

  // Turns the LED light off.
  std::string string_in_control_path_1;
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  std::string string_in_control_path_2;
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  std::string string_in_control_path_3;
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_3"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_3),
                      Return(::util::OkStatus())));
  std::string string_in_control_path_4;
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_4"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_4),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::OFF));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::GREEN));

  EXPECT_EQ(string_in_control_path_1, "1");
  EXPECT_EQ(string_in_control_path_2, "1");
  EXPECT_EQ(string_in_control_path_3, "1");
  EXPECT_EQ(string_in_control_path_4, "1");

  // Turns the LED light green.
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_3"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_3),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_4"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_4),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::SOLID));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::GREEN));

  EXPECT_EQ(string_in_control_path_1, "0");
  EXPECT_EQ(string_in_control_path_2, "1");
  EXPECT_EQ(string_in_control_path_3, "1");
  EXPECT_EQ(string_in_control_path_4, "1");

  // Turns the LED light red.
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_3"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_3),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_4"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_4),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::SOLID));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::RED));

  EXPECT_EQ(string_in_control_path_1, "1");
  EXPECT_EQ(string_in_control_path_2, "0");
  EXPECT_EQ(string_in_control_path_3, "1");
  EXPECT_EQ(string_in_control_path_4, "1");

  // Turns the LED light amber.
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_3"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_3),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_4"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_4),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::SOLID));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::AMBER));

  EXPECT_EQ(string_in_control_path_1, "1");
  EXPECT_EQ(string_in_control_path_2, "1");
  EXPECT_EQ(string_in_control_path_3, "1");
  EXPECT_EQ(string_in_control_path_4, "0");
}

// This test verifies that LEDDatasource can initialize and control tricolor
// FPGA LED light.
TEST(LEDDatasourceTest,
     InitializeAndControlTricolorThreeControlFpgaLedSucceed) {
  hal::LedConfig led_config;
  protobuf::TextFormat::ParseFromString(
      " led_index : 1"
      " led_type : TRICOLOR_FPGA_G_R_Y"
      " led_control_path : \"test_control_path_1\""
      " led_control_path : \"test_control_path_2\""
      " led_control_path : \"test_control_path_2\""
      " led_control_path : \"test_control_path_3\"",
      &led_config);
  MockSystemInterface mock_system;
  // Failed to create led_datasource due to mismatch in size of configured
  // led_control_path.
  EXPECT_THAT(LedDataSource::Make(led_config, &mock_system, nullptr),
              StatusIs(_, ERR_INVALID_PARAM,
                       HasSubstr("Control path size mismatch")));
  // Correct the config and try again.
  protobuf::TextFormat::ParseFromString(
      " led_index : 1"
      " led_type : TRICOLOR_FPGA_G_R_Y"
      " led_control_path : \"test_control_path_1\""
      " led_control_path : \"test_control_path_2\""
      " led_control_path : \"test_control_path_3\"",
      &led_config);
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<LedDataSource> led_datasource,
                       LedDataSource::Make(led_config, &mock_system, nullptr));

  // Turns the LED light off.
  std::string string_in_control_path_1;
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  std::string string_in_control_path_2;
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  std::string string_in_control_path_3;
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_3"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_3),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::OFF));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::GREEN));

  EXPECT_EQ(string_in_control_path_1, "1");
  EXPECT_EQ(string_in_control_path_2, "1");
  EXPECT_EQ(string_in_control_path_3, "1");

  // Turns the LED light green.
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_3"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_3),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::SOLID));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::GREEN));

  EXPECT_EQ(string_in_control_path_1, "0");
  EXPECT_EQ(string_in_control_path_2, "1");
  EXPECT_EQ(string_in_control_path_3, "1");

  // Turns the LED light red.
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_3"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_3),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::SOLID));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::RED));

  EXPECT_EQ(string_in_control_path_1, "1");
  EXPECT_EQ(string_in_control_path_2, "0");
  EXPECT_EQ(string_in_control_path_3, "1");

  // Turns the LED light amber.
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_3"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_3),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::SOLID));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::AMBER));

  EXPECT_EQ(string_in_control_path_1, "1");
  EXPECT_EQ(string_in_control_path_2, "1");
  EXPECT_EQ(string_in_control_path_3, "0");
}

// This test verifies that LEDDatasource can initialize and control bicolor LED
// light LED light.
TEST(LEDDatasourceTest, InitializeAndControlForBicolorGPIOLedSucceed) {
  hal::LedConfig led_config;
  protobuf::TextFormat::ParseFromString(
      " led_index : 1"
      " led_type : BICOLOR_GPIO_G_R"
      " led_control_path : \"test_control_path_1\""
      " led_control_path : \"test_control_path_2\""
      " led_control_path : \"test_control_path_2\""
      " led_control_path : \"test_control_path_3\"",
      &led_config);
  MockSystemInterface mock_system;
  // Failed to create led_datasource due to mismatch in size of configured
  // led_control_path.
  EXPECT_THAT(LedDataSource::Make(led_config, &mock_system, nullptr),
              StatusIs(_, ERR_INVALID_PARAM,
                       HasSubstr("Control path size mismatch")));
  // Correct the config and try again.
  protobuf::TextFormat::ParseFromString(
      " led_index : 1"
      " led_type : BICOLOR_GPIO_G_R"
      " led_control_path : \"test_control_path_1\""
      " led_control_path : \"test_control_path_2\"",
      &led_config);

  ASSERT_OK_AND_ASSIGN(std::shared_ptr<LedDataSource> led_datasource,
                       LedDataSource::Make(led_config, &mock_system, nullptr));

  // Turns the LED light off.
  std::string string_in_control_path_1;
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  std::string string_in_control_path_2;
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::OFF));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::GREEN));

  EXPECT_EQ(string_in_control_path_1, "0");
  EXPECT_EQ(string_in_control_path_2, "0");

  // Turns the LED light green.
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::SOLID));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::GREEN));

  EXPECT_EQ(string_in_control_path_1, "1");
  EXPECT_EQ(string_in_control_path_2, "0");

  // Turns the LED light red.
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_1"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_1),
                      Return(::util::OkStatus())));
  EXPECT_CALL(mock_system, WriteStringToFile(_, "test_control_path_2"))
      .WillOnce(DoAll(SaveArg<0>(&string_in_control_path_2),
                      Return(::util::OkStatus())));
  EXPECT_OK(SetState(led_datasource.get(), LedState::SOLID));
  EXPECT_OK(SetColor(led_datasource.get(), LedColor::RED));

  EXPECT_EQ(string_in_control_path_1, "0");
  EXPECT_EQ(string_in_control_path_2, "1");
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
