// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_TDI_PACKETIO_MANAGER_H_
#define STRATUM_HAL_LIB_TDI_TDI_PACKETIO_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/tdi/tdi.pb.h"
#include "stratum/hal/lib/tdi/tdi_sde_interface.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace tdi {

class TdiPacketioManager {
 public:
  virtual ~TdiPacketioManager();

  // Pushes the parts of the given ChassisConfig proto that this class cares
  // about. If the class is not initialized (i.e. if config is pushed for the
  // first time), this function also initializes class.
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config,
                                           uint64 node_id)
      LOCKS_EXCLUDED(data_lock_);

  // Verifies the parts of ChassisConfig proto that this class cares about.
  // The given node_id is used to understand which part of the ChassisConfig is
  // intended for this class.
  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id);

  // Pushes the forwarding pipeline to this class. If this is the first time, it
  // will also set up the necessary callbacks for packet IO.
  virtual ::util::Status PushForwardingPipelineConfig(
      const TdiDeviceConfig& config) LOCKS_EXCLUDED(data_lock_);

  // Performs coldboot shutdown. Note that there is no public Initialize().
  // Initialization is done as part of PushChassisConfig() if the class is not
  // initialized by the time we push config.
  virtual ::util::Status Shutdown() LOCKS_EXCLUDED(data_lock_);

  // Registers a writer to be invoked when we capture a packet on a PCIe
  // interface.
  virtual ::util::Status RegisterPacketReceiveWriter(
      const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer)
      LOCKS_EXCLUDED(rx_writer_lock_);

  virtual ::util::Status UnregisterPacketReceiveWriter()
      LOCKS_EXCLUDED(rx_writer_lock_);

  // Transmits a packet to the PCIe interface.
  virtual ::util::Status TransmitPacket(const ::p4::v1::PacketOut& packet)
      LOCKS_EXCLUDED(data_lock_);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<TdiPacketioManager> CreateInstance(
      TdiSdeInterface* tdi_sde_interface, int device);

  // TdiPacketioManager is neither copyable nor movable.
  TdiPacketioManager(const TdiPacketioManager&) = delete;
  TdiPacketioManager& operator=(const TdiPacketioManager&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  TdiPacketioManager();

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  explicit TdiPacketioManager(TdiSdeInterface* tdi_sde_interface, int device);

  // Builds the packet header structure for controller packets.
  ::util::Status BuildMetadataMapping(const p4::config::v1::P4Info& p4_info)
      EXCLUSIVE_LOCKS_REQUIRED(data_lock_);

  // Deparses a PacketOut into the buffer by serializing the metadata fields in
  // front of the payload.
  ::util::Status DeparsePacketOut(const ::p4::v1::PacketOut& packet,
                                  std::string* buffer)
      LOCKS_EXCLUDED(data_lock_);

  // Parses a binary string into a PacketIn, filling the metadata fields.
  ::util::Status ParsePacketIn(const std::string& buffer,
                               ::p4::v1::PacketIn* packet)
      LOCKS_EXCLUDED(data_lock_);

  // Handles a received packets and hands it over the registered receive writer.
  ::util::Status HandleSdePacketRx()
      LOCKS_EXCLUDED(data_lock_, rx_writer_lock_);

  // SDE cpu interface RX thread function.
  static void* SdeRxThreadFunc(void* arg);

  // Mutex lock for protecting rx_writer_.
  mutable absl::Mutex rx_writer_lock_;

  // Mutex lock to protect the metadata mappings.
  mutable absl::Mutex data_lock_;

  // Initialized to false, set once only on first PushForwardingPipelineConfig.
  bool initialized_ GUARDED_BY(data_lock_);

  // Stores the registered writer for PacketIns.
  std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> rx_writer_
      GUARDED_BY(rx_writer_lock_);

  // List of metadata id and bitwidth pairs. Stores the size and structure of
  // the CPU packet headers.
  std::vector<std::pair<uint32, int>> packetin_header_ GUARDED_BY(data_lock_);
  std::vector<std::pair<uint32, int>> packetout_header_ GUARDED_BY(data_lock_);
  size_t packetin_header_size_ GUARDED_BY(data_lock_);
  size_t packetout_header_size_ GUARDED_BY(data_lock_);

  // Buffer channel for packets coming from the SDE to this manager.
  std::shared_ptr<Channel<std::string>> packet_receive_channel_
      GUARDED_BY(data_lock_);

  // The ID of the RX thread which handles receiving packets from the SDE.
  pthread_t sde_rx_thread_id_ GUARDED_BY(data_lock_);

  // Pointer to a TdiSdeInterface implementation that wraps all the SDE calls.
  TdiSdeInterface* tdi_sde_interface_ = nullptr;  // not owned by this class.

  // Fixed zero-based Tofino device number corresponding to the node/ASIC
  // managed by this class instance. Assigned in the class constructor.
  const int device_;

  friend class TdiPacketioManagerTest;
};

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_PACKETIO_MANAGER_H_
