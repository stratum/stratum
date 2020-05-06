// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/hal/lib/common/utils.h"

#include <string>
#include <limits>

#include "stratum/lib/constants.h"
#include "stratum/glue/status/canonical_errors.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/substitute.h"
#include "google/protobuf/util/message_differencer.h"


using ::google::protobuf::util::MessageDifferencer;

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

TEST(CommonUtilsTest, PrintNodeProperties) {
  EXPECT_EQ("(slot: 1, index: 2)", PrintNodeProperties(0, 1, 2));
  EXPECT_EQ("(id: 1234567, slot: 1, index: 2)",
            PrintNodeProperties(1234567, 1, 2));
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
  EXPECT_EQ("(port_id: 1234567, slot: 1, port: 10, speed: 50G)",
            PrintSingletonPort(port));

  port.clear_speed_bps();
  EXPECT_EQ("(port_id: 1234567, slot: 1, port: 10)", PrintSingletonPort(port));

  port.set_channel(2);
  EXPECT_EQ("(port_id: 1234567, slot: 1, port: 10, channel: 2)",
            PrintSingletonPort(port));

  port.set_node(2);
  EXPECT_EQ("(node_id: 2, port_id: 1234567, slot: 1, port: 10, channel: 2)",
            PrintSingletonPort(port));
}

TEST(CommonUtilsTest, PrintPortProperties) {
  EXPECT_EQ("(slot: 1, port: 10, channel: 3)",
            PrintPortProperties(0, 0, 1, 10, 3, -1, -1, 0));
  EXPECT_EQ("(slot: 1, port: 10, channel: 3, speed: 50G)",
            PrintPortProperties(0, 0, 1, 10, 3, -1, -1, kFiftyGigBps));
  EXPECT_EQ("(port_id: 1234567, slot: 1, port: 10, channel: 3, speed: 50G)",
            PrintPortProperties(0, 1234567, 1, 10, 3, -1, -1, kFiftyGigBps));
  EXPECT_EQ("(port_id: 1234567, slot: 1, port: 10)",
            PrintPortProperties(0, 1234567, 1, 10, 0, -1, -1, 0));
  EXPECT_EQ("(node_id: 98765, port_id: 1234567, slot: 1, port: 10)",
            PrintPortProperties(98765, 1234567, 1, 10, 0, -1, -1, 0));
  EXPECT_EQ("(node_id: 98765, port_id: 1234567, slot: 1, port: 10, unit: 2)",
            PrintPortProperties(98765, 1234567, 1, 10, 0, 2, -1, 0));
  EXPECT_EQ(
      "(node_id: 98765, port_id: 1234567, slot: 1, port: 10, unit: 2, "
      "logical_port: 33)",
      PrintPortProperties(98765, 1234567, 1, 10, 0, 2, 33, 0));
  EXPECT_EQ(
      "(node_id: 98765, port_id: 1234567, slot: 1, port: 10, unit: 2, "
      "logical_port: 33, speed: 40G)",
      PrintPortProperties(98765, 1234567, 1, 10, 0, 2, 33, kFortyGigBps));
}

TEST(CommonUtilsTest, PrintTrunkPortForEmptyTrunkPortProto) {
  TrunkPort port;
  EXPECT_EQ("()", PrintTrunkPort(port));
}

TEST(CommonUtilsTest, PrintTrunkPortForNonEmptyTrunkPortProto) {
  TrunkPort port;
  port.set_node(98765);
  EXPECT_EQ("(node_id: 98765)", PrintTrunkPort(port));

  port.set_id(12345);
  EXPECT_EQ("(node_id: 98765, trunk_id: 12345)", PrintTrunkPort(port));
}

TEST(CommonUtilsTest, PrintTrunkProperties) {
  EXPECT_EQ("(node_id: 98765, trunk_id: 1234567, unit: 2, trunk_port: 33)",
            PrintTrunkProperties(98765, 1234567, 2, 33, 0));
  EXPECT_EQ(
      "(node_id: 98765, trunk_id: 1234567, unit: 2, trunk_port: 33, speed: "
      "40G)",
      PrintTrunkProperties(98765, 1234567, 2, 33, kFortyGigBps));
}

TEST(CommonUtilsTest, PrintPortState) {
  EXPECT_EQ("UP", PrintPortState(PORT_STATE_UP));
  EXPECT_EQ("DOWN", PrintPortState(PORT_STATE_DOWN));
  EXPECT_EQ("FAILED", PrintPortState(PORT_STATE_FAILED));
  EXPECT_EQ("UNKNOWN", PrintPortState(PORT_STATE_UNKNOWN));
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
      expected_port, BuildSingletonPort(kSlot, kPortNum, kChannel, kSpeedBps)));
}

