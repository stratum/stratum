// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_TABLE_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_TABLE_MANAGER_MOCK_H_

#include <memory>

#include "gmock/gmock.h"
#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtTableManagerMock : public BfrtTableManager {
 public:
  // MOCK_METHOD1(PushChassisConfig,
  //              ::util::Status(const BfrtDeviceConfig& config, uint64
  //              node_id));
  // MOCK_METHOD2(VerifyChassisConfig,
  //              ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD1(PushForwardingPipelineConfig,
               ::util::Status(const BfrtDeviceConfig& config));
  MOCK_CONST_METHOD1(
      VerifyForwardingPipelineConfig,
      ::util::Status(const ::p4::v1::ForwardingPipelineConfig& config));
  MOCK_METHOD3(
      WriteTableEntry,
      ::util::Status(std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const ::p4::v1::Update::Type type,
                     const ::p4::v1::TableEntry& table_entry));
  MOCK_METHOD3(
      ReadTableEntry,
      ::util::Status(std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const ::p4::v1::TableEntry& table_entry,
                     WriterInterface<::p4::v1::ReadResponse>* writer));
  MOCK_METHOD3(
      WriteDirectCounterEntry,
      ::util::Status(std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const ::p4::v1::Update::Type type,
                     const ::p4::v1::DirectCounterEntry& direct_counter_entry));

  MOCK_METHOD3(
      WriteRegisterEntry,
      ::util::Status(std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const ::p4::v1::Update::Type type,
                     const ::p4::v1::RegisterEntry& register_entry));
  MOCK_METHOD3(
      WriteMeterEntry,
      ::util::Status(std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const ::p4::v1::Update::Type type,
                     const ::p4::v1::MeterEntry& meter_entry));
  MOCK_METHOD3(WriteActionProfileMember,
               ::util::Status(
                   std::shared_ptr<BfSdeInterface::SessionInterface> session,
                   const ::p4::v1::Update::Type type,
                   const ::p4::v1::ActionProfileMember& action_profile_member));
  MOCK_METHOD3(
      ReadActionProfileMember,
      ::util::Status(std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const ::p4::v1::ActionProfileMember& action_profile_member,
                     WriterInterface<::p4::v1::ReadResponse>* writer));
  MOCK_METHOD3(
      WriteActionProfileGroup,
      ::util::Status(std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const ::p4::v1::Update::Type type,
                     const ::p4::v1::ActionProfileGroup& action_profile_group));
  MOCK_METHOD3(
      ReadActionProfileGroup,
      ::util::Status(std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const ::p4::v1::ActionProfileGroup& action_profile_group,
                     WriterInterface<::p4::v1::ReadResponse>* writer));
  MOCK_METHOD2(ReadDirectCounterEntry,
               ::util::StatusOr<::p4::v1::DirectCounterEntry>(
                   std::shared_ptr<BfSdeInterface::SessionInterface> session,
                   const ::p4::v1::DirectCounterEntry& direct_counter_entry));
  MOCK_METHOD3(
      ReadRegisterEntry,
      ::util::Status(std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const ::p4::v1::RegisterEntry& register_entry,
                     WriterInterface<::p4::v1::ReadResponse>* writer));
  MOCK_METHOD3(
      ReadMeterEntry,
      ::util::Status(std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const ::p4::v1::MeterEntry& meter_entry,
                     WriterInterface<::p4::v1::ReadResponse>* writer));
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_TABLE_MANAGER_MOCK_H_
