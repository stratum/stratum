// Copyright 2022-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/p4runtime_bfrt_translator.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/gtl/stl_util.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/barefoot/bf_sde_mock.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/p4runtime_bfrt_translator_mock.h"
#include "stratum/hal/lib/barefoot/utils.h"
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

DECLARE_bool(experimental_enable_p4runtime_translation);

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

class P4RuntimeBfrtTranslatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bf_sde_mock_ = absl::make_unique<BfSdeMock>();
    p4rt_bfrt_translator_ =
        P4RuntimeBfrtTranslator::CreateInstance(bf_sde_mock_.get(), kDeviceId);
    FLAGS_experimental_enable_p4runtime_translation = true;
  }

  ::util::Status PushChassisConfig() {
    ChassisConfig config;
    EXPECT_OK(::stratum::ParseProtoFromString(kChassisConfig, &config));
    const PortKey port_key(kSlot, kPort, kChannel);
    EXPECT_CALL(*bf_sde_mock_, GetPortIdFromPortKey(kDeviceId, port_key))
        .WillOnce(Return(::util::StatusOr<uint32>(kSdkPortId)));
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
  std::unique_ptr<P4RuntimeBfrtTranslator> p4rt_bfrt_translator_;

  static constexpr int kDeviceId = 1;
  static constexpr uint64 kNodeId = 0;
  static constexpr uint32 kPortId = 1;
  static constexpr uint32 kSdkPortId = 300;
  static constexpr int32 kPort = 1;
  static constexpr int32 kSlot = 1;
  static constexpr int32 kChannel = 1;

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

constexpr int P4RuntimeBfrtTranslatorTest::kDeviceId;
constexpr uint32 P4RuntimeBfrtTranslatorTest::kPortId;
constexpr uint32 P4RuntimeBfrtTranslatorTest::kSdkPortId;
constexpr int32 P4RuntimeBfrtTranslatorTest::kPort;
constexpr int32 P4RuntimeBfrtTranslatorTest::kSlot;
constexpr int32
    P4RuntimeBfrtTranslatorTest::P4RuntimeBfrtTranslatorTest::kChannel;
constexpr char P4RuntimeBfrtTranslatorTest::kChassisConfig[];
constexpr char P4RuntimeBfrtTranslatorTest::kP4InfoString[];

TEST_F(P4RuntimeBfrtTranslatorTest, PushConfig) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
}

TEST_F(P4RuntimeBfrtTranslatorTest, TranslateValue_UnknownUri) {
  EXPECT_OK(PushChassisConfig());

  // Unknown URI
  EXPECT_THAT(
      TranslateValue("some value", "foo", false, kTnaPortIdBitWidth).status(),
      DerivedFromStatus(::util::Status(StratumErrorSpace(), ERR_UNIMPLEMENTED,
                                       "Unknown URI: foo")));
}

TEST_F(P4RuntimeBfrtTranslatorTest, TranslateValue_InvalidSize) {
  EXPECT_OK(PushChassisConfig());
  // Invalid size
  EXPECT_THAT(
      TranslateValue("some value", kUriTnaPortId, false, kTnaPortIdBitWidth)
          .status(),
      DerivedFromStatus(
          ::util::Status(StratumErrorSpace(), ERR_INVALID_PARAM,
                         "'value.size() == "
                         "NumBitsToNumBytes(kTnaPortIdBitWidth)' is false.")));
}

TEST_F(P4RuntimeBfrtTranslatorTest, TranslateValue_MissingMappingToSdk) {
  EXPECT_OK(PushChassisConfig());
  // No mapping from singleton port to sdk port
  auto singleton_port_id = Uint32ToBytes(10, kTnaPortIdBitWidth);
  EXPECT_THAT(
      TranslateValue(singleton_port_id, kUriTnaPortId, true, kTnaPortIdBitWidth)
          .status(),
      DerivedFromStatus(::util::Status(
          StratumErrorSpace(), ERR_INVALID_PARAM,
          "'singleton_port_to_sdk_port_.count(port_id)' is false. ")));
}

TEST_F(P4RuntimeBfrtTranslatorTest, TranslateValue_MissingMappingToPort) {
  EXPECT_OK(PushChassisConfig());
  // No mapping from sdk port to singleton port
  auto sdk_port_id = Uint32ToBytes(10, kTnaPortIdBitWidth);
  EXPECT_THAT(
      TranslateValue(sdk_port_id, kUriTnaPortId, false, kTnaPortIdBitWidth)
          .status(),
      DerivedFromStatus(::util::Status(
          StratumErrorSpace(), ERR_INVALID_PARAM,
          "'sdk_port_to_singleton_port_.count(sdk_port_id)' is false. ")));
}

