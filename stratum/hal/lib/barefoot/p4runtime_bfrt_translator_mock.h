// Copyright 2022-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_P4RUNTIME_BFRT_TRANSLATOR_MOCK_H_
#define STRATUM_HAL_LIB_BAREFOOT_P4RUNTIME_BFRT_TRANSLATOR_MOCK_H_

#include <string>

#include "gmock/gmock.h"
#include "stratum/hal/lib/barefoot/p4runtime_bfrt_translator.h"

namespace stratum {
namespace hal {
namespace barefoot {

class P4RuntimeBfrtTranslatorMock : public P4RuntimeBfrtTranslator {
 public:
  MOCK_METHOD2(PushChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD1(PushForwardingPipelineConfig,
               ::util::Status(const ::p4::config::v1::P4Info& p4info));
  MOCK_METHOD2(TranslateTableEntry,
               ::util::StatusOr<::p4::v1::TableEntry>(
                   const ::p4::v1::TableEntry& entry, bool to_sdk));
  MOCK_METHOD2(TranslateActionProfileMember,
               ::util::StatusOr<::p4::v1::ActionProfileMember>(
                   const ::p4::v1::ActionProfileMember& entry, bool to_sdk));
  MOCK_METHOD2(TranslateMeterEntry,
               ::util::StatusOr<::p4::v1::MeterEntry>(
                   const ::p4::v1::MeterEntry& entry, bool to_sdk));
  MOCK_METHOD2(TranslateDirectMeterEntry,
               ::util::StatusOr<::p4::v1::DirectMeterEntry>(
                   const ::p4::v1::DirectMeterEntry& entry, bool to_sdk));
  MOCK_METHOD2(TranslateCounterEntry,
               ::util::StatusOr<::p4::v1::CounterEntry>(
                   const ::p4::v1::CounterEntry& entry, bool to_sdk));
  MOCK_METHOD2(TranslateDirectCounterEntry,
               ::util::StatusOr<::p4::v1::DirectCounterEntry>(
                   const ::p4::v1::DirectCounterEntry& entry, bool to_sdk));
  MOCK_METHOD2(TranslateRegisterEntry,
               ::util::StatusOr<::p4::v1::RegisterEntry>(
                   const ::p4::v1::RegisterEntry& entry, bool to_sdk));
  MOCK_METHOD2(TranslatePacketReplicationEngineEntry,
               ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(
                   const ::p4::v1::PacketReplicationEngineEntry& entry,
                   bool to_sdk));
  MOCK_METHOD4(TranslateValue,
               ::util::StatusOr<std::string>(const std::string& value,
                                             const std::string& uri,
                                             bool to_sdk, int32 bit_width));
  MOCK_METHOD3(TranslateTnaPortId,
               ::util::StatusOr<std::string>(const std::string& value,
                                             bool to_sdk, int32 bit_width));
  MOCK_METHOD0(TranslationEnabled, bool());
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_P4RUNTIME_BFRT_TRANSLATOR_MOCK_H_
