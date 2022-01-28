// Copyright 2022-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_p4runtime_translator.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/gtl/stl_util.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/barefoot/bf_sde_mock.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/bfrt_p4runtime_translator_mock.h"
#include "stratum/hal/lib/barefoot/utils.h"
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

MATCHER_P(DerivedFromStatus, status, "") {
  if (arg.error_code() != status.error_code()) {
    return false;
  }
  if (arg.error_message().find(status.error_message()) == std::string::npos) {
    *result_listener << "\nOriginal error string: \"" << status.error_message()
                     << "\" is missing from the actual status.";
    return false;
  }
  return true;
}

class BfrtP4RuntimeTranslatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bf_sde_mock_ = absl::make_unique<BfSdeMock>();
    p4rt_bfrt_translator_ = BfrtP4RuntimeTranslator::CreateInstance(
        bf_sde_mock_.get(), kDeviceId, /* enable translation */ true);
  }

  ::util::Status PushChassisConfig() {
    ChassisConfig config;
    EXPECT_OK(::stratum::ParseProtoFromString(kChassisConfig, &config));
    const PortKey port_key(kSlot, kPort, kChannel);
    EXPECT_CALL(*bf_sde_mock_, GetPortIdFromPortKey(kDeviceId, port_key))
        .WillOnce(Return(::util::StatusOr<uint32>(kSdkPortId)));
    const PortKey port2_key(kSlot, kPort2, kChannel);
    EXPECT_CALL(*bf_sde_mock_, GetPortIdFromPortKey(kDeviceId, port2_key))
        .WillOnce(Return(::util::StatusOr<uint32>(kSdkPort2Id)));
    EXPECT_CALL(*bf_sde_mock_, GetPcieCpuPort(kDeviceId))
        .WillOnce(Return(::util::StatusOr<uint32>(kSdkCpuPortId)));
    return p4rt_bfrt_translator_->PushChassisConfig(config, kNodeId);
  }

  ::util::Status PushForwardingPipelineConfig() {
    ::p4::config::v1::P4Info p4info;
    EXPECT_OK(ParseProtoFromString(kP4InfoString, &p4info));
    return p4rt_bfrt_translator_->PushForwardingPipelineConfig(p4info);
  }

  ::util::StatusOr<std::string> TranslateValue(const std::string& value,
                                               const std::string& uri,
                                               bool to_sdk, int32 bit_width) {
    ::absl::ReaderMutexLock l(&p4rt_bfrt_translator_->lock_);
    return p4rt_bfrt_translator_->TranslateValue(value, uri, to_sdk, bit_width);
  }

  std::string Uint32ToBytes(uint32 value, int32 bit_width) {
    return P4RuntimeByteStringToPaddedByteString(Uint32ToByteStream(value),
                                                 NumBitsToNumBytes(bit_width));
  }

  std::unique_ptr<BfSdeMock> bf_sde_mock_;
  std::unique_ptr<BfrtP4RuntimeTranslator> p4rt_bfrt_translator_;

  static constexpr int kDeviceId = 1;
  static constexpr uint64 kNodeId = 0;
  static constexpr uint32 kSdkCpuPortId = 320;
  // Singleton port 1
  static constexpr uint32 kPortId = 1;
  static constexpr uint32 kSdkPortId = 300;
  static constexpr int32 kPort = 1;
  static constexpr int32 kSlot = 1;
  static constexpr int32 kChannel = 1;
  // Singleton port 2
  static constexpr uint32 kPort2Id = 2;
  static constexpr uint32 kSdkPort2Id = 301;
  static constexpr int32 kPort2 = 2;

  static constexpr char kChassisConfig[] = R"PROTO(
    nodes {
      id: 1
    }
    singleton_ports {
      id: 1
      slot: 1
      port: 1
      channel: 1
    }
    singleton_ports {
      id: 2
      slot: 1
      port: 2
      channel: 1
    }
  )PROTO";

  static constexpr char kP4InfoString[] = R"PROTO(
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
        name: "field1"
        bitwidth: 32
        match_type: EXACT
        type_name {
          name: "FabricPortId_t"
        }
      }
      match_fields {
        id: 2
        name: "field2"
        bitwidth: 32
        match_type: TERNARY
        type_name {
          name: "FabricPortId_t"
        }
      }
      match_fields {
        id: 3
        name: "field3"
        bitwidth: 32
        match_type: RANGE
        type_name {
          name: "FabricPortId_t"
        }
      }
      match_fields {
        id: 4
        name: "field4"
        bitwidth: 32
        match_type: LPM
        type_name {
          name: "FabricPortId_t"
        }
      }
      match_fields {
        id: 5
        name: "field5"
        bitwidth: 32
        match_type: OPTIONAL
        type_name {
          name: "FabricPortId_t"
        }
      }
      match_fields {
        id: 6
        name: "field6"
        bitwidth: 32
        match_type: EXACT
      }
      action_refs {
        id: 16794911
      }
      const_default_action_id: 16836487
      size: 1024
      direct_resource_ids: 330152573
    }
    actions {
      preamble {
        id: 16794911
        name: "Ingress.control.action1"
      }
      params {
        id: 1
        name: "port_id"
        bitwidth: 32
        type_name {
          name: "FabricPortId_t"
        }
      }
      params {
        id: 2
        name: "don't translate"
        bitwidth: 32
      }
    }
    counters {
      preamble {
        id: 318814845
        name: "Ingress.control.counter1"
      }
      spec {
        unit: BOTH
      }
      index_type_name {
        name: "FabricPortId_t"
      }
    }
    direct_counters {
      preamble {
        id: 330152573
        name: "Ingress.control.table1_counter"
        alias: "table1_counter"
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
      index_type_name {
        name: "FabricPortId_t"
      }
      size: 500
    }
    registers {
      preamble {
        id: 66666
        name: "Ingress.control.my_register"
        alias: "my_register"
      }
      type_spec {
        bitstring {
          bit {
            bitwidth: 32
          }
        }
      }
      size: 10
      index_type_name {
        name: "FabricPortId_t"
      }
    }
    controller_packet_metadata {
      preamble {
        id: 81826293
        name: "packet_in"
        alias: "packet_in"
        annotations: "@controller_header(\"packet_in\")"
      }
      metadata {
        id: 1
        name: "ingress_port"
        bitwidth: 32
        type_name {
          name: "FabricPortId_t"
        }
      }
      metadata {
        id: 2
        name: "_pad0"
        bitwidth: 7
      }
    }
    controller_packet_metadata {
      preamble {
        id: 76689799
        name: "packet_out"
        alias: "packet_out"
        annotations: "@controller_header(\"packet_out\")"
      }
      metadata {
        id: 1
        name: "pad0"
        annotations: "@padding"
        bitwidth: 7
      }
      metadata {
        id: 2
        name: "egress_port"
        bitwidth: 32
        type_name {
          name: "FabricPortId_t"
        }
      }
    }
    type_info {
      new_types {
        key: "FabricPortId_t"
        value {
          translated_type {
            uri: "tna/PortId_t"
            sdn_bitwidth: 32
          }
        }
      }
    }
  )PROTO";
};

