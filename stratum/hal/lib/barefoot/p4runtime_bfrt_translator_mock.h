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
  MOCK_METHOD1(PushChassisConfig, ::util::Status(const ChassisConfig& config));
  MOCK_METHOD1(
      PushForwardingPipelineConfig,
      ::util::Status(const ::p4::v1::ForwardingPipelineConfig& config));
  MOCK_METHOD2(TranslateTableEntry,
               ::util::StatusOr<::p4::v1::TableEntry>(
                   const ::p4::v1::TableEntry& entry, const bool& to_sdk));
  MOCK_METHOD2(TranslateActionProfileMember,
               ::util::StatusOr<::p4::v1::ActionProfileMember>(
                   const ::p4::v1::ActionProfileMember& entry,
                   const bool& to_sdk));
  MOCK_METHOD2(TranslateMeterEntry,
               ::util::StatusOr<::p4::v1::MeterEntry>(
                   const ::p4::v1::MeterEntry& entry, const bool& to_sdk));
  MOCK_METHOD2(TranslateDirectMeterEntry,
               ::util::StatusOr<::p4::v1::DirectMeterEntry>(
                   const ::p4::v1::DirectMeterEntry& entry,
                   const bool& to_sdk));
  MOCK_METHOD2(TranslateCounterEntry,
               ::util::StatusOr<::p4::v1::CounterEntry>(
                   const ::p4::v1::CounterEntry& entry, const bool& to_sdk));
  MOCK_METHOD2(TranslateDirectCounterEntry,
               ::util::StatusOr<::p4::v1::DirectCounterEntry>(
                   const ::p4::v1::DirectCounterEntry& entry,
                   const bool& to_sdk));
  MOCK_METHOD2(TranslateRegisterEntry,
               ::util::StatusOr<::p4::v1::RegisterEntry>(
                   const ::p4::v1::RegisterEntry& entry, const bool& to_sdk));
  MOCK_METHOD2(TranslatePacketReplicationEngineEntry,
               ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(
                   const ::p4::v1::PacketReplicationEngineEntry& entry,
                   const bool& to_sdk));
  MOCK_METHOD4(TranslateValue,
               ::util::StatusOr<std::string>(const std::string& value,
                                             const std::string& uri,
                                             const bool& to_sdk,
                                             const int32& bit_width));
  MOCK_METHOD3(TranslateTnaPortId,
               ::util::StatusOr<std::string>(const std::string& value,
                                             const bool& to_sdk,
                                             const int32& bit_width));
  MOCK_METHOD0(TranslationEnabled, bool());
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_P4RUNTIME_BFRT_TRANSLATOR_MOCK_H_