TEST(PortUtilsTest, SingletonPortHash) {
  absl::flat_hash_set<SingletonPort, SingletonPortHash, SingletonPortEqual>
      test_hash_map;
  SingletonPortHash port_hash;
  constexpr int kSlot1 = 1;
  constexpr int kSlot2 = 2;
  constexpr int kPortNum1 = 15;
  constexpr int kPortNum2 = 42;
  constexpr int kChannel1 = 3;
  constexpr int kChannel2 = 1;
  constexpr uint64 kSpeedBps1 = kTwentyGigBps;
  constexpr uint64 kSpeedBps2 = kTwentyFiveGigBps;

  SingletonPort port_1 =
      BuildSingletonPort(kSlot1, kPortNum1, kChannel1, kSpeedBps1);
  SingletonPort port_2 =
      BuildSingletonPort(kSlot2, kPortNum2, kChannel2, kSpeedBps2);
  SingletonPort port_3 =
      BuildSingletonPort(kSlot2, kPortNum2, kChannel2, kSpeedBps1);
  test_hash_map.insert(port_1);
  test_hash_map.insert(port_2);
  test_hash_map.insert(port_3);
  EXPECT_NE(port_hash(port_1), port_hash(port_2));
  EXPECT_EQ(port_hash(port_2), port_hash(port_2));
  EXPECT_EQ(3U, test_hash_map.size());
}

// Note: Assumes BuildSingletonPort() returns a correct port,
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

  SingletonPort port_1 = BuildSingletonPort(kSlot1, kPortNum1, 0, 0ULL);
  SingletonPort port_2 = BuildSingletonPort(kSlot1, kPortNum2, 0, 0ULL);
  SingletonPort port_3 = BuildSingletonPort(kSlot2, kPortNum1, 0, 0ULL);
  EXPECT_FALSE(port_equals(port_1, port_2));
  EXPECT_FALSE(port_equals(port_2, port_3));
  EXPECT_FALSE(port_equals(port_1, port_3));

  SingletonPort port_4 = BuildSingletonPort(kSlot1, kPortNum1, 0, 0ULL);
  EXPECT_TRUE(port_equals(port_4, port_1));

  SingletonPort port_5 = BuildSingletonPort(kSlot1, kPortNum1, kChannel1, 0ULL);
  EXPECT_FALSE(port_equals(port_5, port_4));

  SingletonPort port_6 = BuildSingletonPort(kSlot1, kPortNum1, kChannel2, 0ULL);
  EXPECT_FALSE(port_equals(port_5, port_6));

  SingletonPort port_7 = BuildSingletonPort(kSlot1, kPortNum1, kChannel1, 0ULL);
  EXPECT_TRUE(port_equals(port_7, port_5));

  SingletonPort port_8 = BuildSingletonPort(kSlot1, kPortNum1, 0, kSpeedBps3);
  SingletonPort port_9 = BuildSingletonPort(kSlot1, kPortNum1, 0, kSpeedBps4);
  EXPECT_FALSE(port_equals(port_8, port_9));
  EXPECT_FALSE(port_equals(port_8, port_1));
  EXPECT_FALSE(port_equals(port_8, port_5));

  SingletonPort port_10 = BuildSingletonPort(kSlot1, kPortNum1, 0, kSpeedBps3);
  EXPECT_TRUE(port_equals(port_8, port_10));

  SingletonPort port_11 =
      BuildSingletonPort(kSlot1, kPortNum1, kChannel1, kSpeedBps1);
  SingletonPort port_12 =
      BuildSingletonPort(kSlot1, kPortNum1, kChannel1, kSpeedBps2);
  EXPECT_FALSE(port_equals(port_11, port_12));
  EXPECT_FALSE(port_equals(port_11, port_7));
  EXPECT_FALSE(port_equals(port_11, port_4));

  SingletonPort port_13 =
      BuildSingletonPort(kSlot1, kPortNum1, kChannel2, kSpeedBps2);
  EXPECT_FALSE(port_equals(port_13, port_12));

  SingletonPort port_14 =
      BuildSingletonPort(kSlot1, kPortNum2, kChannel1, kSpeedBps2);
  EXPECT_FALSE(port_equals(port_14, port_12));

  SingletonPort port_15 =
      BuildSingletonPort(kSlot2, kPortNum1, kChannel1, kSpeedBps2);
  EXPECT_FALSE(port_equals(port_15, port_12));
}

