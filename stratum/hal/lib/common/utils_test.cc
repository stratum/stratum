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


#include "stratum/hal/lib/common/utils.h"

#include <string>

#include "stratum/lib/constants.h"
#include "gtest/gtest.h"
#include "absl/strings/substitute.h"


using google::protobuf::util::MessageDifferencer;

namespace stratum {
namespace hal {

TEST(CommonUtilsTest, PrintNodeForEmptyNodeProto) {
  Node node;
  EXPECT_EQ("(slot: 0)", PrintNode(node));
}

TEST(CommonUtilsTest, PrintNodeForNoneEmptyNodeProto) {
  Node node;
  node.set_name("test");
  node.set_slot(1);
  EXPECT_EQ("(slot: 1)", PrintNode(node));

  node.set_index(2);
  EXPECT_EQ("(slot: 1, index: 2)", PrintNode(node));

  node.set_id(1234567);
  EXPECT_EQ("(id: 1234567, slot: 1, index: 2)", PrintNode(node));
}

TEST(CommonUtilsTest, PrintNodeWithDirectArgList) {
  EXPECT_EQ("(slot: 1, index: 2)", PrintNode(0, 1, 2));
  EXPECT_EQ("(id: 1234567, slot: 1, index: 2)", PrintNode(1234567, 1, 2));
}

TEST(CommonUtilsTest, PrintSingletonPortForEmptySingletonPortProto) {
  SingletonPort port;
  EXPECT_EQ("(slot: 0, port: 0)", PrintSingletonPort(port));
}

TEST(CommonUtilsTest, PrintSingletonPortForNonEmptySingletonPortProto) {
  SingletonPort port;
  port.set_slot(1);
  EXPECT_EQ("(slot: 1, port: 0)", PrintSingletonPort(port));

  port.set_port(10);
  EXPECT_EQ("(slot: 1, port: 10)", PrintSingletonPort(port));

  port.set_channel(3);
  EXPECT_EQ("(slot: 1, port: 10, channel: 3)", PrintSingletonPort(port));

  port.set_speed_bps(kFiftyGigBps);
  EXPECT_EQ("(slot: 1, port: 10, channel: 3, speed: 50G)",
            PrintSingletonPort(port));

  port.clear_channel();
  EXPECT_EQ("(slot: 1, port: 10, speed: 50G)", PrintSingletonPort(port));

  port.set_id(1234567);
  EXPECT_EQ("(id: 1234567, slot: 1, port: 10, speed: 50G)",
            PrintSingletonPort(port));

  port.clear_speed_bps();
  EXPECT_EQ("(id: 1234567, slot: 1, port: 10)", PrintSingletonPort(port));

  port.set_channel(2);
  EXPECT_EQ("(id: 1234567, slot: 1, port: 10, channel: 2)",
            PrintSingletonPort(port));
}

TEST(CommonUtilsTest, PrintSingletonPortWithDirectArgList) {
  EXPECT_EQ("(slot: 1, port: 10, channel: 3)",
            PrintSingletonPort(0, 1, 10, 3, 0));
  EXPECT_EQ("(slot: 1, port: 10, channel: 3, speed: 50G)",
            PrintSingletonPort(0, 1, 10, 3, kFiftyGigBps));
  EXPECT_EQ("(id: 1234567, slot: 1, port: 10, channel: 3, speed: 50G)",
            PrintSingletonPort(1234567, 1, 10, 3, kFiftyGigBps));
  EXPECT_EQ("(id: 1234567, slot: 1, port: 10)",
            PrintSingletonPort(1234567, 1, 10, 0, 0));
}

TEST(CommonUtilsTest, PrintPortState) {
  EXPECT_EQ("UP", PrintPortState(PORT_STATE_UP));
  EXPECT_EQ("DOWN", PrintPortState(PORT_STATE_DOWN));
  EXPECT_EQ("FAILED", PrintPortState(PORT_STATE_FAILED));
  EXPECT_EQ("UNKNOWN", PrintPortState(PORT_STATE_UNKNOWN));
}

TEST(CommonUtilsTest, PrintPhysicalPort) {
  constexpr int kSlot = 1;
  constexpr int kPortNum = 14;
  PhysicalPort port;

  std::string expected_string(
      absl::Substitute("(slot: $0, port: $1)", kSlot, kPortNum));
  port.set_slot(kSlot);
  port.set_port(kPortNum);
  EXPECT_EQ(expected_string, PrintPhysicalPort(port));
}

TEST(PortUtilsTest, BuildPhysicalPort) {
  constexpr int kSlot = 1;
  constexpr int kPortNum = 23;

  PhysicalPort expected_port;
  expected_port.set_slot(kSlot);
  expected_port.set_port(kPortNum);

  EXPECT_TRUE(MessageDifferencer::Equals(
      expected_port, PortUtils::BuildPhysicalPort(kSlot, kPortNum)));
}

TEST(PortUtilsTest, BuildSingletonPort) {
  constexpr int kSlot = 1;
  constexpr int kPortNum = 23;
  constexpr int kChannel = 4;
  constexpr uint64 kSpeedBps = kTwentyFiveGigBps;

  SingletonPort expected_port;
  expected_port.set_slot(kSlot);
  expected_port.set_port(kPortNum);
  expected_port.set_channel(kChannel);
  expected_port.set_speed_bps(kSpeedBps);

  EXPECT_TRUE(MessageDifferencer::Equals(
      expected_port,
      PortUtils::BuildSingletonPort(kSlot, kPortNum, kChannel, kSpeedBps)));
}

// Note: Assumes PortUtils::BuildSingletonPort() returns a correct port,
// avoids defining a similar method to construct a SingletonPort.
// Constructs a few variations of SingletonPort and checks for equality.
TEST(PortUtilsTest, SingletonPortEqual) {
  SingletonPortEqual port_equals;
  constexpr int kSlot1 = 1;
  constexpr int kSlot2 = 2;
  constexpr int kPortNum1 = 15;
  constexpr int kPortNum2 = 42;
  constexpr int kChannel1 = 3;
  constexpr int kChannel2 = 1;
  constexpr uint64 kSpeedBps1 = kTwentyGigBps;
  constexpr uint64 kSpeedBps2 = kTwentyFiveGigBps;
  constexpr uint64 kSpeedBps3 = kFortyGigBps;
  constexpr uint64 kSpeedBps4 = kHundredGigBps;

  SingletonPort port_1 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, 0, 0ULL);
  SingletonPort port_2 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum2, 0, 0ULL);
  SingletonPort port_3 =
      PortUtils::BuildSingletonPort(kSlot2, kPortNum1, 0, 0ULL);
  EXPECT_FALSE(port_equals(port_1, port_2));
  EXPECT_FALSE(port_equals(port_2, port_3));
  EXPECT_FALSE(port_equals(port_1, port_3));

