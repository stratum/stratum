// Copyright 2019-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_SDE_MOCK_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_SDE_MOCK_H_

#include <memory>

#include "gmock/gmock.h"
#include "stratum/hal/lib/barefoot/bf_sde_interface.h"

namespace stratum {
namespace hal {
namespace barefoot {

class SessionMock : public BfSdeInterface::SessionInterface {
 public:
  MOCK_METHOD0(BeginBatch, ::util::Status());
  MOCK_METHOD0(EndBatch, ::util::Status());
};

class BfSdeMock : public BfSdeInterface {
 public:
  MOCK_METHOD2(AddDevice,
               ::util::Status(int device,
                              const BfrtDeviceConfig& device_config));
  MOCK_METHOD0(CreateSession,
               ::util::StatusOr<std::shared_ptr<SessionInterface>>());
  MOCK_METHOD2(GetPortState, ::util::StatusOr<PortState>(int device, int port));
  MOCK_METHOD3(GetPortCounters,
               ::util::Status(int device, int port, PortCounters* counters));
  MOCK_METHOD1(
      RegisterPortStatusEventWriter,
      ::util::Status(std::unique_ptr<ChannelWriter<PortStatusEvent>> writer));
  MOCK_METHOD0(UnregisterPortStatusEventWriter, ::util::Status());
  MOCK_METHOD4(AddPort, ::util::Status(int device, int port, uint64 speed_bps,
                                       FecMode fec_mode));
  MOCK_METHOD2(DeletePort, ::util::Status(int device, int port));
  MOCK_METHOD2(EnablePort, ::util::Status(int device, int port));
  MOCK_METHOD2(DisablePort, ::util::Status(int device, int port));
  MOCK_METHOD3(SetPortAutonegPolicy,
               ::util::Status(int device, int port, TriState autoneg));
  MOCK_METHOD3(SetPortMtu, ::util::Status(int device, int port, int32 mtu));
  MOCK_METHOD2(IsValidPort, bool(int device, int port));
  MOCK_METHOD3(SetPortLoopbackMode,
               ::util::Status(int device, int port,
                              LoopbackState loopback_mode));
  MOCK_METHOD2(GetPortIdFromPortKey,
               ::util::StatusOr<uint32>(int device, const PortKey& port_key));
  MOCK_METHOD1(GetPcieCpuPort, ::util::StatusOr<int>(int device));
  MOCK_METHOD2(SetTmCpuPort, ::util::Status(int device, int port));
  MOCK_METHOD1(IsSoftwareModel, ::util::StatusOr<bool>(int device));
  MOCK_METHOD2(TxPacket, ::util::Status(int device, const std::string& packet));
  MOCK_METHOD1(StartPacketIo, ::util::Status(int device));
  MOCK_METHOD1(StopPacketIo, ::util::Status(int device));
  MOCK_METHOD2(
      RegisterPacketReceiveWriter,
      ::util::Status(int device,
                     std::unique_ptr<ChannelWriter<std::string>> writer));
  MOCK_METHOD1(UnregisterPacketReceiveWriter, ::util::Status(int device));
  MOCK_METHOD5(CreateMulticastNode,
               ::util::StatusOr<uint32>(
                   int device,
                   std::shared_ptr<BfSdeInterface::SessionInterface> session,
                   int mc_replication_id, const std::vector<uint32>& mc_lag_ids,
                   const std::vector<uint32> ports));
  MOCK_METHOD3(
      DeleteMulticastNodes,
      ::util::Status(int device,
                     std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     const std::vector<uint32>& mc_node_ids));
  MOCK_METHOD6(
      GetMulticastNode,
      ::util::Status(int device,
                     std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     uint32 mc_node_id, int* replication_id,
                     std::vector<uint32>* lag_ids, std::vector<uint32>* ports));
  MOCK_METHOD4(
      InsertMulticastGroup,
      ::util::Status(int device,
                     std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     uint32 group_id, const std::vector<uint32>& mc_node_ids));
  MOCK_METHOD4(
      ModifyMulticastGroup,
      ::util::Status(int device,
                     std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     uint32 group_id, const std::vector<uint32>& mc_node_ids));
  MOCK_METHOD3(DeleteMulticastGroup,
               ::util::Status(int device,
                              std::shared_ptr<SessionInterface> session,
                              uint32 group_id));
  MOCK_METHOD5(
      GetMulticastGroups,
      ::util::Status(int device,
                     std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     uint32 group_id, std::vector<uint32>* group_ids,
                     std::vector<std::vector<uint32>>* mc_node_ids));
  MOCK_METHOD6(
      InsertCloneSession,
      ::util::Status(int device,
                     std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     uint32 session_id, int egress_port, int cos,
                     int max_pkt_len));
  MOCK_METHOD6(
      ModifyCloneSession,
      ::util::Status(int device,
                     std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     uint32 session_id, int egress_port, int cos,
                     int max_pkt_len));
  MOCK_METHOD3(GetNodesInMulticastGroup,
               ::util::StatusOr<std::vector<uint32>>(
                   int device,
                   std::shared_ptr<BfSdeInterface::SessionInterface> session,
                   uint32 group_id));
  MOCK_METHOD3(
      DeleteCloneSession,
      ::util::Status(int device,
                     std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     uint32 session_id));
  MOCK_METHOD7(
      GetCloneSessions,
      ::util::Status(int device,
                     std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     uint32 session_id, std::vector<uint32>* session_ids,
                     std::vector<int>* egress_ports, std::vector<int>* coss,
                     std::vector<int>* max_pkt_lens));
  MOCK_METHOD6(
      WriteIndirectCounter,
      ::util::Status(int device,
                     std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     uint32 counter_id, int counter_index,
                     absl::optional<uint64> byte_count,
                     absl::optional<uint64> packet_count));

  MOCK_METHOD7(
      ReadIndirectCounter,
      ::util::Status(int device,
                     std::shared_ptr<BfSdeInterface::SessionInterface> session,
                     uint32 counter_id, int counter_index,
                     absl::optional<uint64>* byte_count,
                     absl::optional<uint64>* packet_count,
                     absl::Duration timeout));
  MOCK_CONST_METHOD1(GetBfRtId, ::util::StatusOr<uint32>(uint32 p4info_id));
  MOCK_CONST_METHOD1(GetP4InfoId, ::util::StatusOr<uint32>(uint32 bfrt_id));
  MOCK_CONST_METHOD1(GetActionSelectorBfRtId,
               ::util::StatusOr<uint32>(uint32 action_profile_id));
  MOCK_CONST_METHOD1(GetActionProfileBfRtId,
               ::util::StatusOr<uint32>(uint32 action_selector_id));
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_SDE_MOCK_H_
