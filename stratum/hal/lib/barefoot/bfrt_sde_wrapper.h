// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_SDE_WRAPPER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_SDE_WRAPPER_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "bf_rt/bf_rt_init.hpp"
#include "bf_rt/bf_rt_session.hpp"
#include "bf_rt/bf_rt_table_key.hpp"
#include "pkt_mgr/pkt_mgr_intf.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bfrt.pb.h"
#include "stratum/hal/lib/barefoot/bfrt_sde_interface.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtSdeWrapper : public BfrtSdeInterface {
 public:
  ~BfrtSdeWrapper() override;

  // BfrtSdeInterface public methods.
  ::util::Status InitializeSdk(int device_id) override;
  ::util::Status StartLinkscan(int device_id) override;
  ::util::Status StopLinkscan(int device_id) override;
  void OnLinkscanEvent(int device_id, int port, PortState linkstatus) override;
  ::util::StatusOr<int> RegisterLinkscanEventWriter(
      std::unique_ptr<ChannelWriter<LinkscanEvent>> writer,
      int priority) override;
  ::util::Status UnregisterLinkscanEventWriter(int id) override;
  ::util::Status TxPacket(int device_id, const std::string packet) override;
  ::util::Status StartPacketIo(int device_id) override;
  ::util::Status StopPacketIo(int device_id) override;
  ::util::Status RegisterPacketReceiveWriter(
      int device_id,
      std::unique_ptr<ChannelWriter<std::string>> writer) override;
  ::util::Status UnregisterPacketReceiveWriter(int device_id) override;

  // BfrtSdeWrapper is neither copyable nor movable.
  BfrtSdeWrapper(const BfrtSdeWrapper&) = delete;
  BfrtSdeWrapper& operator=(const BfrtSdeWrapper&) = delete;

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static BfrtSdeWrapper* CreateSingleton() LOCKS_EXCLUDED(init_lock_);

  ::util::Status HandlePacketRx(bf_dev_id_t dev_id, bf_pkt* pkt,
                                bf_pkt_rx_ring_t rx_ring)
      LOCKS_EXCLUDED(packet_rx_callback_lock_);

 protected:
  // RW mutex lock for protecting the singleton instance initialization and
  // reading it back from other threads. Unlike other singleton classes, we
  // use RW lock as we need the pointer to class to be returned.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static BfrtSdeWrapper* singleton_ GUARDED_BY(init_lock_);

 private:
  explicit BfrtSdeWrapper();

  mutable absl::Mutex packet_rx_callback_lock_;

  absl::flat_hash_map<int, std::unique_ptr<ChannelWriter<std::string>>>
      device_id_to_packet_rx_writer_ GUARDED_BY(packet_rx_callback_lock_);

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

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_SDE_WRAPPER_H_