constexpr int BfrtP4RuntimeTranslatorTest::kDeviceId;
constexpr uint32 BfrtP4RuntimeTranslatorTest::kSdkCpuPortId;
constexpr uint32 BfrtP4RuntimeTranslatorTest::kPortId;
constexpr uint32 BfrtP4RuntimeTranslatorTest::kSdkPortId;
constexpr int32 BfrtP4RuntimeTranslatorTest::kPort;
constexpr int32 BfrtP4RuntimeTranslatorTest::kSlot;
constexpr int32 BfrtP4RuntimeTranslatorTest::kChannel;
constexpr char BfrtP4RuntimeTranslatorTest::kChassisConfig[];
constexpr char BfrtP4RuntimeTranslatorTest::kP4InfoString[];
constexpr uint32 BfrtP4RuntimeTranslatorTest::kPort2Id;
constexpr uint32 BfrtP4RuntimeTranslatorTest::kSdkPort2Id;
constexpr int32 BfrtP4RuntimeTranslatorTest::kPort2;

TEST_F(BfrtP4RuntimeTranslatorTest, PushConfig) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
}

TEST_F(BfrtP4RuntimeTranslatorTest, TranslateValue_UnknownUri) {
  EXPECT_OK(PushChassisConfig());

  // Unknown URI
  EXPECT_THAT(
      TranslateValue("some value", "foo", /*to_sdk=*/false, kTnaPortIdBitWidth)
          .status(),
      DerivedFromStatus(::util::Status(StratumErrorSpace(), ERR_UNIMPLEMENTED,
                                       "Unknown URI: foo")));
}

TEST_F(BfrtP4RuntimeTranslatorTest, TranslateValue_InvalidSize) {
  EXPECT_OK(PushChassisConfig());
  // Invalid size
  EXPECT_THAT(TranslateValue("some value", kUriTnaPortId, /*to_sdk=*/false,
                             kTnaPortIdBitWidth)
                  .status(),
              DerivedFromStatus(::util::Status(
                  StratumErrorSpace(), ERR_INVALID_PARAM,
                  "'value.size() == "
                  "NumBitsToNumBytes(kTnaPortIdBitWidth)' is false.")));
}

TEST_F(BfrtP4RuntimeTranslatorTest, TranslateValue_MissingMappingToSdk) {
  EXPECT_OK(PushChassisConfig());
  // No mapping from singleton port to sdk port
  auto singleton_port_id = Uint32ToBytes(10, kTnaPortIdBitWidth);
  EXPECT_THAT(TranslateValue(singleton_port_id, kUriTnaPortId, /*to_sdk=*/true,
                             kTnaPortIdBitWidth)
                  .status(),
              DerivedFromStatus(::util::Status(
                  StratumErrorSpace(), ERR_INVALID_PARAM,
                  "'singleton_port_to_sdk_port_.count(port_id)' is false. ")));
}

TEST_F(BfrtP4RuntimeTranslatorTest, TranslateValue_MissingMappingToPort) {
  EXPECT_OK(PushChassisConfig());
  // No mapping from sdk port to singleton port
  auto sdk_port_id = Uint32ToBytes(10, kTnaPortIdBitWidth);
  EXPECT_THAT(
      TranslateValue(sdk_port_id, kUriTnaPortId, /*to_sdk=*/false,
                     kTnaPortIdBitWidth)
          .status(),
      DerivedFromStatus(::util::Status(
          StratumErrorSpace(), ERR_INVALID_PARAM,
          "'sdk_port_to_singleton_port_.count(sdk_port_id)' is false. ")));
}

