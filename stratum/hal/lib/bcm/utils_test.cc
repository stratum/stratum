// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/hal/lib/bcm/utils.h"

#include "stratum/lib/constants.h"
#include "gtest/gtest.h"

namespace stratum {
namespace hal {
namespace bcm {

TEST(BcmUtilsTest, PrintBcmPortForEmptyBcmPortProto) {
  BcmPort port;
  EXPECT_EQ("(slot: 0, port: 0, unit: 0, logical_port: 0)", PrintBcmPort(port));
}

TEST(BcmUtilsTest, PrintBcmPortForNonEmptyBcmPortProto) {
  BcmPort port;
  port.set_slot(1);
  EXPECT_EQ("(slot: 1, port: 0, unit: 0, logical_port: 0)", PrintBcmPort(port));

  port.set_port(10);
  EXPECT_EQ("(slot: 1, port: 10, unit: 0, logical_port: 0)",
            PrintBcmPort(port));

  port.set_channel(3);
  EXPECT_EQ("(slot: 1, port: 10, channel: 3, unit: 0, logical_port: 0)",
            PrintBcmPort(port));

  port.set_speed_bps(kFiftyGigBps);
  EXPECT_EQ(
      "(slot: 1, port: 10, channel: 3, unit: 0, logical_port: 0, speed: 50G)",
      PrintBcmPort(port));

  port.set_unit(2);
  EXPECT_EQ(
      "(slot: 1, port: 10, channel: 3, unit: 2, logical_port: 0, speed: 50G)",
      PrintBcmPort(port));

  port.set_logical_port(33);
  EXPECT_EQ(
      "(slot: 1, port: 10, channel: 3, unit: 2, logical_port: 33, speed: 50G)",
      PrintBcmPort(port));

  port.clear_channel();
  port.set_speed_bps(kFortyGigBps);
  EXPECT_EQ("(slot: 1, port: 10, unit: 2, logical_port: 33, speed: 40G)",
            PrintBcmPort(port));
}

TEST(BcmUtilsTest, PrintBcmPortWithDirectArgList) {}

TEST(BcmUtilsTest, PrintBcmPortOptionsForEmptyOption) {
  BcmPortOptions options;
  EXPECT_EQ("()", PrintBcmPortOptions(options));
}

TEST(BcmUtilsTest, PrintBcmPortOptionsForNonEmptyOption) {
  BcmPortOptions options;

  // Example 1.
  options.Clear();
  options.set_enabled(TRI_STATE_TRUE);
  options.set_blocked(TRI_STATE_FALSE);
  options.set_speed_bps(kFiftyGigBps);
  EXPECT_EQ("(enabled: true, blocked: false, speed: 50G)",
            PrintBcmPortOptions(options));

  // Example 2.
  options.Clear();
  options.set_blocked(TRI_STATE_TRUE);
  options.set_speed_bps(kFortyGigBps);
  options.set_max_frame_size(1200);
  EXPECT_EQ("(blocked: true, speed: 40G, max_frame_size: 1200)",
            PrintBcmPortOptions(options));

  // Example 3.
  options.Clear();
  options.set_enabled(TRI_STATE_FALSE);
  options.set_linkscan_mode(BcmPortOptions::LINKSCAN_MODE_SW);
  EXPECT_EQ("(enabled: false, linkscan_mode: LINKSCAN_MODE_SW)",
            PrintBcmPortOptions(options));

  // Example 4.
  options.Clear();
  options.set_enabled(TRI_STATE_TRUE);
  options.set_flex(TRI_STATE_FALSE);
  options.set_autoneg(TRI_STATE_TRUE);
  options.set_num_serdes_lanes(2);
  EXPECT_EQ("(enabled: true, flex: false, autoneg: true, num_serdes_lanes: 2)",
            PrintBcmPortOptions(options));

  // Example 5.
  options.Clear();
  options.set_loopback_mode(LOOPBACK_STATE_MAC);
  EXPECT_EQ("(loopback_mode: LOOPBACK_STATE_MAC)",
            PrintBcmPortOptions(options));
}

TEST(BcmUtilsTest, PrintBcmChipNumber) {
  BcmChip::BcmChipType chip_type = BcmChip::TOMAHAWK;
  EXPECT_EQ("BCM56960", PrintBcmChipNumber(chip_type));
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
