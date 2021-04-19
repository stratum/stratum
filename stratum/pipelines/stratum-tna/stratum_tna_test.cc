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

DECLARE_string(grpc_addr);
DECLARE_string(p4_info_file);
DECLARE_string(p4_pipeline_config_file);
DECLARE_uint64(device_id);

namespace stratum {
namespace pipelines {
namespace stratum_tna {
namespace {

using tools::benchmark::BuildP4RTEntityIdReplacementMap;
using tools::benchmark::HydrateP4RuntimeProtoFromStringOrDie;

class StratumTnaTest : public hal::P4RuntimeFixture {
 protected:
  void SetUp() override {
    P4RuntimeFixture::SetUp();
    ASSERT_OK(BuildP4RTEntityIdReplacementMap(P4Info(), &p4_id_replacements_));
    for (const auto& e : p4_id_replacements_) {
      LOG(INFO) << e.first << " -> " << e.second;
    }
  }

  ::p4::v1::TableEntry CreateIpv4RouteEntry(std::string dst_addr, uint32 prefix,
                                            uint16 dst_port, uint64 dst_mac) {
    ::p4::v1::TableEntry route_entry =
        HydrateP4RuntimeProtoFromStringOrDie< ::p4::v1::TableEntry>(
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
              param_id: 1 # port
              value: "\x00"
            }
            params {
              param_id: 2 # dmac
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
  ASSERT_OK(InstallTableEntry(
      SutP4RuntimeSession(),
      CreateIpv4RouteEntry("\x0a\x00\x00\x01", 24, 1, 0x000000aaaaaa)));
}

}  // namespace
}  // namespace stratum_tna
}  // namespace pipelines
}  // namespace stratum