TEST_F(BfrtP4RuntimeTranslatorTest, TranslateValue_ToSdk) {
  EXPECT_OK(PushChassisConfig());
  // Translate from singleton port to sdk port
  auto singleton_port_id = Uint32ToBytes(kPortId, kTnaPortIdBitWidth);
  auto expected_value = Uint32ToBytes(kSdkPortId, kTnaPortIdBitWidth);
  auto actual_value = TranslateValue(singleton_port_id, kUriTnaPortId,
                                     /*to_sdk=*/true, kTnaPortIdBitWidth)
                          .ValueOrDie();
  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(BfrtP4RuntimeTranslatorTest, TranslateValue_FromSdk) {
  EXPECT_OK(PushChassisConfig());
  // Translate from sdk port to singleton port
  auto sdk_port_id = Uint32ToBytes(kSdkPortId, kTnaPortIdBitWidth);
  auto expected_value = Uint32ToBytes(kPortId, kTnaPortIdBitWidth);
  auto actual_value = TranslateValue(sdk_port_id, kUriTnaPortId,
                                     /*to_sdk=*/false, kTnaPortIdBitWidth)
                          .ValueOrDie();
  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(BfrtP4RuntimeTranslatorTest, TranslateP4Info) {
  const char expect_translated_p4info_str[] = R"PROTO(
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
        name: "field1"
        bitwidth: 9
        match_type: EXACT
      }
      match_fields {
        id: 2
        name: "field2"
        bitwidth: 9
        match_type: TERNARY
      }
      match_fields {
        id: 3
        name: "field3"
        bitwidth: 9
        match_type: RANGE
      }
      match_fields {
        id: 4
        name: "field4"
        bitwidth: 9
        match_type: LPM
      }
      match_fields {
        id: 5
        name: "field5"
        bitwidth: 9
        match_type: OPTIONAL
      }
      match_fields {
        id: 6
        name: "field6"
        bitwidth: 32
        match_type: EXACT
      }
      action_refs {
        id: 16794911
      }
      const_default_action_id: 16836487
      size: 1024
      direct_resource_ids: 330152573
    }
    actions {
      preamble {
        id: 16794911
        name: "Ingress.control.action1"
      }
      params {
        id: 1
        name: "port_id"
        bitwidth: 9
      }
      params {
        id: 2
        name: "don't translate"
        bitwidth: 32
      }
    }
    counters {
      preamble {
        id: 318814845
        name: "Ingress.control.counter1"
      }
      spec {
        unit: BOTH
      }
    }
    direct_counters {
      preamble {
        id: 330152573
        name: "Ingress.control.table1_counter"
        alias: "table1_counter"
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
    registers {
      preamble {
        id: 66666
        name: "Ingress.control.my_register"
        alias: "my_register"
      }
      type_spec {
        bitstring {
          bit {
            bitwidth: 32
          }
        }
      }
      size: 10
    }
    controller_packet_metadata {
      preamble {
        id: 81826293
        name: "packet_in"
        alias: "packet_in"
        annotations: "@controller_header(\"packet_in\")"
      }
      metadata {
        id: 1
        name: "ingress_port"
        bitwidth: 9
      }
      metadata {
        id: 2
        name: "_pad0"
        bitwidth: 7
      }
    }
    controller_packet_metadata {
      preamble {
        id: 76689799
        name: "packet_out"
        alias: "packet_out"
        annotations: "@controller_header(\"packet_out\")"
      }
      metadata {
        id: 1
        name: "pad0"
        annotations: "@padding"
        bitwidth: 7
      }
      metadata {
        id: 2
        name: "egress_port"
        bitwidth: 9
      }
    }
  )PROTO";
  ::p4::config::v1::P4Info p4info;
  EXPECT_OK(ParseProtoFromString(kP4InfoString, &p4info));
  ::p4::config::v1::P4Info expected_translated_p4info;
  EXPECT_OK(ParseProtoFromString(expect_translated_p4info_str,
                                 &expected_translated_p4info));
  const auto& statusor = p4rt_bfrt_translator_->TranslateP4Info(p4info);
  EXPECT_OK(statusor.status());
  const auto& translated_p4info = statusor.ValueOrDie();
  EXPECT_THAT(translated_p4info, EqualsProto(expected_translated_p4info));
}

// Table entry
TEST_F(BfrtP4RuntimeTranslatorTest, WriteTableEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 1
      exact { value: "\x00\x00\x00\x01" }
    }
    match {
      field_id: 2
      ternary { value: "\x00\x00\x00\x01" mask: "\xff\xff\xff\xff" }
    }
    match {
      field_id: 3
      range { low: "\x00\x00\x00\x01" high: "\x00\x00\x00\x01" }
    }
    match {
      field_id: 4
      lpm { value: "\x00\x00\x00\x01" prefix_len: 32 }
    }
    match {
      field_id: 5
      optional { value: "\x00\x00\x00\x01"}
    }
    match {
      field_id: 6
      exact { value: "\x00\x00\x00\x01" }
    }
    action {
      action {
        action_id: 16794911
        params { param_id: 1 value: "\x00\x00\x00\x01" }
        params { param_id: 2 value: "\x00\x00\x00\x01" }
      }
    }
  )PROTO";
  const char expected_table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 1
      exact { value: "\x01\x2C" }
    }
    match {
      field_id: 2
      ternary { value: "\x01\x2C" mask: "\x01\xff" }
    }
    match {
      field_id: 3
      range { low: "\x01\x2C" high: "\x01\x2C" }
    }
    match {
      field_id: 4
      lpm { value: "\x01\x2C" prefix_len: 9 }
    }
    match {
      field_id: 5
      optional { value: "\x01\x2C" }
    }
    match {
      field_id: 6
      exact { value: "\x00\x00\x00\x01" }
    }
    action {
      action {
        action_id: 16794911
        params { param_id: 1 value: "\x01\x2C" }
        params { param_id: 2 value: "\x00\x00\x00\x01" }
      }
    }
  )PROTO";

  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(ParseProtoFromString(table_entry_str, &table_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslateTableEntry(table_entry, true);
  EXPECT_OK(translated_value.status());
  table_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::TableEntry expected_table_entry;
  EXPECT_OK(
      ParseProtoFromString(expected_table_entry_str, &expected_table_entry));
  EXPECT_THAT(table_entry, EqualsProto(expected_table_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, WriteTableEntry_ActionProfileActionSet) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char table_entry_str[] = R"PROTO(
    table_id: 33583783
    action {
      action_profile_action_set {
        action_profile_actions {
          action {
            action_id: 16794911
            params { param_id: 1 value: "\x00\x00\x00\x01" }
            params { param_id: 2 value: "\x00\x00\x00\x01" }
          }
        }
      }
    }
  )PROTO";
  const char expected_table_entry_str[] = R"PROTO(
    table_id: 33583783
    action {
      action_profile_action_set {
        action_profile_actions {
          action {
            action_id: 16794911
            params { param_id: 1 value: "\x01\x2C" }
            params { param_id: 2 value: "\x00\x00\x00\x01" }
          }
        }
      }
    }
  )PROTO";

  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(ParseProtoFromString(table_entry_str, &table_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslateTableEntry(table_entry, true);
  EXPECT_OK(translated_value.status());
  table_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::TableEntry expected_table_entry;
  EXPECT_OK(
      ParseProtoFromString(expected_table_entry_str, &expected_table_entry));
  EXPECT_THAT(table_entry, EqualsProto(expected_table_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadTableEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 1
      exact { value: "\x01\x2C" }
    }
    match {
      field_id: 2
      ternary { value: "\x01\x2C" mask: "\x01\xff" }
    }
    match {
      field_id: 3
      range { low: "\x01\x2C" high: "\x01\x2C" }
    }
    match {
      field_id: 4
      lpm { value: "\x01\x2C" prefix_len: 9 }
    }
    match {
      field_id: 5
      optional { value: "\x01\x2C" }
    }
    match {
      field_id: 6
      exact { value: "\x00\x00\x00\x01" }
    }
    action {
      action {
        action_id: 16794911
        params { param_id: 1 value: "\x01\x2C" }
        params { param_id: 2 value: "\x00\x00\x00\x01" }
      }
    }

  )PROTO";
  const char expected_table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 1
      exact { value: "\x00\x00\x00\x01" }
    }
    match {
      field_id: 2
      ternary { value: "\x00\x00\x00\x01" mask: "\xff\xff\xff\xff" }
    }
    match {
      field_id: 3
      range { low: "\x00\x00\x00\x01" high: "\x00\x00\x00\x01" }
    }
    match {
      field_id: 4
      lpm { value: "\x00\x00\x00\x01" prefix_len: 32 }
    }
    match {
      field_id: 5
      optional { value: "\x00\x00\x00\x01" }
    }
    match {
      field_id: 6
      exact { value: "\x00\x00\x00\x01" }
    }
    action {
      action {
        action_id: 16794911
        params { param_id: 1 value: "\x00\x00\x00\x01" }
        params { param_id: 2 value: "\x00\x00\x00\x01" }
      }
    }
  )PROTO";

  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(ParseProtoFromString(table_entry_str, &table_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslateTableEntry(table_entry, false);
  EXPECT_OK(translated_value.status());
  table_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::TableEntry expected_table_entry;
  EXPECT_OK(
      ParseProtoFromString(expected_table_entry_str, &expected_table_entry));
  EXPECT_THAT(table_entry, EqualsProto(expected_table_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadTableEntry_ActionProfileActionSet) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char table_entry_str[] = R"PROTO(
    table_id: 33583783
    action {
      action_profile_action_set {
        action_profile_actions {
          action {
            action_id: 16794911
            params { param_id: 1 value: "\x01\x2C" }
            params { param_id: 2 value: "\x00\x00\x00\x01" }
          }
        }
      }
    }
  )PROTO";
  const char expected_table_entry_str[] = R"PROTO(
    table_id: 33583783
    action {
      action_profile_action_set {
        action_profile_actions {
          action {
            action_id: 16794911
            params { param_id: 1 value: "\x00\x00\x00\x01" }
            params { param_id: 2 value: "\x00\x00\x00\x01" }
          }
        }
      }
    }
  )PROTO";

  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(ParseProtoFromString(table_entry_str, &table_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslateTableEntry(table_entry, false);
  EXPECT_OK(translated_value.status());
  table_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::TableEntry expected_table_entry;
  EXPECT_OK(
      ParseProtoFromString(expected_table_entry_str, &expected_table_entry));
  EXPECT_THAT(table_entry, EqualsProto(expected_table_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, WriteTableEntry_InvalidTernary) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  // mask must be all-one.
  const char table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 2
      ternary { value: "\x00\x00\x00\x01" mask: "\x00\x00\xff\xff" }
    }
  )PROTO";

  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(ParseProtoFromString(table_entry_str, &table_entry));
  EXPECT_THAT(
      p4rt_bfrt_translator_->TranslateTableEntry(table_entry, true).status(),
      DerivedFromStatus(
          ::util::Status(StratumErrorSpace(), ERR_INVALID_PARAM,
                         "'field_match.ternary().mask() == "
                         "AllOnesByteString(from_bit_width)' is false.")));
}

TEST_F(BfrtP4RuntimeTranslatorTest, WriteTableEntry_InvalidRange) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  // low and high must be the same value.
  const char table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 3
      range { low: "foo" high: "bar" }
    }
  )PROTO";

  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(ParseProtoFromString(table_entry_str, &table_entry));
  EXPECT_THAT(
      p4rt_bfrt_translator_->TranslateTableEntry(table_entry, true).status(),
      DerivedFromStatus(
          ::util::Status(StratumErrorSpace(), ERR_INVALID_PARAM,
                         "'field_match.range().low() == "
                         "field_match.range().high()' is false.")));
}