TEST_F(P4RuntimeBfrtTranslatorTest, TranslateValue_ToSdk) {
  EXPECT_OK(PushChassisConfig());
  // Translate from singleton port to sdk port
  auto singleton_port_id = Uint32ToBytes(kPortId, kTnaPortIdBitWidth);
  auto expected_value = Uint32ToBytes(kSdkPortId, kTnaPortIdBitWidth);
  auto actual_value =
      TranslateValue(singleton_port_id, kUriTnaPortId, true, kTnaPortIdBitWidth)
          .ValueOrDie();
  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(P4RuntimeBfrtTranslatorTest, TranslateValue_FromSdk) {
  EXPECT_OK(PushChassisConfig());
  // Translate from sdk port to singleton port
  auto sdk_port_id = Uint32ToBytes(kSdkPortId, kTnaPortIdBitWidth);
  auto expected_value = Uint32ToBytes(kPortId, kTnaPortIdBitWidth);
  auto actual_value =
      TranslateValue(sdk_port_id, kUriTnaPortId, false, kTnaPortIdBitWidth)
          .ValueOrDie();
  EXPECT_EQ(expected_value, actual_value);
}

// Table entry
TEST_F(P4RuntimeBfrtTranslatorTest, WriteTableEntryRequest) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char write_req_str[] = R"PROTO(
    updates {
      entity {
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
        }
      }
    }
  )PROTO";
  const char expected_write_req_str[] = R"PROTO(
    updates {
      entity {
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
      }
    }
  )PROTO";

  ::p4::v1::WriteRequest write_req;
  EXPECT_OK(ParseProtoFromString(write_req_str, &write_req));
  auto translated_value =
      p4rt_bfrt_translator_->TranslateWriteRequest(write_req);
  EXPECT_OK(translated_value.status());
  write_req = translated_value.ConsumeValueOrDie();
  ::p4::v1::WriteRequest expected_write_req;
  EXPECT_OK(ParseProtoFromString(expected_write_req_str, &expected_write_req));
  EXPECT_THAT(write_req, EqualsProto(expected_write_req));
}

TEST_F(P4RuntimeBfrtTranslatorTest, ReadTableEntryRequest) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char read_req_str[] = R"PROTO(
    entities {
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
    }
  )PROTO";
  const char expected_read_req_str[] = R"PROTO(
    entities {
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
    }
  )PROTO";

  ::p4::v1::ReadRequest read_req;
  EXPECT_OK(ParseProtoFromString(read_req_str, &read_req));
  auto translated_value = p4rt_bfrt_translator_->TranslateReadRequest(read_req);
  EXPECT_OK(translated_value.status());
  read_req = translated_value.ConsumeValueOrDie();
  ::p4::v1::ReadRequest expected_read_req;
  EXPECT_OK(ParseProtoFromString(expected_read_req_str, &expected_read_req));
  EXPECT_THAT(read_req, EqualsProto(expected_read_req));
}

TEST_F(P4RuntimeBfrtTranslatorTest, ReadTableEntryResponse) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  const char read_resp_str[] = R"PROTO(
    entities {
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
    }
  )PROTO";
  const char expected_read_resp_str[] = R"PROTO(
    entities {
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
    }
  )PROTO";
  ::p4::v1::ReadResponse read_resp;
  EXPECT_OK(ParseProtoFromString(read_resp_str, &read_resp));
  auto translated_value =
      p4rt_bfrt_translator_->TranslateReadResponse(read_resp);
  EXPECT_OK(translated_value.status());
  read_resp = translated_value.ConsumeValueOrDie();
  ::p4::v1::ReadResponse expected_read_resp;
  EXPECT_OK(ParseProtoFromString(expected_read_resp_str, &expected_read_resp));
  EXPECT_THAT(read_resp, EqualsProto(expected_read_resp));
}

