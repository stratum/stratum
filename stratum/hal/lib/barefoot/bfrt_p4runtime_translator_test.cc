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
    bfrt_p4runtime_translator_ = BfrtP4RuntimeTranslator::CreateInstance(
        /*translation_enabled=*/true, bf_sde_mock_.get(), kDeviceId);
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
    return bfrt_p4runtime_translator_->PushChassisConfig(config, kNodeId);
  }

  ::util::Status PushForwardingPipelineConfig(
      const char* p4info_str = kP4InfoString) {
    ::p4::config::v1::P4Info p4info;
    EXPECT_OK(ParseProtoFromString(p4info_str, &p4info));
    return bfrt_p4runtime_translator_->PushForwardingPipelineConfig(p4info);
  }

  ::util::StatusOr<std::string> TranslateValue(const std::string& value,
                                               const std::string& uri,
                                               bool to_sdk, int32 bit_width) {
    ::absl::ReaderMutexLock l(&bfrt_p4runtime_translator_->lock_);
    return bfrt_p4runtime_translator_->TranslateValue(value, uri, to_sdk,
                                                      bit_width);
  }

  std::string Uint32ToBytes(uint32 value, int32 bit_width) {
    return P4RuntimeByteStringToPaddedByteString(Uint32ToByteStream(value),
                                                 NumBitsToNumBytes(bit_width));
  }

  template <typename E>
  void TestEntryTranslation(
      std::string from_entry_str, std::string to_entry_str, bool to_sdk,
      ::util::StatusOr<E> (BfrtP4RuntimeTranslator::*translation_func)(const E&,
                                                                       bool)) {
    E from_entry;
    E to_entry;
    EXPECT_OK(ParseProtoFromString(from_entry_str, &from_entry));
    EXPECT_OK(ParseProtoFromString(to_entry_str, &to_entry));
    ::util::StatusOr<E> res =
        (bfrt_p4runtime_translator_.get()->*translation_func)(from_entry,
                                                              to_sdk);
    EXPECT_OK(res.status());
    E translated_entry = res.ValueOrDie();
    EXPECT_THAT(translated_entry, EqualsProto(to_entry));
  }

  template <typename E>
  void TestEntryTranslation(
      std::string from_entry_str, std::string to_entry_str,
      ::util::StatusOr<E> (BfrtP4RuntimeTranslator::*translation_func)(
          const E&)) {
    E from_entry;
    E to_entry;
    EXPECT_OK(ParseProtoFromString(from_entry_str, &from_entry));
    EXPECT_OK(ParseProtoFromString(to_entry_str, &to_entry));
    ::util::StatusOr<E> res =
        (bfrt_p4runtime_translator_.get()->*translation_func)(from_entry);
    EXPECT_OK(res.status());
    E translated_entry = res.ValueOrDie();
    EXPECT_THAT(translated_entry, EqualsProto(to_entry));
  }

  std::unique_ptr<BfSdeMock> bf_sde_mock_;
  std::unique_ptr<BfrtP4RuntimeTranslator> bfrt_p4runtime_translator_;

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

  static constexpr char kNoTranslationP4InfoString[] = R"PROTO(
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
constexpr char BfrtP4RuntimeTranslatorTest::kNoTranslationP4InfoString[];
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
                  "'value.size() <= "
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
  auto expected_value = ByteStringToP4RuntimeByteString(
      Uint32ToBytes(kPortId, kTnaPortIdBitWidth));
  auto actual_value = TranslateValue(sdk_port_id, kUriTnaPortId,
                                     /*to_sdk=*/false, kTnaPortIdBitWidth)
                          .ValueOrDie();
  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(BfrtP4RuntimeTranslatorTest, TranslateP4Info) {
  TestEntryTranslation(kP4InfoString, kNoTranslationP4InfoString,
                       &BfrtP4RuntimeTranslator::TranslateP4Info);
}