TEST_F(BfrtP4RuntimeTranslatorTest, WriteTableEntry_InvalidLpm) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  // prefix must be the same value as bitwidth of the field
  const char table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 4
      lpm { value: "\x00\x00\x00\x01" prefix_len: 10 }
    }
  )PROTO";

  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(ParseProtoFromString(table_entry_str, &table_entry));
  EXPECT_THAT(
      p4rt_bfrt_translator_->TranslateTableEntry(table_entry, true).status(),
      DerivedFromStatus(::util::Status(
          StratumErrorSpace(), ERR_INVALID_PARAM,
          "'field_match.lpm().prefix_len() == from_bit_width' is false.")));
}

// Action profile member
TEST_F(BfrtP4RuntimeTranslatorTest, WriteActionProfileMember) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char action_profile_member_str[] = R"PROTO(
    action_profile_id: 1
    member_id: 1
    action {
      action_id: 16794911
      params { param_id: 1 value: "\x00\x00\x00\x01" }
      params { param_id: 2 value: "\x00\x00\x00\x01" }
    }
  )PROTO";
  constexpr char expected_action_profile_member_str[] = R"PROTO(
    action_profile_id: 1
    member_id: 1
    action {
      action_id: 16794911
      params { param_id: 1 value: "\x01\x2C" }
      params { param_id: 2 value: "\x00\x00\x00\x01" }
    }
  )PROTO";
  ::p4::v1::ActionProfileMember action_profile_member;
  EXPECT_OK(
      ParseProtoFromString(action_profile_member_str, &action_profile_member));
  auto translated_value = p4rt_bfrt_translator_->TranslateActionProfileMember(
      action_profile_member, true);
  EXPECT_OK(translated_value.status());
  action_profile_member = translated_value.ConsumeValueOrDie();
  ::p4::v1::ActionProfileMember expected_action_profile_member;
  EXPECT_OK(ParseProtoFromString(expected_action_profile_member_str,
                                 &expected_action_profile_member));
  EXPECT_THAT(action_profile_member,
              EqualsProto(expected_action_profile_member));
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadActionProfileMember) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char action_profile_member_str[] = R"PROTO(
    action_profile_id: 1
    member_id: 1
    action {
      action_id: 16794911
      params { param_id: 1 value: "\x01\x2C" }
      params { param_id: 2 value: "\x00\x00\x00\x01" }
    }
  )PROTO";
  const char expected_action_profile_member_str[] = R"PROTO(
    action_profile_id: 1
    member_id: 1
    action {
      action_id: 16794911
      params { param_id: 1 value: "\x00\x00\x00\x01" }
      params { param_id: 2 value: "\x00\x00\x00\x01" }
    }
  )PROTO";
  ::p4::v1::ActionProfileMember action_profile_member;
  EXPECT_OK(
      ParseProtoFromString(action_profile_member_str, &action_profile_member));
  auto translated_value = p4rt_bfrt_translator_->TranslateActionProfileMember(
      action_profile_member, false);
  EXPECT_OK(translated_value.status());
  action_profile_member = translated_value.ConsumeValueOrDie();
  ::p4::v1::ActionProfileMember expected_action_profile_member;
  EXPECT_OK(ParseProtoFromString(expected_action_profile_member_str,
                                 &expected_action_profile_member));
  EXPECT_THAT(action_profile_member,
              EqualsProto(expected_action_profile_member));
}

