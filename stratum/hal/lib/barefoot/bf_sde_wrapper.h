// Copyright 2019-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_SDE_WRAPPER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_SDE_WRAPPER_H_

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "bf_rt/bf_rt_init.hpp"
#include "pkt_mgr/pkt_mgr_intf.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bf_sde_interface.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/channel/channel.h"

namespace stratum {
namespace hal {
namespace barefoot {

// The "BfSdeWrapper" is an implementation of BfSdeInterface which is used
// on real hardware to talk to Tofino ASIC.
class BfSdeWrapper : public BfSdeInterface {
 public:
  // Default MTU for ports on Tofino.
  static constexpr int32 kBfDefaultMtu = 10 * 1024;  // 10K

  // BfSdeInterface public methods.
  ::util::Status AddDevice(int device,
                           const BfrtDeviceConfig& device_config) override;
  ::util::StatusOr<PortState> GetPortState(int device, int port) override;
  ::util::Status GetPortCounters(int device, int port,
                                 PortCounters* counters) override;
  ::util::Status RegisterPortStatusEventWriter(
      std::unique_ptr<ChannelWriter<PortStatusEvent>> writer) override
      LOCKS_EXCLUDED(port_status_event_writer_lock_);
  ::util::Status UnregisterPortStatusEventWriter() override
      LOCKS_EXCLUDED(port_status_event_writer_lock_);
  ::util::Status AddPort(int device, int port, uint64 speed_bps,
                         FecMode fec_mode) override;
  ::util::Status DeletePort(int device, int port) override;
  ::util::Status EnablePort(int device, int port) override;
  ::util::Status DisablePort(int device, int port) override;
  ::util::Status SetPortAutonegPolicy(int device, int port,
                                      TriState autoneg) override;
  ::util::Status SetPortMtu(int device, int port, int32 mtu) override;
  bool IsValidPort(int device, int port) override;
  ::util::Status SetPortLoopbackMode(int uint, int port,
                                     LoopbackState loopback_mode) override;
  ::util::StatusOr<uint32> GetPortIdFromPortKey(
      int device, const PortKey& port_key) override;
  ::util::StatusOr<int> GetPcieCpuPort(int device) override;
  ::util::Status SetTmCpuPort(int device, int port) override;
  ::util::StatusOr<bool> IsSoftwareModel(int device) override;
  ::util::Status TxPacket(int device, const std::string& packet) override;
  ::util::Status StartPacketIo(int device) override;
  ::util::Status StopPacketIo(int device) override;
  ::util::Status RegisterPacketReceiveWriter(
      int device, std::unique_ptr<ChannelWriter<std::string>> writer) override;
  ::util::Status UnregisterPacketReceiveWriter(int device) override;

  //
  ::util::Status HandlePacketRx(bf_dev_id_t dev_id, bf_pkt* pkt,
                                bf_pkt_rx_ring_t rx_ring)
      LOCKS_EXCLUDED(packet_rx_callback_lock_);

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static BfSdeWrapper* CreateSingleton() LOCKS_EXCLUDED(init_lock_);

  // The following public functions are specific to this class. They are to be
  // called by SDE callbacks only.

  // Return the singleton instance to be used in the SDE callbacks.
  static BfSdeWrapper* GetSingleton() LOCKS_EXCLUDED(init_lock_);

  // Called whenever a port status event is received from SDK. It forwards the
  // port status event to the module who registered a callback by calling
  // RegisterPortStatusEventWriter().
  ::util::Status OnPortStatusEvent(int dev_id, int dev_port, bool up)
      LOCKS_EXCLUDED(port_status_event_writer_lock_);

  // BfSdeWrapper is neither copyable nor movable.
  BfSdeWrapper(const BfSdeWrapper&) = delete;
  BfSdeWrapper& operator=(const BfSdeWrapper&) = delete;
  BfSdeWrapper(BfSdeWrapper&&) = delete;
  BfSdeWrapper& operator=(BfSdeWrapper&&) = delete;

 protected:
  // RW mutex lock for protecting the singleton instance initialization and
  // reading it back from other threads. Unlike other singleton classes, we
  // use RW lock as we need the pointer to class to be returned.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static BfSdeWrapper* singleton_ GUARDED_BY(init_lock_);

 private:
  // Timeout for Write() operations on port status events.
  static constexpr absl::Duration kWriteTimeout = absl::InfiniteDuration();

  // Private constructor, use CreateSingleton and GetSingleton().
  BfSdeWrapper();

  // RM Mutex to protect the port status writer.
  mutable absl::Mutex port_status_event_writer_lock_;

  // Mutex protecting the packet rx writer map.
  mutable absl::Mutex packet_rx_callback_lock_;

  // RW mutex lock for protecting the pipeline state.
  mutable absl::Mutex data_lock_;

  // Writer to forward the port status change message to. It is registered by
  // chassis manager to receive SDE port status change events.
  std::unique_ptr<ChannelWriter<PortStatusEvent>> port_status_event_writer_
      GUARDED_BY(port_status_event_writer_lock_);

  // Map from device ID to packet receive writer.
  absl::flat_hash_map<int, std::unique_ptr<ChannelWriter<std::string>>>
      device_to_packet_rx_writer_ GUARDED_BY(packet_rx_callback_lock_);

  // Pointer to the current BfR info object. Not owned by this class.
  const bfrt::BfRtInfo* bfrt_info_ GUARDED_BY(data_lock_);

  // Pointer to the bfrt device manager. Not owned by this class.
  bfrt::BfRtDevMgr* bfrt_device_manager_ GUARDED_BY(data_lock_);

  // Callback registed with the SDE for Tx notifications.
  static bf_status_t BfPktTxNotifyCallback(bf_dev_id_t dev_id,
                                           bf_pkt_tx_ring_t tx_ring,
                                           uint64 tx_cookie, uint32 status);

  // Callback registed with the SDE for Rx notifications.
  static bf_status_t BfPktRxNotifyCallback(bf_dev_id_t dev_id, bf_pkt* pkt,
                                           void* cookie,
                                           bf_pkt_rx_ring_t rx_ring);
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_SDE_WRAPPER_H_