// Table entry
TEST_F(BfrtP4RuntimeTranslatorTest, WriteTableEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 1
      exact { value: "\x01" }
    }
    match {
      field_id: 2
      ternary { value: "\x01" mask: "\xff\xff\xff\xff" }
    }
    match {
      field_id: 3
      range { low: "\x01" high: "\x01" }
    }
    match {
      field_id: 4
      lpm { value: "\x01" prefix_len: 32 }
    }
    match {
      field_id: 5
      optional { value: "\x01"}
    }
    match {
      field_id: 6
      exact { value: "\x01" }
    }
    action {
      action {
        action_id: 16794911
        params { param_id: 1 value: "\x01" }
        params { param_id: 2 value: "\x01" }
      }
    }
  )PROTO";
  constexpr char expected_table_entry_str[] = R"PROTO(
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
      exact { value: "\x01" }
    }
    action {
      action {
        action_id: 16794911
        params { param_id: 1 value: "\x01\x2C" }
        params { param_id: 2 value: "\x01" }
      }
    }
  )PROTO";

  TestEntryTranslation(table_entry_str, expected_table_entry_str, true,
                       &BfrtP4RuntimeTranslator::TranslateTableEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, WriteTableEntry_ActionProfileActionSet) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char table_entry_str[] = R"PROTO(
    table_id: 33583783
    action {
      action_profile_action_set {
        action_profile_actions {
          action {
            action_id: 16794911
            params { param_id: 1 value: "\x01" }
            params { param_id: 2 value: "\x01" }
          }
        }
      }
    }
  )PROTO";
  constexpr char expected_table_entry_str[] = R"PROTO(
    table_id: 33583783
    action {
      action_profile_action_set {
        action_profile_actions {
          action {
            action_id: 16794911
            params { param_id: 1 value: "\x01\x2C" }
            params { param_id: 2 value: "\x01" }
          }
        }
      }
    }
  )PROTO";

  TestEntryTranslation(table_entry_str, expected_table_entry_str, true,
                       &BfrtP4RuntimeTranslator::TranslateTableEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadTableEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char table_entry_str[] = R"PROTO(
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
      exact { value: "\x01" }
    }
    action {
      action {
        action_id: 16794911
        params { param_id: 1 value: "\x01\x2C" }
        params { param_id: 2 value: "\x01" }
      }
    }

  )PROTO";
  constexpr char expected_table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 1
      exact { value: "\x01" }
    }
    match {
      field_id: 2
      ternary { value: "\x01" mask: "\xff\xff\xff\xff" }
    }
    match {
      field_id: 3
      range { low: "\x01" high: "\x01" }
    }
    match {
      field_id: 4
      lpm { value: "\x01" prefix_len: 32 }
    }
    match {
      field_id: 5
      optional { value: "\x01" }
    }
    match {
      field_id: 6
      exact { value: "\x01" }
    }
    action {
      action {
        action_id: 16794911
        params { param_id: 1 value: "\x01" }
        params { param_id: 2 value: "\x01" }
      }
    }
  )PROTO";

  TestEntryTranslation(table_entry_str, expected_table_entry_str, false,
                       &BfrtP4RuntimeTranslator::TranslateTableEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadTableEntry_ActionProfileActionSet) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char table_entry_str[] = R"PROTO(
    table_id: 33583783
    action {
      action_profile_action_set {
        action_profile_actions {
          action {
            action_id: 16794911
            params { param_id: 1 value: "\x01\x2C" }
            params { param_id: 2 value: "\x01" }
          }
        }
      }
    }
  )PROTO";
  constexpr char expected_table_entry_str[] = R"PROTO(
    table_id: 33583783
    action {
      action_profile_action_set {
        action_profile_actions {
          action {
            action_id: 16794911
            params { param_id: 1 value: "\x01" }
            params { param_id: 2 value: "\x01" }
          }
        }
      }
    }
  )PROTO";

  TestEntryTranslation(table_entry_str, expected_table_entry_str, false,
                       &BfrtP4RuntimeTranslator::TranslateTableEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, WriteTableEntry_InvalidTernary) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  // mask must be all-one.
  constexpr char table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 2
      ternary { value: "\x01" mask: "\xff\xff" }
    }
  )PROTO";

  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(ParseProtoFromString(table_entry_str, &table_entry));
  EXPECT_THAT(bfrt_p4runtime_translator_->TranslateTableEntry(table_entry, true)
                  .status(),
              DerivedFromStatus(::util::Status(
                  StratumErrorSpace(), ERR_INVALID_PARAM,
                  "'field_match.ternary().mask() == "
                  "AllOnesByteString(from_bit_width)' is false.")));
}