// Packet replication engine.
TEST_F(BfrtP4RuntimeTranslatorTest, WritePRE_MulticastGroup) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char pre_entry_str[] = R"PROTO(
    multicast_group_entry {
      multicast_group_id: 1
      replicas {
        egress_port: 1
        instance: 1
      }
      replicas {
        egress_port: 2
        instance: 1
      }
    }
  )PROTO";
  const char expected_pre_entry_str[] = R"PROTO(
    multicast_group_entry {
      multicast_group_id: 1
      replicas {
        egress_port: 300
        instance: 1
      }
      replicas {
        egress_port: 301
        instance: 1
      }
    }
  )PROTO";

  ::p4::v1::PacketReplicationEngineEntry pre_entry;
  EXPECT_OK(ParseProtoFromString(pre_entry_str, &pre_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslatePacketReplicationEngineEntry(pre_entry,
                                                                   true);
  EXPECT_OK(translated_value.status());
  pre_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::PacketReplicationEngineEntry expected_pre_entry;
  EXPECT_OK(ParseProtoFromString(expected_pre_entry_str, &expected_pre_entry));
  EXPECT_THAT(pre_entry, EqualsProto(expected_pre_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadPRE_MulticastGroup) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char pre_entry_str[] = R"PROTO(
    multicast_group_entry {
      multicast_group_id: 1
      replicas {
        egress_port: 300
        instance: 1
      }
      replicas {
        egress_port: 301
        instance: 1
      }
    }
  )PROTO";
  const char expected_pre_entry_str[] = R"PROTO(
    multicast_group_entry {
      multicast_group_id: 1
      replicas {
        egress_port: 1
        instance: 1
      }
      replicas {
        egress_port: 2
        instance: 1
      }
    }
  )PROTO";
  ::p4::v1::PacketReplicationEngineEntry pre_entry;
  EXPECT_OK(ParseProtoFromString(pre_entry_str, &pre_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslatePacketReplicationEngineEntry(pre_entry,
                                                                   false);
  EXPECT_OK(translated_value.status());
  pre_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::PacketReplicationEngineEntry expected_pre_entry;
  EXPECT_OK(ParseProtoFromString(expected_pre_entry_str, &expected_pre_entry));
  EXPECT_THAT(pre_entry, EqualsProto(expected_pre_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, WritePRE_CloneSession) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char pre_entry_str[] = R"PROTO(
    clone_session_entry {
      session_id: 1
      replicas {
        egress_port: 1
        instance: 1
      }
      replicas {
        egress_port: 2
        instance: 1
      }
      replicas {
        egress_port: 0xfffffffd # CPU
        instance: 1
      }
      replicas {
        egress_port: 0xffffff00 # Recirculation port 0
        instance: 1
      }
      replicas {
        egress_port: 0xffffff01 # Recirculation port 1
        instance: 1
      }
      replicas {
        egress_port: 0xffffff02 # Recirculation port 2
        instance: 1
      }
      replicas {
        egress_port: 0xffffff03 # Recirculation port 3
        instance: 1
      }
    }
  )PROTO";
  const char expected_pre_entry_str[] = R"PROTO(
    clone_session_entry {
      session_id: 1
      replicas {
        egress_port: 300
        instance: 1
      }
      replicas {
        egress_port: 301
        instance: 1
      }
      replicas {
        egress_port: 320
        instance: 1
      }
      replicas {
        egress_port: 68
        instance: 1
      }
      replicas {
        egress_port: 196
        instance: 1
      }
      replicas {
        egress_port: 324
        instance: 1
      }
      replicas {
        egress_port: 452
        instance: 1
      }
    }
  )PROTO";

  ::p4::v1::PacketReplicationEngineEntry pre_entry;
  EXPECT_OK(ParseProtoFromString(pre_entry_str, &pre_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslatePacketReplicationEngineEntry(pre_entry,
                                                                   true);
  EXPECT_OK(translated_value.status());
  pre_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::PacketReplicationEngineEntry expected_pre_entry;
  EXPECT_OK(ParseProtoFromString(expected_pre_entry_str, &expected_pre_entry));
  EXPECT_THAT(pre_entry, EqualsProto(expected_pre_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadPRE_CloneSession) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char pre_entry_str[] = R"PROTO(
    clone_session_entry {
      session_id: 1
      replicas {
        egress_port: 300
        instance: 1
      }
      replicas {
        egress_port: 301
        instance: 1
      }
      replicas {
        egress_port: 320
        instance: 1
      }
      replicas {
        egress_port: 68
        instance: 1
      }
      replicas {
        egress_port: 196
        instance: 1
      }
      replicas {
        egress_port: 324
        instance: 1
      }
      replicas {
        egress_port: 452
        instance: 1
      }
    }
  )PROTO";
  const char expected_pre_entry_str[] = R"PROTO(
    clone_session_entry {
      session_id: 1
      replicas {
        egress_port: 1
        instance: 1
      }
      replicas {
        egress_port: 2
        instance: 1
      }
      replicas {
        egress_port: 0xfffffffd
        instance: 1
      }
      replicas {
        egress_port: 0xffffff00
        instance: 1
      }
      replicas {
        egress_port: 0xffffff01
        instance: 1
      }
      replicas {
        egress_port: 0xffffff02
        instance: 1
      }
      replicas {
        egress_port: 0xffffff03
        instance: 1
      }
    }
  )PROTO";
  ::p4::v1::PacketReplicationEngineEntry pre_entry;
  EXPECT_OK(ParseProtoFromString(pre_entry_str, &pre_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslatePacketReplicationEngineEntry(pre_entry,
                                                                   false);
  EXPECT_OK(translated_value.status());
  pre_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::PacketReplicationEngineEntry expected_pre_entry;
  EXPECT_OK(ParseProtoFromString(expected_pre_entry_str, &expected_pre_entry));
  EXPECT_THAT(pre_entry, EqualsProto(expected_pre_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, WritePRE_InvalidPort) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char pre_entry_str[] = R"PROTO(
    multicast_group_entry {
      multicast_group_id: 1
      replicas {
        egress_port: 3
        instance: 1
      }
    }
  )PROTO";

  ::p4::v1::PacketReplicationEngineEntry pre_entry;
  EXPECT_OK(ParseProtoFromString(pre_entry_str, &pre_entry));
  EXPECT_THAT(
      p4rt_bfrt_translator_
          ->TranslatePacketReplicationEngineEntry(pre_entry, true)
          .status(),
      DerivedFromStatus(::util::Status(StratumErrorSpace(), ERR_INVALID_PARAM,
                                       "'singleton_port_to_sdk_port_.count("
                                       "replica.egress_port())' is false.")));
}

// PacketIO
TEST_F(BfrtP4RuntimeTranslatorTest, PacketOut) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  char packet_out_str[] = R"PROTO(
    payload: "<raw packet>"
    metadata {
      metadata_id: 1
      value: "\x00" # padding
    }
    metadata {
      metadata_id: 2
      value: "\x00\x00\x00\x01" # egress port
    }
  )PROTO";
  char expected_packet_out_str[] = R"PROTO(
    payload: "<raw packet>"
    metadata {
      metadata_id: 1
      value: "\x00" # padding
    }
    metadata {
      metadata_id: 2
      value: "\x01\x2C" # egress port
    }
  )PROTO";
  ::p4::v1::PacketOut packet_out;
  ::p4::v1::PacketOut expected_packet_out;
  EXPECT_OK(ParseProtoFromString(packet_out_str, &packet_out));
  EXPECT_OK(
      ParseProtoFromString(expected_packet_out_str, &expected_packet_out));

  auto translated = p4rt_bfrt_translator_->TranslatePacketOut(packet_out);
  EXPECT_OK(translated.status());
  packet_out = translated.ValueOrDie();
  EXPECT_THAT(packet_out, EqualsProto(expected_packet_out));
}

TEST_F(BfrtP4RuntimeTranslatorTest, PacketIn) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  char packet_int_str[] = R"PROTO(
    payload: "<raw packet>"
    metadata {
      metadata_id: 1
      value: "\x01\x2C" # ingress port
    }
    metadata {
      metadata_id: 2
      value: "\x00" # padding
    }
  )PROTO";
  char expected_packet_int_str[] = R"PROTO(
    payload: "<raw packet>"
    metadata {
      metadata_id: 1
      value: "\x00\x00\x00\x01" # ingress port
    }
    metadata {
      metadata_id: 2
      value: "\x00" # padding
    }
  )PROTO";
  ::p4::v1::PacketIn packet_int;
  ::p4::v1::PacketIn expected_packet_int;
  EXPECT_OK(ParseProtoFromString(packet_int_str, &packet_int));
  EXPECT_OK(
      ParseProtoFromString(expected_packet_int_str, &expected_packet_int));

  auto translated = p4rt_bfrt_translator_->TranslatePacketIn(packet_int);
  EXPECT_OK(translated.status());
  packet_int = translated.ValueOrDie();
  EXPECT_THAT(packet_int, EqualsProto(expected_packet_int));
}