  SingletonPort port_4 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, 0, 0ULL);
  EXPECT_TRUE(port_equals(port_4, port_1));

  SingletonPort port_5 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, kChannel1, 0ULL);
  EXPECT_FALSE(port_equals(port_5, port_4));

  SingletonPort port_6 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, kChannel2, 0ULL);
  EXPECT_FALSE(port_equals(port_5, port_6));

  SingletonPort port_7 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, kChannel1, 0ULL);
  EXPECT_TRUE(port_equals(port_7, port_5));

  SingletonPort port_8 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, 0, kSpeedBps3);
  SingletonPort port_9 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, 0, kSpeedBps4);
  EXPECT_FALSE(port_equals(port_8, port_9));
  EXPECT_FALSE(port_equals(port_8, port_1));
  EXPECT_FALSE(port_equals(port_8, port_5));

  SingletonPort port_10 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, 0, kSpeedBps3);
  EXPECT_TRUE(port_equals(port_8, port_10));

  SingletonPort port_11 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, kChannel1, kSpeedBps1);
  SingletonPort port_12 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, kChannel1, kSpeedBps2);
  EXPECT_FALSE(port_equals(port_11, port_12));
  EXPECT_FALSE(port_equals(port_11, port_7));
  EXPECT_FALSE(port_equals(port_11, port_4));

  SingletonPort port_13 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, kChannel2, kSpeedBps2);
  EXPECT_FALSE(port_equals(port_13, port_12));

  SingletonPort port_14 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum2, kChannel1, kSpeedBps2);
  EXPECT_FALSE(port_equals(port_14, port_12));

  SingletonPort port_15 =
      PortUtils::BuildSingletonPort(kSlot2, kPortNum1, kChannel1, kSpeedBps2);
  EXPECT_FALSE(port_equals(port_15, port_12));
}