TEST_F(BfrtP4RuntimeTranslatorTest, WriteTableEntry_InvalidRange) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  // low and high must be the same value.
  constexpr char table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 3
      range { low: "foo" high: "bar" }
    }
  )PROTO";

  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(ParseProtoFromString(table_entry_str, &table_entry));
  EXPECT_THAT(bfrt_p4runtime_translator_->TranslateTableEntry(table_entry, true)
                  .status(),
              DerivedFromStatus(
                  ::util::Status(StratumErrorSpace(), ERR_INVALID_PARAM,
                                 "'field_match.range().low() == "
                                 "field_match.range().high()' is false.")));
}

TEST_F(BfrtP4RuntimeTranslatorTest, WriteTableEntry_InvalidLpm) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  // prefix must be the same value as bitwidth of the field
  constexpr char table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 4
      lpm { value: "\x01" prefix_len: 10 }
    }
  )PROTO";

  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(ParseProtoFromString(table_entry_str, &table_entry));
  EXPECT_THAT(
      bfrt_p4runtime_translator_->TranslateTableEntry(table_entry, true)
          .status(),
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
      params { param_id: 1 value: "\x01" }
      params { param_id: 2 value: "\x01" }
    }
  )PROTO";
  constexpr char expected_action_profile_member_str[] = R"PROTO(
    action_profile_id: 1
    member_id: 1
    action {
      action_id: 16794911
      params { param_id: 1 value: "\x01\x2C" }
      params { param_id: 2 value: "\x01" }
    }
  )PROTO";

  TestEntryTranslation(action_profile_member_str,
                       expected_action_profile_member_str, true,
                       &BfrtP4RuntimeTranslator::TranslateActionProfileMember);
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadActionProfileMember) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char action_profile_member_str[] = R"PROTO(
    action_profile_id: 1
    member_id: 1
    action {
      action_id: 16794911
      params { param_id: 1 value: "\x01\x2C" }
      params { param_id: 2 value: "\x01" }
    }
  )PROTO";
  constexpr char expected_action_profile_member_str[] = R"PROTO(
    action_profile_id: 1
    member_id: 1
    action {
      action_id: 16794911
      params { param_id: 1 value: "\x01" }
      params { param_id: 2 value: "\x01" }
    }
  )PROTO";

  TestEntryTranslation(action_profile_member_str,
                       expected_action_profile_member_str, false,
                       &BfrtP4RuntimeTranslator::TranslateActionProfileMember);
}

// Packet replication engine.
TEST_F(BfrtP4RuntimeTranslatorTest, WritePRE_MulticastGroup) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char pre_entry_str[] = R"PROTO(
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
  constexpr char expected_pre_entry_str[] = R"PROTO(
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

  TestEntryTranslation(
      pre_entry_str, expected_pre_entry_str, true,
      &BfrtP4RuntimeTranslator::TranslatePacketReplicationEngineEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadPRE_MulticastGroup) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char pre_entry_str[] = R"PROTO(
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
  constexpr char expected_pre_entry_str[] = R"PROTO(
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

  TestEntryTranslation(
      pre_entry_str, expected_pre_entry_str, false,
      &BfrtP4RuntimeTranslator::TranslatePacketReplicationEngineEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, WritePRE_CloneSession) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char pre_entry_str[] = R"PROTO(
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
  constexpr char expected_pre_entry_str[] = R"PROTO(
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

  TestEntryTranslation(
      pre_entry_str, expected_pre_entry_str, true,
      &BfrtP4RuntimeTranslator::TranslatePacketReplicationEngineEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadPRE_CloneSession) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char pre_entry_str[] = R"PROTO(
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
  constexpr char expected_pre_entry_str[] = R"PROTO(
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

  TestEntryTranslation(
      pre_entry_str, expected_pre_entry_str, false,
      &BfrtP4RuntimeTranslator::TranslatePacketReplicationEngineEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, WritePRE_InvalidPort) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char pre_entry_str[] = R"PROTO(
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
      bfrt_p4runtime_translator_
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
      value: "\x01" # egress port
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

  TestEntryTranslation(packet_out_str, expected_packet_out_str,
                       &BfrtP4RuntimeTranslator::TranslatePacketOut);
}