TEST_F(P4RuntimeBfrtTranslatorTest, WriteTableEntry_InvalidTernary) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  // mask must be all-one.
  const char write_req_str[] = R"PROTO(
    updates {
      entity {
        table_entry {
          table_id: 33583783
          match {
            field_id: 2
            ternary { value: "\x00\x00\x00\x01" mask: "\x00\x00\xff\xff" }
          }
        }
      }
    }
  )PROTO";

  ::p4::v1::WriteRequest write_req;
  EXPECT_OK(ParseProtoFromString(write_req_str, &write_req));
  EXPECT_THAT(p4rt_bfrt_translator_->TranslateWriteRequest(write_req).status(),
              DerivedFromStatus(::util::Status(
                  StratumErrorSpace(), ERR_INVALID_PARAM,
                  "'field_match.ternary().mask() == all_one' is false.")));
}

TEST_F(P4RuntimeBfrtTranslatorTest, WriteTableEntry_InvalidRange) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  // mask must be all-one.
  const char write_req_str[] = R"PROTO(
    updates {
      entity {
        table_entry {
          table_id: 33583783
          match {
            field_id: 3
            range { low: "foo" high: "bar" }
          }
        }
      }
    }
  )PROTO";

  ::p4::v1::WriteRequest write_req;
  EXPECT_OK(ParseProtoFromString(write_req_str, &write_req));
  EXPECT_THAT(p4rt_bfrt_translator_->TranslateWriteRequest(write_req).status(),
              DerivedFromStatus(
                  ::util::Status(StratumErrorSpace(), ERR_INVALID_PARAM,
                                 "'field_match.range().low() == "
                                 "field_match.range().high()' is false.")));
}

TEST_F(P4RuntimeBfrtTranslatorTest, WriteTableEntry_InvalidLpm) {
  EXPECT_OK(PushChassisConfig());
  EXPECT_OK(PushForwardingPipelineConfig());
  // mask must be all-one.
  const char write_req_str[] = R"PROTO(
    updates {
      entity {
        table_entry {
          table_id: 33583783
          match {
            field_id: 4
            lpm { value: "\x00\x00\x00\x01" prefix_len: 10 }
          }
        }
      }
    }
  )PROTO";

  ::p4::v1::WriteRequest write_req;
  EXPECT_OK(ParseProtoFromString(write_req_str, &write_req));
  EXPECT_THAT(
      p4rt_bfrt_translator_->TranslateWriteRequest(write_req).status(),
      DerivedFromStatus(::util::Status(
          StratumErrorSpace(), ERR_INVALID_PARAM,
          "'field_match.lpm().prefix_len() == from_bit_width' is false.")));
}

// TODO(Yi Tseng): Will support these tests in other PRs.
// Action profile member
// Meter entry (translate index)
// Direct meter entry (translate index)
// Counter entry (translate index)
// Direct counter entry (translate index)
// Register entry (translate index)
// Packet replication engine entry (no p4info represent this, but still need to
// translate)
// PacketIO
class TranslatorWriterWrapperTest : public ::testing::Test {
 public:
  bool Write(const ::p4::v1::ReadResponse& msg) { return wrapper_->Write(msg); }

 protected:
  void SetUp() override {
    writer_mock_ = absl::make_unique<WriterMock<::p4::v1::ReadResponse>>();
    p4runtime_bfrt_translator_mock_ =
        absl::make_unique<P4RuntimeBfrtTranslatorMock>();
    wrapper_ = absl::make_unique<P4RuntimeBfrtTranslationWriterWrapper>(
        writer_mock_.get(), p4runtime_bfrt_translator_mock_.get());
  }

  std::unique_ptr<P4RuntimeBfrtTranslationWriterWrapper> wrapper_;
  std::unique_ptr<WriterMock<::p4::v1::ReadResponse>> writer_mock_;
  // The pointer point to the translator, not owned by this class.
  std::unique_ptr<P4RuntimeBfrtTranslatorMock> p4runtime_bfrt_translator_mock_;
};

TEST_F(TranslatorWriterWrapperTest, Entities) {
  const char read_resp_str[] = R"PROTO(
    entities {
      table_entry {
        table_id: 1
        }
    }
  )PROTO";

  ::p4::v1::ReadResponse read_resp;
  EXPECT_OK(ParseProtoFromString(read_resp_str, &read_resp));
  EXPECT_CALL(*p4runtime_bfrt_translator_mock_,
              TranslateReadResponse(EqualsProto(read_resp)))
      .WillOnce(Return(::util::StatusOr<::p4::v1::ReadResponse>(read_resp)));
  EXPECT_CALL(*writer_mock_, Write(_)).WillOnce(Return(true));
  EXPECT_EQ(Write(read_resp), true);
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