// Note: Assumes PortUtils::BuildSingletonPort() returns a correct port.
// Constructs a few representative instances of SingletonPort and checks if
// the compare function returns the expected order (compares slot, port,
// channel, speed, in that order).
TEST(PortUtilsTest, SingletonPortCompare) {
  SingletonPortCompare port_lesser;
  constexpr int kSlot1 = 1;
  constexpr int kSlot2 = 2;
  constexpr int kPortNum1 = 15;
  constexpr int kPortNum2 = 42;
  constexpr int kChannel1 = 1;
  constexpr int kChannel2 = 3;
  constexpr uint64 kSpeedBps1 = kTwentyGigBps;
  constexpr uint64 kSpeedBps2 = kTwentyFiveGigBps;
  constexpr uint64 kSpeedBps3 = kFortyGigBps;
  constexpr uint64 kSpeedBps4 = kHundredGigBps;

  SingletonPort port_1 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, 0, 0ULL);
  SingletonPort port_2 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum2, 0, 0ULL);
  SingletonPort port_3 =
      PortUtils::BuildSingletonPort(kSlot2, kPortNum1, 0, 0ULL);
  EXPECT_TRUE(port_lesser(port_1, port_2));
  EXPECT_TRUE(port_lesser(port_2, port_3));
  EXPECT_TRUE(port_lesser(port_1, port_3));

  SingletonPort port_4 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, 0, 0ULL);
  EXPECT_FALSE(port_lesser(port_4, port_1));
  EXPECT_FALSE(port_lesser(port_1, port_4));

  SingletonPort port_5 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, kChannel1, 0ULL);
  EXPECT_TRUE(port_lesser(port_4, port_5));
  EXPECT_FALSE(port_lesser(port_5, port_4));

  SingletonPort port_6 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, kChannel2, 0ULL);
  EXPECT_TRUE(port_lesser(port_5, port_6));
  EXPECT_FALSE(port_lesser(port_6, port_5));

  SingletonPort port_7 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum2, kChannel1, 0ULL);
  EXPECT_TRUE(port_lesser(port_6, port_7));
  EXPECT_FALSE(port_lesser(port_7, port_6));

  SingletonPort port_8 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, 0, kSpeedBps3);
  SingletonPort port_9 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, 0, kSpeedBps4);
  EXPECT_TRUE(port_lesser(port_8, port_9));
  EXPECT_TRUE(port_lesser(port_1, port_8));
  EXPECT_TRUE(port_lesser(port_8, port_5));

  // Skip port_10.

  SingletonPort port_11 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, kChannel1, kSpeedBps1);
  SingletonPort port_12 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, kChannel1, kSpeedBps2);
  EXPECT_TRUE(port_lesser(port_11, port_12));
  EXPECT_TRUE(port_lesser(port_4, port_11));
  EXPECT_TRUE(port_lesser(port_5, port_11));

  SingletonPort port_13 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum1, kChannel2, kSpeedBps1);
  EXPECT_TRUE(port_lesser(port_12, port_13));
  EXPECT_FALSE(port_lesser(port_13, port_12));

  SingletonPort port_14 =
      PortUtils::BuildSingletonPort(kSlot1, kPortNum2, kChannel1, kSpeedBps1);
  EXPECT_TRUE(port_lesser(port_12, port_14));
  EXPECT_FALSE(port_lesser(port_14, port_12));

  SingletonPort port_15 =
      PortUtils::BuildSingletonPort(kSlot2, kPortNum1, kChannel1, kSpeedBps1);
  EXPECT_TRUE(port_lesser(port_12, port_15));
  EXPECT_FALSE(port_lesser(port_15, port_12));
}

// Note: Assumes PortUtils::BuildPhysicalPort() returns a correct port.
// Constructs a few variations of PhysicalPort and checks for equality.
TEST(PortUtilsTest, PhysicalPortEqual) {
  PhysicalPortEqual port_equals;
  constexpr int kSlot1 = 1;
  constexpr int kSlot2 = 2;
  constexpr int kPortNum1 = 15;
  constexpr int kPortNum2 = 42;

  PhysicalPort port_1 = PortUtils::BuildPhysicalPort(kSlot1, kPortNum1);
  PhysicalPort port_2 = PortUtils::BuildPhysicalPort(kSlot1, kPortNum2);
  PhysicalPort port_3 = PortUtils::BuildPhysicalPort(kSlot2, kPortNum1);
  EXPECT_FALSE(port_equals(port_1, port_2));
  EXPECT_FALSE(port_equals(port_2, port_3));
  EXPECT_FALSE(port_equals(port_1, port_3));

  PhysicalPort port_4 = PortUtils::BuildPhysicalPort(kSlot2, kPortNum1);
  EXPECT_TRUE(port_equals(port_3, port_4));
}

// Note: Assumes PortUtils::BuildPhysicalPort() returns a correct port.
// Constructs a few representative instances of PhysicalPort and checks if
// the compare function returns the expected order (compares slot, port,
// in that order).
TEST(PortUtilsTest, PhysicalPortCompare) {
  PhysicalPortCompare port_lesser;
  constexpr int kSlot1 = 1;
  constexpr int kSlot2 = 2;
  constexpr int kPortNum1 = 15;
  constexpr int kPortNum2 = 42;

  PhysicalPort port_1 = PortUtils::BuildPhysicalPort(kSlot1, kPortNum1);
  PhysicalPort port_2 = PortUtils::BuildPhysicalPort(kSlot1, kPortNum2);
  PhysicalPort port_3 = PortUtils::BuildPhysicalPort(kSlot2, kPortNum1);
  EXPECT_TRUE(port_lesser(port_1, port_2));
  EXPECT_FALSE(port_lesser(port_2, port_1));
  EXPECT_TRUE(port_lesser(port_2, port_3));
  EXPECT_FALSE(port_lesser(port_3, port_2));
  EXPECT_TRUE(port_lesser(port_1, port_3));
  EXPECT_FALSE(port_lesser(port_3, port_1));

  PhysicalPort port_4 = PortUtils::BuildPhysicalPort(kSlot2, kPortNum1);
  EXPECT_FALSE(port_lesser(port_3, port_4));
  EXPECT_FALSE(port_lesser(port_4, port_3));
}

}  // namespace hal
}  // namespace stratum