TEST_F(BfrtP4RuntimeTranslatorTest, PacketIn) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  char packet_in_str[] = R"PROTO(
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
  char expected_packet_in_str[] = R"PROTO(
    payload: "<raw packet>"
    metadata {
      metadata_id: 1
      value: "\x01" # ingress port
    }
    metadata {
      metadata_id: 2
      value: "\x00" # padding
    }
  )PROTO";

  TestEntryTranslation(packet_in_str, expected_packet_in_str,
                       &BfrtP4RuntimeTranslator::TranslatePacketIn);
}

// Counter entry
TEST_F(BfrtP4RuntimeTranslatorTest, WriteCounterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char counter_entry_str[] = R"PROTO(
    counter_id: 318814845
    index {
      index: 1
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  constexpr char expected_counter_entry_str[] = R"PROTO(
    counter_id: 318814845
    index {
      index: 300
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";

  TestEntryTranslation(counter_entry_str, expected_counter_entry_str, true,
                       &BfrtP4RuntimeTranslator::TranslateCounterEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadCounterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char counter_entry_str[] = R"PROTO(
    counter_id: 318814845
    index {
      index: 300
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  constexpr char expected_counter_entry_str[] = R"PROTO(
    counter_id: 318814845
    index {
      index: 1
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  TestEntryTranslation(counter_entry_str, expected_counter_entry_str, false,
                       &BfrtP4RuntimeTranslator::TranslateCounterEntry);
}

// Direct counter entry
TEST_F(BfrtP4RuntimeTranslatorTest, WriteDirectCounterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char direct_counter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x01" }
      }
      match {
        field_id: 2
        ternary { value: "\x01" mask: "\xff\xff\xff\xff" }
      }
      match {
        field_id: 3
        range { low: "\x01" high: "\x01" }
      }
      match {
        field_id: 4
        lpm { value: "\x01" prefix_len: 32 }
      }
      match {
        field_id: 5
        optional { value: "\x01" }
      }
      match {
        field_id: 6
        exact { value: "\x01" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01" }
          params { param_id: 2 value: "\x01" }
        }
      }
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  constexpr char expected_direct_counter_entry_str[] = R"PROTO(
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
        exact { value: "\x01" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01\x2C" }
          params { param_id: 2 value: "\x01" }
        }
      }
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";

  TestEntryTranslation(direct_counter_entry_str,
                       expected_direct_counter_entry_str, true,
                       &BfrtP4RuntimeTranslator::TranslateDirectCounterEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadDirectCounterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char direct_counter_entry_str[] = R"PROTO(
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
        exact { value: "\x01\x2C" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01\x2C" }
          params { param_id: 2 value: "\x01\x2C" }
        }
      }
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  constexpr char expected_direct_counter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x01" }
      }
      match {
        field_id: 2
        ternary { value: "\x01" mask: "\xff\xff\xff\xff" }
      }
      match {
        field_id: 3
        range { low: "\x01" high: "\x01" }
      }
      match {
        field_id: 4
        lpm { value: "\x01" prefix_len: 32 }
      }
      match {
        field_id: 5
        optional { value: "\x01" }
      }
      match {
        field_id: 6
        exact { value: "\x01\x2C" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01" }
          params { param_id: 2 value: "\x01\x2C" }
        }
      }
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";

  TestEntryTranslation(direct_counter_entry_str,
                       expected_direct_counter_entry_str, false,
                       &BfrtP4RuntimeTranslator::TranslateDirectCounterEntry);
}

// Meter entry
TEST_F(BfrtP4RuntimeTranslatorTest, WriteMeterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char meter_entry_str[] = R"PROTO(
    meter_id: 55555
    index {
      index: 1
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";
  constexpr char expected_meter_entry_str[] = R"PROTO(
    meter_id: 55555
    index {
      index: 300
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";

  TestEntryTranslation(meter_entry_str, expected_meter_entry_str, true,
                       &BfrtP4RuntimeTranslator::TranslateMeterEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadMeterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char meter_entry_str[] = R"PROTO(
    meter_id: 55555
    index {
      index: 300
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";
  constexpr char expected_meter_entry_str[] = R"PROTO(
    meter_id: 55555
    index {
      index: 1
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";

  TestEntryTranslation(meter_entry_str, expected_meter_entry_str, false,
                       &BfrtP4RuntimeTranslator::TranslateMeterEntry);
}

// Direct meter entry
TEST_F(BfrtP4RuntimeTranslatorTest, WriteDirectMeterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char direct_meter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x01" }
      }
      match {
        field_id: 2
        ternary { value: "\x01" mask: "\xff\xff\xff\xff" }
      }
      match {
        field_id: 3
        range { low: "\x01" high: "\x01" }
      }
      match {
        field_id: 4
        lpm { value: "\x01" prefix_len: 32 }
      }
      match {
        field_id: 5
        optional { value: "\x01" }
      }
      match {
        field_id: 6
        exact { value: "\x01" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01" }
          params { param_id: 2 value: "\x01" }
        }
      }
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";
  constexpr char expected_direct_meter_entry_str[] = R"PROTO(
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
        exact { value: "\x01" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01\x2C" }
          params { param_id: 2 value: "\x01" }
        }
      }
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";

  TestEntryTranslation(direct_meter_entry_str, expected_direct_meter_entry_str,
                       true,
                       &BfrtP4RuntimeTranslator::TranslateDirectMeterEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadDirectMeterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char direct_meter_entry_str[] = R"PROTO(
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
        exact { value: "\x01\x2C" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01\x2C" }
          params { param_id: 2 value: "\x01\x2C" }
        }
      }
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";
  constexpr char expected_direct_meter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x01" }
      }
      match {
        field_id: 2
        ternary { value: "\x01" mask: "\xff\xff\xff\xff" }
      }
      match {
        field_id: 3
        range { low: "\x01" high: "\x01" }
      }
      match {
        field_id: 4
        lpm { value: "\x01" prefix_len: 32 }
      }
      match {
        field_id: 5
        optional { value: "\x01" }
      }
      match {
        field_id: 6
        exact { value: "\x01\x2C" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01" }
          params { param_id: 2 value: "\x01\x2C" }
        }
      }
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";

  TestEntryTranslation(direct_meter_entry_str, expected_direct_meter_entry_str,
                       false,
                       &BfrtP4RuntimeTranslator::TranslateDirectMeterEntry);
}

// Register entry
TEST_F(BfrtP4RuntimeTranslatorTest, WriteRegisterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char register_entry_str[] = R"PROTO(
    register_id: 66666
    index {
      index: 1
    }
    data {
      bitstring: "\x00"
    }
  )PROTO";
  constexpr char expected_register_entry_str[] = R"PROTO(
    register_id: 66666
    index {
      index: 300
    }
    data {
      bitstring: "\x00"
    }
  )PROTO";

  TestEntryTranslation(register_entry_str, expected_register_entry_str, true,
                       &BfrtP4RuntimeTranslator::TranslateRegisterEntry);
}

TEST_F(BfrtP4RuntimeTranslatorTest, ReadRegisterEntry) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char register_entry_str[] = R"PROTO(
    register_id: 66666
    index {
      index: 300
    }
    data {
      bitstring: "\x00"
    }
  )PROTO";
  constexpr char expected_register_entry_str[] = R"PROTO(
    register_id: 66666
    index {
      index: 1
    }
    data {
      bitstring: "\x00"
    }
  )PROTO";

  TestEntryTranslation(register_entry_str, expected_register_entry_str, false,
                       &BfrtP4RuntimeTranslator::TranslateRegisterEntry);
}