// Note: Assumes BuildSingletonPort() returns a correct port.
// Constructs a few representative instances of SingletonPort and checks if
// the compare function returns the expected order (compares slot, port,
// channel, speed, in that order).
TEST(PortUtilsTest, SingletonPortLess) {
  SingletonPortLess port_less;
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

  SingletonPort port_1 = BuildSingletonPort(kSlot1, kPortNum1, 0, 0ULL);
  SingletonPort port_2 = BuildSingletonPort(kSlot1, kPortNum2, 0, 0ULL);
  SingletonPort port_3 = BuildSingletonPort(kSlot2, kPortNum1, 0, 0ULL);
  EXPECT_TRUE(port_less(port_1, port_2));
  EXPECT_TRUE(port_less(port_2, port_3));
  EXPECT_TRUE(port_less(port_1, port_3));

  SingletonPort port_4 = BuildSingletonPort(kSlot1, kPortNum1, 0, 0ULL);
  EXPECT_FALSE(port_less(port_4, port_1));
  EXPECT_FALSE(port_less(port_1, port_4));

  SingletonPort port_5 = BuildSingletonPort(kSlot1, kPortNum1, kChannel1, 0ULL);
  EXPECT_TRUE(port_less(port_4, port_5));
  EXPECT_FALSE(port_less(port_5, port_4));

  SingletonPort port_6 = BuildSingletonPort(kSlot1, kPortNum1, kChannel2, 0ULL);
  EXPECT_TRUE(port_less(port_5, port_6));
  EXPECT_FALSE(port_less(port_6, port_5));

  SingletonPort port_7 = BuildSingletonPort(kSlot1, kPortNum2, kChannel1, 0ULL);
  EXPECT_TRUE(port_less(port_6, port_7));
  EXPECT_FALSE(port_less(port_7, port_6));

  SingletonPort port_8 = BuildSingletonPort(kSlot1, kPortNum1, 0, kSpeedBps3);
  SingletonPort port_9 = BuildSingletonPort(kSlot1, kPortNum1, 0, kSpeedBps4);
  EXPECT_TRUE(port_less(port_8, port_9));
  EXPECT_TRUE(port_less(port_1, port_8));
  EXPECT_TRUE(port_less(port_8, port_5));

  // Skip port_10.

  SingletonPort port_11 =
      BuildSingletonPort(kSlot1, kPortNum1, kChannel1, kSpeedBps1);
  SingletonPort port_12 =
      BuildSingletonPort(kSlot1, kPortNum1, kChannel1, kSpeedBps2);
  EXPECT_TRUE(port_less(port_11, port_12));
  EXPECT_TRUE(port_less(port_4, port_11));
  EXPECT_TRUE(port_less(port_5, port_11));

  SingletonPort port_13 =
      BuildSingletonPort(kSlot1, kPortNum1, kChannel2, kSpeedBps1);
  EXPECT_TRUE(port_less(port_12, port_13));
  EXPECT_FALSE(port_less(port_13, port_12));

  SingletonPort port_14 =
      BuildSingletonPort(kSlot1, kPortNum2, kChannel1, kSpeedBps1);
  EXPECT_TRUE(port_less(port_12, port_14));
  EXPECT_FALSE(port_less(port_14, port_12));

  SingletonPort port_15 =
      BuildSingletonPort(kSlot2, kPortNum1, kChannel1, kSpeedBps1);
  EXPECT_TRUE(port_less(port_12, port_15));
  EXPECT_FALSE(port_less(port_15, port_12));
}

