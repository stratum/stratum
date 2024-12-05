// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// This file contains unit tests for P4StaticEntryMapper.

#include "stratum/hal/lib/p4/p4_proto_printer.h"

#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "gflags/gflags.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
// #include "p4/v1/p4runtime.pb.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/status/status_test_util.h"
// #include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::HasSubstr;
using ::testing::Return;

// This class is the P4ProtoPrinterTest test fixture.
class P4ProtoPrinterTest : public testing::Test {
 protected:
  ::p4::config::v1::P4Info GetTestP4Info() const {
    constexpr char kP4InfoString[] = R"pb(
      pkg_info {
        arch: "tna"
      }
      tables {
        preamble {
          id: 33583783
          name: "Ingress.control.table1"
        }
        match_fields {
          id: 1
          name: "ingress_port"
          bitwidth: 9
          match_type: EXACT
        }
        match_fields {
          id: 2
          name: "vlan_id"
          bitwidth: 12
          match_type: TERNARY
        }
        match_fields {
          id: 3
          name: "field3"
          bitwidth: 15
          match_type: RANGE
        }
        action_refs {
          id: 16794911
        }
        const_default_action_id: 16836487
        direct_resource_ids: 318814845
        size: 1024
      }
      actions {
        preamble {
          id: 16794911
          name: "Ingress.control.action1"
        }
        params {
          id: 1
          name: "vlan_id"
          bitwidth: 12
        }
        params {
          id: 2
          name: "vrf"
          bitwidth: 12
        }
      }
      direct_counters {
        preamble {
          id: 318814845
          name: "Ingress.control.counter1"
        }
        spec {
          unit: BOTH
        }
        direct_table_id: 33583783
      }
      meters {
        preamble {
          id: 55555
          name: "Ingress.control.meter_bytes"
          alias: "meter_bytes"
        }
        spec {
          unit: BYTES
        }
        size: 500
      }
      meters {
        preamble {
          id: 55556
          name: "Ingress.control.meter_packets"
          alias: "meter_packets"
        }
        spec {
          unit: PACKETS
        }
        size: 500
      }
    )pb";
    ::p4::config::v1::P4Info p4_info;
    CHECK_OK(ParseProtoFromString(kP4InfoString, &p4_info));
    return p4_info;
  }

  ::p4::v1::Action GetTestAction() const {
    ::p4::v1::Action a;
    a.set_action_id(kActionId);
    auto* p = a.add_params();
    p->set_param_id(1);
    p->set_value("\xab");
    p = a.add_params();
    p->set_param_id(2);
    p->set_value("\xff");
    return a;
  }

  ::p4::v1::FieldMatch GetTestExactFieldMatch() const {
    ::p4::v1::FieldMatch fm;
    fm.set_field_id(1);
    fm.mutable_exact()->set_value("\xab");
    return fm;
  }

  static constexpr uint64 kNodeId = 123123123;
  static constexpr uint32 kTableId = 33583783;
  static constexpr uint32 kActionId = 16794911;

  ::p4::config::v1::P4Info p4_info_;
};

constexpr uint64 P4ProtoPrinterTest::kNodeId;
constexpr uint32 P4ProtoPrinterTest::kTableId;
constexpr uint32 P4ProtoPrinterTest::kActionId;

TEST_F(P4ProtoPrinterTest, PrettyPrintAction) {
  ::p4::config::v1::P4Info p4_info = GetTestP4Info();
  ::p4::v1::Action a = GetTestAction();

  std::string s;
  ASSERT_OK(PrettyPrintP4ProtoToString(p4_info, a, &s));
  LOG(WARNING) << "pretty:\n" << s;
  LOG(WARNING) << "normal:\n" << a.DebugString();
}

TEST_F(P4ProtoPrinterTest, PrettyPrintTableEntry) {
  ::p4::config::v1::P4Info p4_info = GetTestP4Info();
  ::p4::v1::TableEntry t;
  t.set_table_id(kTableId);
  *t.mutable_action()->mutable_action() = GetTestAction();
  *t.add_match() = GetTestExactFieldMatch();

  std::string s;
  ASSERT_OK(PrettyPrintP4ProtoToString(p4_info, t, &s));
  LOG(WARNING) << "pretty:\n" << s;
  LOG(WARNING) << "normal:\n" << t.DebugString();
}

}  // namespace hal
}  // namespace stratum
