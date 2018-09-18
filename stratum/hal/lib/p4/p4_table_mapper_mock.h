/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


// This is a mock implementation of P4TableMapper.

#ifndef STRATUM_HAL_LIB_P4_P4_TABLE_MAPPER_MOCK_H_
#define STRATUM_HAL_LIB_P4_P4_TABLE_MAPPER_MOCK_H_

#include "stratum/hal/lib/p4/p4_table_mapper.h"
#include "testing/base/public/gmock.h"

namespace stratum {
namespace hal {

class P4TableMapperMock : public P4TableMapper {
 public:
  MOCK_METHOD2(PushChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD2(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD1(
      PushForwardingPipelineConfig,
      ::util::Status(const ::p4::v1::ForwardingPipelineConfig& config));
  MOCK_METHOD1(
      VerifyForwardingPipelineConfig,
      ::util::Status(const ::p4::v1::ForwardingPipelineConfig& config));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_CONST_METHOD3(MapFlowEntry,
                     ::util::Status(const ::p4::v1::TableEntry& table_entry,
                                    ::p4::v1::Update::Type update_type,
                                    CommonFlowEntry* flow_entry));
  MOCK_CONST_METHOD2(MapActionProfileMember,
                     ::util::Status(const ::p4::v1::ActionProfileMember& member,
                                    MappedAction* mapped_action));
  MOCK_CONST_METHOD2(MapActionProfileGroup,
                     ::util::Status(const ::p4::v1::ActionProfileGroup& group,
                                    MappedAction* mapped_action));
  MOCK_CONST_METHOD2(
      DeparsePacketInMetadata,
      ::util::Status(const MappedPacketMetadata& mapped_packet_metadata,
                     ::p4::v1::PacketMetadata* p4_packet_metadata));
  MOCK_CONST_METHOD2(
      ParsePacketOutMetadata,
      ::util::Status(const ::p4::v1::PacketMetadata& p4_packet_metadata,
                     MappedPacketMetadata* mapped_packet_metadata));
  MOCK_CONST_METHOD2(
      DeparsePacketOutMetadata,
      ::util::Status(const MappedPacketMetadata& mapped_packet_metadata,
                     ::p4::v1::PacketMetadata* p4_packet_metadata));
  MOCK_CONST_METHOD2(
      ParsePacketInMetadata,
      ::util::Status(const ::p4::v1::PacketMetadata& p4_packet_metadata,
                     MappedPacketMetadata* mapped_packet_metadata));
  MOCK_CONST_METHOD3(MapMatchField,
                     ::util::Status(int table_id, uint32 field_id,
                                    MappedField* mapped_field));
  MOCK_CONST_METHOD2(LookupTable,
                     ::util::Status(int table_id,
                                    ::p4::config::v1::Table* table));
  MOCK_METHOD0(EnableStaticTableUpdates, void());
  MOCK_METHOD0(DisableStaticTableUpdates, void());
  MOCK_METHOD2(HandlePrePushStaticEntryChanges,
               ::util::Status(const ::p4::v1::WriteRequest& new_static_config,
                              ::p4::v1::WriteRequest* out_request));
  MOCK_METHOD2(HandlePostPushStaticEntryChanges,
               ::util::Status(const ::p4::v1::WriteRequest& new_static_config,
                              ::p4::v1::WriteRequest* out_request));
  MOCK_CONST_METHOD1(IsTableStageHidden, TriState(int table_id));
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_TABLE_MAPPER_MOCK_H_