TEST(PortUtilsTest, FindPortLedColorAndState) {
  EXPECT_EQ(std::make_pair(LED_COLOR_AMBER, LED_STATE_SOLID),
            FindPortLedColorAndState(ADMIN_STATE_DISABLED, PORT_STATE_UNKNOWN,
                                     HEALTH_STATE_UNKNOWN,
                                     TRUNK_MEMBER_BLOCK_STATE_UNKNOWN));
  EXPECT_EQ(std::make_pair(LED_COLOR_GREEN, LED_STATE_OFF),
            FindPortLedColorAndState(ADMIN_STATE_ENABLED, PORT_STATE_UNKNOWN,
                                     HEALTH_STATE_UNKNOWN,
                                     TRUNK_MEMBER_BLOCK_STATE_UNKNOWN));
  EXPECT_EQ(std::make_pair(LED_COLOR_GREEN, LED_STATE_OFF),
            FindPortLedColorAndState(ADMIN_STATE_ENABLED, PORT_STATE_DOWN,
                                     HEALTH_STATE_UNKNOWN,
                                     TRUNK_MEMBER_BLOCK_STATE_UNKNOWN));
  EXPECT_EQ(std::make_pair(LED_COLOR_GREEN, LED_STATE_BLINKING_SLOW),
            FindPortLedColorAndState(ADMIN_STATE_ENABLED, PORT_STATE_UP,
                                     HEALTH_STATE_UNKNOWN,
                                     TRUNK_MEMBER_BLOCK_STATE_BLOCKED));
  EXPECT_EQ(std::make_pair(LED_COLOR_GREEN, LED_STATE_SOLID),
            FindPortLedColorAndState(ADMIN_STATE_ENABLED, PORT_STATE_UP,
                                     HEALTH_STATE_GOOD,
                                     TRUNK_MEMBER_BLOCK_STATE_UNKNOWN));
  EXPECT_EQ(std::make_pair(LED_COLOR_AMBER, LED_STATE_BLINKING_FAST),
            FindPortLedColorAndState(ADMIN_STATE_ENABLED, PORT_STATE_UP,
                                     HEALTH_STATE_BAD,
                                     TRUNK_MEMBER_BLOCK_STATE_UNKNOWN));
  EXPECT_EQ(std::make_pair(LED_COLOR_GREEN, LED_STATE_BLINKING_FAST),
            FindPortLedColorAndState(ADMIN_STATE_ENABLED, PORT_STATE_UP,
                                     HEALTH_STATE_UNKNOWN,
                                     TRUNK_MEMBER_BLOCK_STATE_UNKNOWN));
  EXPECT_EQ(std::make_pair(LED_COLOR_GREEN, LED_STATE_BLINKING_FAST),
            FindPortLedColorAndState(ADMIN_STATE_ENABLED, PORT_STATE_UP,
                                     HEALTH_STATE_UNKNOWN,
                                     TRUNK_MEMBER_BLOCK_STATE_FORWARDING));
}

TEST(PortUtilsTest, AggregatePortLedColorsStatePairs) {
  EXPECT_EQ(std::make_pair(LED_COLOR_UNKNOWN, LED_STATE_UNKNOWN),
            AggregatePortLedColorsStatePairs(
                {std::make_pair(LED_COLOR_UNKNOWN, LED_STATE_UNKNOWN)}));
  EXPECT_EQ(std::make_pair(LED_COLOR_UNKNOWN, LED_STATE_UNKNOWN),
            AggregatePortLedColorsStatePairs({}));
  EXPECT_EQ(std::make_pair(LED_COLOR_AMBER, LED_STATE_SOLID),
            AggregatePortLedColorsStatePairs(
                {std::make_pair(LED_COLOR_AMBER, LED_STATE_SOLID)}));
  EXPECT_EQ(std::make_pair(LED_COLOR_AMBER, LED_STATE_SOLID),
            AggregatePortLedColorsStatePairs(
                {std::make_pair(LED_COLOR_AMBER, LED_STATE_SOLID),
                 std::make_pair(LED_COLOR_UNKNOWN, LED_STATE_UNKNOWN)}));
  EXPECT_EQ(std::make_pair(LED_COLOR_AMBER, LED_STATE_BLINKING_SLOW),
            AggregatePortLedColorsStatePairs(
                {std::make_pair(LED_COLOR_AMBER, LED_STATE_SOLID),
                 std::make_pair(LED_COLOR_AMBER, LED_STATE_BLINKING_SLOW)}));
  EXPECT_EQ(std::make_pair(LED_COLOR_AMBER, LED_STATE_BLINKING_SLOW),
            AggregatePortLedColorsStatePairs(
                {std::make_pair(LED_COLOR_AMBER, LED_STATE_SOLID),
                 std::make_pair(LED_COLOR_UNKNOWN, LED_STATE_UNKNOWN),
                 std::make_pair(LED_COLOR_AMBER, LED_STATE_BLINKING_FAST)}));
  EXPECT_EQ(std::make_pair(LED_COLOR_AMBER, LED_STATE_SOLID),
            AggregatePortLedColorsStatePairs(
                {std::make_pair(LED_COLOR_UNKNOWN, LED_STATE_UNKNOWN),
                 std::make_pair(LED_COLOR_UNKNOWN, LED_STATE_SOLID),
                 std::make_pair(LED_COLOR_UNKNOWN, LED_STATE_UNKNOWN)}));
  EXPECT_EQ(std::make_pair(LED_COLOR_AMBER, LED_STATE_BLINKING_SLOW),
            AggregatePortLedColorsStatePairs(
                {std::make_pair(LED_COLOR_AMBER, LED_STATE_SOLID),
                 std::make_pair(LED_COLOR_AMBER, LED_STATE_BLINKING_FAST)}));
  EXPECT_EQ(std::make_pair(LED_COLOR_GREEN, LED_STATE_SOLID),
            AggregatePortLedColorsStatePairs(
                {std::make_pair(LED_COLOR_GREEN, LED_STATE_SOLID)}));
  EXPECT_EQ(std::make_pair(LED_COLOR_GREEN, LED_STATE_SOLID),
            AggregatePortLedColorsStatePairs(
                {std::make_pair(LED_COLOR_GREEN, LED_STATE_SOLID),
                 std::make_pair(LED_COLOR_GREEN, LED_STATE_SOLID)}));
}