// Counter entry
TEST_F(BfrtP4RuntimeTranslatorTest, WriteCounterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char counter_entry_str[] = R"PROTO(
    counter_id: 318814845
    index {
      index: 1
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  const char expected_counter_entry_str[] = R"PROTO(
    counter_id: 318814845
    index {
      index: 300
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";

  ::p4::v1::CounterEntry counter_entry;
  EXPECT_OK(ParseProtoFromString(counter_entry_str, &counter_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslateCounterEntry(counter_entry, true);
  EXPECT_OK(translated_value.status());
  counter_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::CounterEntry expected_counter_entry;
  EXPECT_OK(ParseProtoFromString(expected_counter_entry_str,
                                 &expected_counter_entry));
  EXPECT_THAT(counter_entry, EqualsProto(expected_counter_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadCounterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char counter_entry_str[] = R"PROTO(
    counter_id: 318814845
    index {
      index: 300
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  const char expected_counter_entry_str[] = R"PROTO(
    counter_id: 318814845
    index {
      index: 1
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  ::p4::v1::CounterEntry counter_entry;
  EXPECT_OK(ParseProtoFromString(counter_entry_str, &counter_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslateCounterEntry(counter_entry, false);
  EXPECT_OK(translated_value.status());
  counter_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::CounterEntry expected_counter_entry;
  EXPECT_OK(ParseProtoFromString(expected_counter_entry_str,
                                 &expected_counter_entry));
  EXPECT_THAT(counter_entry, EqualsProto(expected_counter_entry));
}

// Direct counter entry
TEST_F(BfrtP4RuntimeTranslatorTest, WriteDirectCounterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char direct_counter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x00\x00\x00\x01" }
      }
      match {
        field_id: 2
        ternary { value: "\x00\x00\x00\x01" mask: "\xff\xff\xff\xff" }
      }
      match {
        field_id: 3
        range { low: "\x00\x00\x00\x01" high: "\x00\x00\x00\x01" }
      }
      match {
        field_id: 4
        lpm { value: "\x00\x00\x00\x01" prefix_len: 32 }
      }
      match {
        field_id: 5
        optional { value: "\x00\x00\x00\x01" }
      }
      match {
        field_id: 6
        exact { value: "\x00\x00\x00\x01" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x00\x00\x00\x01" }
          params { param_id: 2 value: "\x00\x00\x00\x01" }
        }
      }
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  const char expected_direct_counter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x01\x2C" }
      }
      match {
        field_id: 2
        ternary { value: "\x01\x2C" mask: "\x01\xff" }
      }
      match {
        field_id: 3
        range { low: "\x01\x2C" high: "\x01\x2C" }
      }
      match {
        field_id: 4
        lpm { value: "\x01\x2C" prefix_len: 9 }
      }
      match {
        field_id: 5
        optional { value: "\x01\x2C" }
      }
      match {
        field_id: 6
        exact { value: "\x00\x00\x00\x01" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01\x2C" }
          params { param_id: 2 value: "\x00\x00\x00\x01" }
        }
      }
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";

  ::p4::v1::DirectCounterEntry direct_counter_entry;
  EXPECT_OK(
      ParseProtoFromString(direct_counter_entry_str, &direct_counter_entry));
  auto translated_value = p4rt_bfrt_translator_->TranslateDirectCounterEntry(
      direct_counter_entry, true);
  EXPECT_OK(translated_value.status());
  direct_counter_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::DirectCounterEntry expected_direct_counter_entry;
  EXPECT_OK(ParseProtoFromString(expected_direct_counter_entry_str,
                                 &expected_direct_counter_entry));
  EXPECT_THAT(direct_counter_entry, EqualsProto(expected_direct_counter_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadDirectCounterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char direct_counter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x01\x2C" }
      }
      match {
        field_id: 2
        ternary { value: "\x01\x2C" mask: "\x01\xff" }
      }
      match {
        field_id: 3
        range { low: "\x01\x2C" high: "\x01\x2C" }
      }
      match {
        field_id: 4
        lpm { value: "\x01\x2C" prefix_len: 9 }
      }
      match {
        field_id: 5
        optional { value: "\x01\x2C" }
      }
      match {
        field_id: 6
        exact { value: "\x00\x00\x01\x2C" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01\x2C" }
          params { param_id: 2 value: "\x00\x00\x01\x2C" }
        }
      }
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  const char expected_direct_counter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x00\x00\x00\x01" }
      }
      match {
        field_id: 2
        ternary { value: "\x00\x00\x00\x01" mask: "\xff\xff\xff\xff" }
      }
      match {
        field_id: 3
        range { low: "\x00\x00\x00\x01" high: "\x00\x00\x00\x01" }
      }
      match {
        field_id: 4
        lpm { value: "\x00\x00\x00\x01" prefix_len: 32 }
      }
      match {
        field_id: 5
        optional { value: "\x00\x00\x00\x01" }
      }
      match {
        field_id: 6
        exact { value: "\x00\x00\x01\x2C" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x00\x00\x00\x01" }
          params { param_id: 2 value: "\x00\x00\x01\x2C" }
        }
      }
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  ::p4::v1::DirectCounterEntry direct_counter_entry;
  EXPECT_OK(
      ParseProtoFromString(direct_counter_entry_str, &direct_counter_entry));
  auto translated_value = p4rt_bfrt_translator_->TranslateDirectCounterEntry(
      direct_counter_entry, false);
  EXPECT_OK(translated_value.status());
  direct_counter_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::DirectCounterEntry expected_direct_counter_entry;
  EXPECT_OK(ParseProtoFromString(expected_direct_counter_entry_str,
                                 &expected_direct_counter_entry));
  EXPECT_THAT(direct_counter_entry, EqualsProto(expected_direct_counter_entry));
}

// Meter entry
TEST_F(BfrtP4RuntimeTranslatorTest, WriteMeterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char meter_entry_str[] = R"PROTO(
    meter_id: 55555
    index {
      index: 1
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";
  const char expected_meter_entry_str[] = R"PROTO(
    meter_id: 55555
    index {
      index: 300
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";

  ::p4::v1::MeterEntry meter_entry;
  EXPECT_OK(ParseProtoFromString(meter_entry_str, &meter_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslateMeterEntry(meter_entry, true);
  EXPECT_OK(translated_value.status());
  meter_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::MeterEntry expected_meter_entry;
  EXPECT_OK(
      ParseProtoFromString(expected_meter_entry_str, &expected_meter_entry));
  EXPECT_THAT(meter_entry, EqualsProto(expected_meter_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadMeterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char meter_entry_str[] = R"PROTO(
    meter_id: 55555
    index {
      index: 300
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";
  const char expected_meter_entry_str[] = R"PROTO(
    meter_id: 55555
    index {
      index: 1
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";
  ::p4::v1::MeterEntry meter_entry;
  EXPECT_OK(ParseProtoFromString(meter_entry_str, &meter_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslateMeterEntry(meter_entry, false);
  EXPECT_OK(translated_value.status());
  meter_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::MeterEntry expected_meter_entry;
  EXPECT_OK(
      ParseProtoFromString(expected_meter_entry_str, &expected_meter_entry));
  EXPECT_THAT(meter_entry, EqualsProto(expected_meter_entry));
}

// Direct meter entry
TEST_F(BfrtP4RuntimeTranslatorTest, WriteDirectMeterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char direct_meter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x00\x00\x00\x01" }
      }
      match {
        field_id: 2
        ternary { value: "\x00\x00\x00\x01" mask: "\xff\xff\xff\xff" }
      }
      match {
        field_id: 3
        range { low: "\x00\x00\x00\x01" high: "\x00\x00\x00\x01" }
      }
      match {
        field_id: 4
        lpm { value: "\x00\x00\x00\x01" prefix_len: 32 }
      }
      match {
        field_id: 5
        optional { value: "\x00\x00\x00\x01" }
      }
      match {
        field_id: 6
        exact { value: "\x00\x00\x00\x01" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x00\x00\x00\x01" }
          params { param_id: 2 value: "\x00\x00\x00\x01" }
        }
      }
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";
  const char expected_direct_meter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x01\x2C" }
      }
      match {
        field_id: 2
        ternary { value: "\x01\x2C" mask: "\x01\xff" }
      }
      match {
        field_id: 3
        range { low: "\x01\x2C" high: "\x01\x2C" }
      }
      match {
        field_id: 4
        lpm { value: "\x01\x2C" prefix_len: 9 }
      }
      match {
        field_id: 5
        optional { value: "\x01\x2C" }
      }
      match {
        field_id: 6
        exact { value: "\x00\x00\x00\x01" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01\x2C" }
          params { param_id: 2 value: "\x00\x00\x00\x01" }
        }
      }
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";

  ::p4::v1::DirectMeterEntry direct_meter_entry;
  EXPECT_OK(ParseProtoFromString(direct_meter_entry_str, &direct_meter_entry));
  auto translated_value = p4rt_bfrt_translator_->TranslateDirectMeterEntry(
      direct_meter_entry, true);
  EXPECT_OK(translated_value.status());
  direct_meter_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::DirectMeterEntry expected_direct_meter_entry;
  EXPECT_OK(ParseProtoFromString(expected_direct_meter_entry_str,
                                 &expected_direct_meter_entry));
  EXPECT_THAT(direct_meter_entry, EqualsProto(expected_direct_meter_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadDirectMeterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char direct_meter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x01\x2C" }
      }
      match {
        field_id: 2
        ternary { value: "\x01\x2C" mask: "\x01\xff" }
      }
      match {
        field_id: 3
        range { low: "\x01\x2C" high: "\x01\x2C" }
      }
      match {
        field_id: 4
        lpm { value: "\x01\x2C" prefix_len: 9 }
      }
      match {
        field_id: 5
        optional { value: "\x01\x2C" }
      }
      match {
        field_id: 6
        exact { value: "\x00\x00\x01\x2C" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01\x2C" }
          params { param_id: 2 value: "\x00\x00\x01\x2C" }
        }
      }
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";
  const char expected_direct_meter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x00\x00\x00\x01" }
      }
      match {
        field_id: 2
        ternary { value: "\x00\x00\x00\x01" mask: "\xff\xff\xff\xff" }
      }
      match {
        field_id: 3
        range { low: "\x00\x00\x00\x01" high: "\x00\x00\x00\x01" }
      }
      match {
        field_id: 4
        lpm { value: "\x00\x00\x00\x01" prefix_len: 32 }
      }
      match {
        field_id: 5
        optional { value: "\x00\x00\x00\x01" }
      }
      match {
        field_id: 6
        exact { value: "\x00\x00\x01\x2C" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x00\x00\x00\x01" }
          params { param_id: 2 value: "\x00\x00\x01\x2C" }
        }
      }
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";
  ::p4::v1::DirectMeterEntry direct_meter_entry;
  EXPECT_OK(ParseProtoFromString(direct_meter_entry_str, &direct_meter_entry));
  auto translated_value = p4rt_bfrt_translator_->TranslateDirectMeterEntry(
      direct_meter_entry, false);
  EXPECT_OK(translated_value.status());
  direct_meter_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::DirectMeterEntry expected_direct_meter_entry;
  EXPECT_OK(ParseProtoFromString(expected_direct_meter_entry_str,
                                 &expected_direct_meter_entry));
  EXPECT_THAT(direct_meter_entry, EqualsProto(expected_direct_meter_entry));
}

// Register entry
TEST_F(BfrtP4RuntimeTranslatorTest, WriteRegisterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char register_entry_str[] = R"PROTO(
    register_id: 66666
    index {
      index: 1
    }
    data {
      bitstring: "\x00"
    }
  )PROTO";
  const char expected_register_entry_str[] = R"PROTO(
    register_id: 66666
    index {
      index: 300
    }
    data {
      bitstring: "\x00"
    }
  )PROTO";

  ::p4::v1::RegisterEntry register_entry;
  EXPECT_OK(ParseProtoFromString(register_entry_str, &register_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslateRegisterEntry(register_entry, true);
  EXPECT_OK(translated_value.status());
  register_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::RegisterEntry expected_register_entry;
  EXPECT_OK(ParseProtoFromString(expected_register_entry_str,
                                 &expected_register_entry));
  EXPECT_THAT(register_entry, EqualsProto(expected_register_entry));
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadRegisterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char register_entry_str[] = R"PROTO(
    register_id: 66666
    index {
      index: 300
    }
    data {
      bitstring: "\x00"
    }
  )PROTO";
  const char expected_register_entry_str[] = R"PROTO(
    register_id: 66666
    index {
      index: 1
    }
    data {
      bitstring: "\x00"
    }
  )PROTO";
  ::p4::v1::RegisterEntry register_entry;
  EXPECT_OK(ParseProtoFromString(register_entry_str, &register_entry));
  auto translated_value =
      p4rt_bfrt_translator_->TranslateRegisterEntry(register_entry, false);
  EXPECT_OK(translated_value.status());
  register_entry = translated_value.ConsumeValueOrDie();
  ::p4::v1::RegisterEntry expected_register_entry;
  EXPECT_OK(ParseProtoFromString(expected_register_entry_str,
                                 &expected_register_entry));
  EXPECT_THAT(register_entry, EqualsProto(expected_register_entry));
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
