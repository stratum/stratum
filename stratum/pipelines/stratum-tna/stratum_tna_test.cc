// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "gflags/gflags.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/p4runtime/entity_management.h"
#include "stratum/lib/p4runtime/p4runtime_fixture.h"
#include "stratum/lib/p4runtime/p4runtime_session.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace pipelines {
namespace stratum_tna {
namespace {

using p4runtime::BuildP4RTEntityIdReplacementMap;
using p4runtime::HydrateP4RuntimeProtoFromStringOrDie;
using p4runtime::P4RuntimeFixture;

class StratumTnaTest : public P4RuntimeFixture {
 protected:
  void SetUp() override {
    FLAGS_p4_info_file =
        "stratum/pipelines/stratum-tna/stratum_tna_p4info.pb.txt";
    FLAGS_p4_pipeline_config_file =
        "stratum/pipelines/stratum-tna/stratum_tna.pb.bin";
    P4RuntimeFixture::SetUp();
    if (HasFailure()) return;
    ASSERT_OK(BuildP4RTEntityIdReplacementMap(P4Info(), &p4_id_replacements_));
    for (const auto& e : p4_id_replacements_) {
      LOG(INFO) << e.first << " -> " << e.second;
    }

    // TOOD(max): look this up from the switch somehow
    ASSERT_OK(SetSwitchInfo(320));
  }

  void TearDown() override { P4RuntimeFixture::TearDown(); }

  ::util::Status SetSwitchInfo(uint16 cpu_port) {
    ::p4::v1::TableEntry entry =
        HydrateP4RuntimeProtoFromStringOrDie<::p4::v1::TableEntry>(
            p4_id_replacements_,
            R"PROTO(
        table_id: {StratumEgress.switch_info}
        action {
          action {
            action_id: {StratumEgress.set_switch_info}
            params {
              param_id: {StratumEgress.set_switch_info.cpu_port}
              value: "\x00"
            }
          }
        }
        is_default_action: true
      )PROTO");
    entry.mutable_action()->mutable_action()->mutable_params(0)->set_value(
        hal::Uint32ToByteStream(cpu_port));

    return ModifyTableEntries(SutP4RuntimeSession(), {entry});
  }

  ::p4::v1::TableEntry CreateIpv4RouteEntry(std::string dst_addr, uint32 prefix,
                                            uint16 dst_port, uint64 dst_mac) {
    ::p4::v1::TableEntry route_entry =
        HydrateP4RuntimeProtoFromStringOrDie<::p4::v1::TableEntry>(
            p4_id_replacements_,
            R"PROTO(
        table_id: {StratumIngress.ipv4_route}
        match {
          field_id: {StratumIngress.ipv4_route.hdr.ipv4.dst_addr}
          lpm {
            value: "\000\000\000\000"
            prefix_len: 0
          }
        }
        action {
          action {
            action_id: {StratumIngress.fwd_route}
            params {
              param_id: {StratumIngress.fwd_route.port}
              value: "\x00"
            }
            params {
              param_id: {StratumIngress.fwd_route.dmac}
              value: "\x00"
            }
          }
        }
      )PROTO");
    route_entry.mutable_match(0)->mutable_lpm()->set_value(dst_addr);
    route_entry.mutable_match(0)->mutable_lpm()->set_prefix_len(prefix);
    route_entry.mutable_action()
        ->mutable_action()
        ->mutable_params(0)
        ->set_value(hal::Uint32ToByteStream(dst_port));
    route_entry.mutable_action()
        ->mutable_action()
        ->mutable_params(1)
        ->set_value(hal::Uint64ToByteStream(dst_mac));

    return route_entry;
  }

  absl::flat_hash_map<std::string, std::string> p4_id_replacements_;
};

TEST_F(StratumTnaTest, InsertTableEntry) {
  ASSERT_OK(
      InstallTableEntry(SutP4RuntimeSession(),
                        CreateIpv4RouteEntry(std::string("\x0a\x00\x00\x00", 4),
                                             24, 1, 0x000000aaaaaa)));
}

TEST_F(StratumTnaTest, PacketOut) {
  ::p4::v1::PacketOut packet_out =
      HydrateP4RuntimeProtoFromStringOrDie<::p4::v1::PacketOut>(
          p4_id_replacements_,
          R"PROTO(
              payload: "\x00\x00\x00\xcc\xcc\xcc\x00\x00\x00\xaa\xaa\xaa\x08\x00"
              metadata {
                metadata_id: 1
                value: "\x01\x0c"
              }
              metadata {
                metadata_id: 2
                value: "\x00"
              }
              metadata {
                metadata_id: 3
                value: "\xBF\x01"
              }
      )PROTO");

  // FIXME: Wait for port up.
  absl::SleepFor(absl::Seconds(2));

  EXPECT_OK(SendPacketOut(SutP4RuntimeSession(), packet_out));
}

}  // namespace
}  // namespace stratum_tna
}  // namespace pipelines
}  // namespace stratum