void DoubleToDecimalTest(double from, int64 digits, uint32 precision) {
  auto res = ConvertDoubleToDecimal64(from, precision);
  EXPECT_TRUE(res.ok());
  auto decimal_val = res.ValueOrDie();
  EXPECT_EQ(decimal_val.digits(), digits);
  EXPECT_EQ(decimal_val.precision(), precision);
}

TEST(DecimalUtilTest, TestFromDoubleToDecimal64) {
  DoubleToDecimalTest(123.456, 123ll, 0);
  DoubleToDecimalTest(123.456, 1235ll, 1);  // Mind the round up
  DoubleToDecimalTest(123.456, 12346ll, 2);
  DoubleToDecimalTest(123.456, 123456ll, 3);
  DoubleToDecimalTest(123.456, 1234560ll, 4);

  DoubleToDecimalTest(-123.456, -123ll, 0);
  DoubleToDecimalTest(-123.456, -1235ll, 1);  // Mind the round up
  DoubleToDecimalTest(-123.456, -12346ll, 2);
  DoubleToDecimalTest(-123.456, -123456ll, 3);
  DoubleToDecimalTest(-123.456, -1234560ll, 4);

  // Check zero handling
  DoubleToDecimalTest(0.00, 0ll, 2);
  DoubleToDecimalTest(-0.00, -0ll, 2);
  DoubleToDecimalTest(0.00, 0ll, 1);
  DoubleToDecimalTest(-0.00, 0ll, 1);

  // Check rounding
  DoubleToDecimalTest(0.49, 0, 0);
  DoubleToDecimalTest(0.5, 1, 0);

  // Some edge cases
  ::util::IsOutOfRange(
    ConvertDoubleToDecimal64(std::numeric_limits<double>::max(), 0).status());
  ::util::IsOutOfRange(ConvertDoubleToDecimal64(
    std::numeric_limits<double>::min(), 0).status());
  ::util::IsOutOfRange(ConvertDoubleToDecimal64(
    std::numeric_limits<double>::infinity(), 0).status());
  ::util::IsOutOfRange(ConvertDoubleToDecimal64(
    std::numeric_limits<double>::lowest(), 0).status());
}

void DecimalToDoubleTest(int64 digits, uint32 precision, double to) {
  ::gnmi::Decimal64 from;
  from.set_digits(digits);
  from.set_precision(precision);
  auto res = ConvertDecimal64ToDouble(from);
  EXPECT_TRUE(res.ok());
  EXPECT_DOUBLE_EQ(res.ValueOrDie(), to);
}

TEST(DecimalUtilTest, TestFromDecimal64ToDouble) {
  DecimalToDoubleTest(12345ll, 0, 12345.);
  DecimalToDoubleTest(12345ll, 1, 1234.5);
  DecimalToDoubleTest(12345ll, 2, 123.45);
  DecimalToDoubleTest(12345ll, 3, 12.345);
  DecimalToDoubleTest(12345ll, 4, 1.2345);
  DecimalToDoubleTest(12345ll, 5, .12345);
  DecimalToDoubleTest(12345ll, 6, .012345);

  DecimalToDoubleTest(-12345ll, 0, -12345.);
  DecimalToDoubleTest(-12345ll, 1, -1234.5);
  DecimalToDoubleTest(-12345ll, 2, -123.45);
  DecimalToDoubleTest(-12345ll, 3, -12.345);
  DecimalToDoubleTest(-12345ll, 4, -1.2345);
  DecimalToDoubleTest(-12345ll, 5, -.12345);
  DecimalToDoubleTest(-12345ll, 6, -.012345);

  DecimalToDoubleTest(0ll, 0, 0);
  DecimalToDoubleTest(-0ll, -0, 0);
}

}  // namespace hal
}  // namespace stratum