// Translation disabled
TEST_F(BfrtP4RuntimeTranslatorTest, TranslationDisabled) {
  bfrt_p4runtime_translator_ = BfrtP4RuntimeTranslator::CreateInstance(
      /*translation_enabled=*/false, bf_sde_mock_.get(), kDeviceId);
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  constexpr char table_entry_str[] = R"PROTO(
    table_id: 33583783
    match {
      field_id: 1
      exact { value: "\x01" }
    }
    action {
      action {
        action_id: 16794911
        params { param_id: 1 value: "\x01" }
      }
    }
  )PROTO";

  TestEntryTranslation(table_entry_str, table_entry_str, true,
                       &BfrtP4RuntimeTranslator::TranslateTableEntry);

  constexpr char action_profile_member_str[] = R"PROTO(
    action_profile_id: 1
    member_id: 1
    action {
      action_id: 16794911
      params { param_id: 1 value: "\x01" }
      params { param_id: 2 value: "\x01" }
    }
  )PROTO";

  TestEntryTranslation(action_profile_member_str, action_profile_member_str,
                       true,
                       &BfrtP4RuntimeTranslator::TranslateActionProfileMember);

  constexpr char direct_meter_entry_str[] = R"PROTO(
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
        exact { value: "\x01\x2C" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01\x2C" }
          params { param_id: 2 value: "\x01\x2C" }
        }
      }
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";
  TestEntryTranslation(direct_meter_entry_str, direct_meter_entry_str, true,
                       &BfrtP4RuntimeTranslator::TranslateDirectMeterEntry);

  constexpr char meter_entry_str[] = R"PROTO(
    meter_id: 55555
    index {
      index: 1
    }
    config {
      cir: 1
      pir: 1
    }
  )PROTO";
  TestEntryTranslation(meter_entry_str, meter_entry_str, true,
                       &BfrtP4RuntimeTranslator::TranslateMeterEntry);

  constexpr char counter_entry_str[] = R"PROTO(
    counter_id: 318814845
    index {
      index: 1
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  TestEntryTranslation(counter_entry_str, counter_entry_str, true,
                       &BfrtP4RuntimeTranslator::TranslateCounterEntry);

  constexpr char direct_counter_entry_str[] = R"PROTO(
    table_entry {
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\x01" }
      }
      match {
        field_id: 2
        ternary { value: "\x01" mask: "\xff\xff\xff\xff" }
      }
      match {
        field_id: 3
        range { low: "\x01" high: "\x01" }
      }
      match {
        field_id: 4
        lpm { value: "\x01" prefix_len: 32 }
      }
      match {
        field_id: 5
        optional { value: "\x01" }
      }
      match {
        field_id: 6
        exact { value: "\x01" }
      }
      action {
        action {
          action_id: 16794911
          params { param_id: 1 value: "\x01" }
          params { param_id: 2 value: "\x01" }
        }
      }
    }
    data {
      byte_count: 1
      packet_count: 1
    }
  )PROTO";
  TestEntryTranslation(direct_counter_entry_str, direct_counter_entry_str, true,
                       &BfrtP4RuntimeTranslator::TranslateDirectCounterEntry);

  constexpr char pre_entry_str[] = R"PROTO(
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
  TestEntryTranslation(
      pre_entry_str, pre_entry_str, true,
      &BfrtP4RuntimeTranslator::TranslatePacketReplicationEngineEntry);

  char packet_out_str[] = R"PROTO(
    payload: "<raw packet>"
    metadata {
      metadata_id: 1
      value: "\x00" # padding
    }
    metadata {
      metadata_id: 2
      value: "\x01" # egress port
    }
  )PROTO";
  TestEntryTranslation(packet_out_str, packet_out_str,
                       &BfrtP4RuntimeTranslator::TranslatePacketOut);

  char packet_in_str[] = R"PROTO(
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
  TestEntryTranslation(packet_in_str, packet_in_str,
                       &BfrtP4RuntimeTranslator::TranslatePacketIn);

  constexpr char register_entry_str[] = R"PROTO(
    register_id: 66666
    index {
      index: 300
    }
    data {
      bitstring: "\x00"
    }
  )PROTO";
  TestEntryTranslation(register_entry_str, register_entry_str, true,
                       &BfrtP4RuntimeTranslator::TranslateRegisterEntry);

  TestEntryTranslation(kNoTranslationP4InfoString, kNoTranslationP4InfoString,
                       &BfrtP4RuntimeTranslator::TranslateP4Info);
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
