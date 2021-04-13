// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "gflags/gflags.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/p4runtime/p4runtime_fixture.h"
#include "stratum/lib/p4runtime/p4runtime_session.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

DEFINE_string(grpc_addr, "127.0.0.1:9339", "P4Runtime server address.");
DEFINE_string(p4_info_file, "",
              "Path to an optional P4Info text proto file. If specified, file "
              "content will be serialized into the p4info field in "
              "ForwardingPipelineConfig proto and pushed to the switch.");
DEFINE_string(p4_pipeline_config_file, "",
              "Path to an optional P4PipelineConfig bin proto file. If "
              "specified, file content will be serialized into the "
              "p4_device_config field in ForwardingPipelineConfig proto "
              "and pushed to the switch.");
DEFINE_uint64(device_id, 1, "P4Runtime device ID.");

namespace stratum {
namespace tools {
namespace benchmark {
namespace {

using test_utils::EqualsProto;

class FabricTest : public hal::P4RuntimeFixture {
 protected:
  std::vector<::p4::v1::TableEntry> CreateUpTo16KGenericFarTableEntries(
      int num_table_entries) {
    num_table_entries = std::min(num_table_entries, 1024 * 16);
    std::vector<::p4::v1::TableEntry> table_entries;
    table_entries.reserve(num_table_entries);
    for (int i = 0; i < num_table_entries; ++i) {
      const std::string far_entry_text = R"PROTO(
        table_id: 49866391 # FabricIngress.spgw.fars
        match {
          field_id: 1 # far_id
          exact {
            value: "\000\000\000\000"
          }
        }
        action {
          action {
            action_id: 24881235 # load_normal_far
            params {
              param_id: 1 # drop
              value: "\x00"
            }
            params {
              param_id: 2 # notify_cp
              value: "\x00"
            }
          }
        }
      )PROTO";
      ::p4::v1::TableEntry entry;
      CHECK_OK(ParseProtoFromString(far_entry_text, &entry));
      std::string value = hal::Uint32ToByteStream(i);
      while (value.size() < 4) value.insert(0, 1, '\x00');
      CHECK_EQ(4, value.size()) << StringToHex(value) << " for i " << i;
      entry.mutable_match(0)->mutable_exact()->set_value(value);
      table_entries.emplace_back(entry);
    }

    return table_entries;
  }
};

TEST_F(FabricTest, CanInsert16KFarEntries) {
  auto entries = CreateUpTo16KGenericFarTableEntries(16000);
  ASSERT_OK(InstallTableEntries(SutP4RuntimeSession(), entries));
  ASSERT_OK_AND_ASSIGN(auto read_entries,
                       ReadTableEntries(SutP4RuntimeSession()));
  ASSERT_EQ(entries.size(), read_entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    ASSERT_THAT(entries[i], EqualsProto(read_entries[i]));
  }
}

TEST_F(FabricTest, InsertTableEntry) {
  const std::string entry_text = R"PROTO(
      table_id: 39601850
      match {
        field_id: 1
        ternary {
          value: "\001\004"
          mask: "\001\377"
        }
      }
      action {
        action {
          action_id: 21161133
        }
      }
      priority: 10
    )PROTO";
  ::p4::v1::TableEntry entry;
  ASSERT_OK(ParseProtoFromString(entry_text, &entry));
  ASSERT_OK(InstallTableEntry(SutP4RuntimeSession(), entry));
}

}  // namespace
}  // namespace benchmark
}  // namespace tools
}  // namespace stratum
