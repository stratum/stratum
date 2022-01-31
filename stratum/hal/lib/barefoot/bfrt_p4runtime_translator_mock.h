// Copyright 2022-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_P4RUNTIME_TRANSLATOR_MOCK_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_P4RUNTIME_TRANSLATOR_MOCK_H_

#include <string>

#include "gmock/gmock.h"
#include "stratum/hal/lib/barefoot/bfrt_p4runtime_translator.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtP4RuntimeTranslatorMock : public BfrtP4RuntimeTranslator {
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
  MOCK_METHOD1(TranslatePacketIn, ::util::StatusOr<::p4::v1::PacketIn>(
                                      const ::p4::v1::PacketIn& packet_in));
  MOCK_METHOD1(TranslatePacketOut, ::util::StatusOr<::p4::v1::PacketOut>(
                                       const ::p4::v1::PacketOut& packet_out));
  MOCK_METHOD1(TranslateP4Info, ::util::StatusOr<::p4::config::v1::P4Info>(
                                    const ::p4::config::v1::P4Info& p4info));
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_P4RUNTIME_TRANSLATOR_MOCK_H_
