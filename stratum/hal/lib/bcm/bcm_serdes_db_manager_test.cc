// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <string>

#include "stratum/hal/lib/bcm/bcm_serdes_db_manager.h"
#include "gflags/gflags.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

DECLARE_string(test_tmpdir);
DECLARE_string(bcm_serdes_db_proto_file);

using ::testing::HasSubstr;

namespace stratum {
namespace hal {
namespace bcm {

class BcmSerdesDbManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FLAGS_bcm_serdes_db_proto_file =
        FLAGS_test_tmpdir + "/bcm_serdes_db.pb.bin";
    bcm_serdes_db_manager_ = BcmSerdesDbManager::CreateInstance();
  }

  void SaveTestBcmSerdesDb() {
    BcmSerdesDb bcm_serdes_db;
    const std::string bcm_serdes_db_text = R"(
      bcm_serdes_db_entries {
        media_type: MEDIA_TYPE_QSFP_SR4
        vendor_name: "vendor_1"
        part_numbers: "part_number_1"
        part_numbers: "part_number_2"
        speed_bps: 20000000000
        bcm_serdes_board_config {
          bcm_serdes_chip_configs {
            value {
              # A valid serdes core config.
              bcm_serdes_core_configs {
                value {
                  bcm_serdes_lane_configs {
                    value {
                      intf_type: "sr"
                      bcm_serdes_register_configs {
                        key: 0x123
                        value: 0x01
                      }
                    }
                  }
                  bcm_serdes_lane_configs {
                    key: 1
                    value {
                      intf_type: "sr"
                      bcm_serdes_register_configs {
                        key: 0x123
                        value: 0x01
                      }
                    }
                  }
                }
              }
              # An invalid serdes core config with non-matching lane config.
              bcm_serdes_core_configs {
                key: 1
                value {
                  bcm_serdes_lane_configs {
                    value {
                      intf_type: "sr"
                      bcm_serdes_register_configs {
                        key: 456
                        value: 0x02
                      }
                    }
                  }
                  bcm_serdes_lane_configs {
                    key: 1
                    value {
                      intf_type: "sr"
                    }
                  }
                }
              }
              # Another invalid serdes core config where not all the lanes
              # have configs (2nd lane missing config).
              bcm_serdes_core_configs {
                key: 2
                value {
                  bcm_serdes_lane_configs {
                    value {
                      intf_type: "sr"
                      bcm_serdes_register_configs {
                        key: 789
                        value: 0x03
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      # serdes config for BP ports. part_numbers and vendor_name are
      # intentionally kept empty in this case.
      bcm_serdes_db_entries {
        media_type: MEDIA_TYPE_BP_COPPER
        speed_bps: 40000000000
        bcm_serdes_board_config {
          bcm_serdes_chip_configs {
            key: 2
            value {
              # A valid serdes core config.
              bcm_serdes_core_configs {
                value {
                  bcm_serdes_lane_configs {
                    value {
                      intf_type: "sfi"
                      bcm_serdes_register_configs {
                        key: 0x123
                        value: 0x01
                      }
                    }
                  }
                  bcm_serdes_lane_configs {
                    key: 1
                    value {
                      intf_type: "sfi"
                      bcm_serdes_register_configs {
                        key: 0x123
                        value: 0x01
                      }
                    }
                  }
                  bcm_serdes_lane_configs {
                    key: 2
                    value {
                      intf_type: "sfi"
                      bcm_serdes_register_configs {
                        key: 0x123
                        value: 0x01
                      }
                    }
                  }
                  bcm_serdes_lane_configs {
                    key: 3
                    value {
                      intf_type: "sfi"
                      bcm_serdes_register_configs {
                        key: 0x123
                        value: 0x01
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    )";
    ASSERT_OK(ParseProtoFromString(bcm_serdes_db_text, &bcm_serdes_db));
    // Save the serdes db text to a file to emulate the case where we read the
    // serdes DB file from file system.
    ASSERT_OK(
        WriteProtoToBinFile(bcm_serdes_db, FLAGS_bcm_serdes_db_proto_file));
  }

  ::util::Status TestLookup(uint64 speed_bps, int unit, int serdes_core,
                            int serdes_lane, int num_serdes_lanes,
                            MediaType media_type,
                            const std::string& vendor_name,
                            const std::string& part_number,
                            BcmSerdesLaneConfig* bcm_serdes_lane_config) {
    BcmPort bcm_port;
    FrontPanelPortInfo fp_port_info;
    bcm_port.set_speed_bps(speed_bps);
    bcm_port.set_unit(unit);
    bcm_port.set_serdes_core(serdes_core);
    bcm_port.set_serdes_lane(serdes_lane);
    bcm_port.set_num_serdes_lanes(num_serdes_lanes);
    fp_port_info.set_media_type(media_type);
    fp_port_info.set_vendor_name(vendor_name);
    fp_port_info.set_part_number(part_number);

    return bcm_serdes_db_manager_->LookupSerdesConfigForPort(
        bcm_port, fp_port_info, bcm_serdes_lane_config);
  }

  std::unique_ptr<BcmSerdesDbManager> bcm_serdes_db_manager_;
};

TEST_F(BcmSerdesDbManagerTest, LoadSuccess) {
  SaveTestBcmSerdesDb();
  EXPECT_OK(bcm_serdes_db_manager_->Load());
}

TEST_F(BcmSerdesDbManagerTest, LoadFailure) {
  if (PathExists(FLAGS_bcm_serdes_db_proto_file)) {
    ASSERT_OK(RemoveFile(FLAGS_bcm_serdes_db_proto_file));
  }
  EXPECT_FALSE(bcm_serdes_db_manager_->Load().ok());
}

TEST_F(BcmSerdesDbManagerTest, LookupSerdesConfigForPortSuccess) {
  SaveTestBcmSerdesDb();
  ASSERT_OK(bcm_serdes_db_manager_->Load());
  BcmSerdesLaneConfig bcm_serdes_lane_config;

  // Basic serdes lookup for a channelized port.
  EXPECT_OK(TestLookup(kTwentyGigBps, 0, 0, 0, 2, MEDIA_TYPE_QSFP_SR4,
                       "vendor_1", "part_number_1", &bcm_serdes_lane_config));
  EXPECT_EQ("sr", bcm_serdes_lane_config.intf_type());
  EXPECT_EQ(0x01,
            bcm_serdes_lane_config.bcm_serdes_register_configs().at(0x123));

  // The other part number from the same vendor should return the same.
  EXPECT_OK(TestLookup(kTwentyGigBps, 0, 0, 0, 2, MEDIA_TYPE_QSFP_SR4,
                       "vendor_1", "part_number_2", &bcm_serdes_lane_config));
  EXPECT_EQ("sr", bcm_serdes_lane_config.intf_type());
  EXPECT_EQ(0x01,
            bcm_serdes_lane_config.bcm_serdes_register_configs().at(0x123));

  // Basic serdes lookup for a backplane port (no part number, no vendor number)
  EXPECT_OK(TestLookup(kFortyGigBps, 2, 0, 0, 4, MEDIA_TYPE_BP_COPPER, "", "",
                       &bcm_serdes_lane_config));
  EXPECT_EQ("sfi", bcm_serdes_lane_config.intf_type());
  EXPECT_EQ(0x01,
            bcm_serdes_lane_config.bcm_serdes_register_configs().at(0x123));
}

TEST_F(BcmSerdesDbManagerTest,
       LookupSerdesConfigForPortFailureWhenMapLookupFails) {
  SaveTestBcmSerdesDb();
  ASSERT_OK(bcm_serdes_db_manager_->Load());
  BcmSerdesLaneConfig bcm_serdes_lane_config;

  // Unknown speed.
  ::util::Status status =
      TestLookup(kFiftyGigBps, 0, 0, 0, 2, MEDIA_TYPE_QSFP_SR4, "vendor_1",
                 "part_number_1", &bcm_serdes_lane_config);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("not find serdes lane info"));

  // Unknown media_type.
  status = TestLookup(kTwentyGigBps, 0, 0, 0, 2, MEDIA_TYPE_QSFP_COPPER,
                      "vendor_1", "part_number_1", &bcm_serdes_lane_config);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("not find serdes lane info"));

  // Unknown vendor.
  status = TestLookup(kTwentyGigBps, 0, 0, 0, 2, MEDIA_TYPE_QSFP_SR4,
                      "vendor_x", "part_number_1", &bcm_serdes_lane_config);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("not find serdes lane info"));

  // Unit part number.
  status = TestLookup(kTwentyGigBps, 0, 0, 0, 2, MEDIA_TYPE_QSFP_SR4,
                      "vendor_1", "part_number_x", &bcm_serdes_lane_config);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("not find serdes lane info"));

  // Unit not found.
  status = TestLookup(kTwentyGigBps, 99, 0, 0, 2, MEDIA_TYPE_QSFP_SR4,
                      "vendor_1", "part_number_1", &bcm_serdes_lane_config);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Unit 99 not found"));

  // Serdes core not found.
  status = TestLookup(kTwentyGigBps, 0, 99, 0, 2, MEDIA_TYPE_QSFP_SR4,
                      "vendor_1", "part_number_1", &bcm_serdes_lane_config);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Serdes core 99 not found"));

  // Serdes lane not found.
  status = TestLookup(kTwentyGigBps, 0, 0, 99, 2, MEDIA_TYPE_QSFP_SR4,
                      "vendor_1", "part_number_1", &bcm_serdes_lane_config);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Serdes lane 99 not found"));

  // Serdes lane config not found for all lanes (2nd lane missing config).
  status = TestLookup(kTwentyGigBps, 0, 2, 0, 2, MEDIA_TYPE_QSFP_SR4,
                      "vendor_1", "part_number_1", &bcm_serdes_lane_config);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Serdes lane 1 not found"));
}

TEST_F(BcmSerdesDbManagerTest,
       LookupSerdesConfigForPortFailureWhenSerdesLaneConfigsNotMatch) {
  SaveTestBcmSerdesDb();
  ASSERT_OK(bcm_serdes_db_manager_->Load());
  BcmSerdesLaneConfig bcm_serdes_lane_config;

  // Serdes core 1 in the config has non-matching serdes config for the 2 lanes.
  ::util::Status status =
      TestLookup(kTwentyGigBps, 0, 1, 0, 2, MEDIA_TYPE_QSFP_SR4, "vendor_1",
                 "part_number_1", &bcm_serdes_lane_config);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("do not have the same value"));
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
