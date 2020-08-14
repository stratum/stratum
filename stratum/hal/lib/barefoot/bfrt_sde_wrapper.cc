// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_sde_wrapper.h"

#include "stratum/glue/gtl/cleanup.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {
namespace barefoot {

BfrtSdeWrapper* BfrtSdeWrapper::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex BfrtSdeWrapper::init_lock_(absl::kConstInit);

BfrtSdeWrapper::BfrtSdeWrapper() : device_id_to_packet_rx_writer_() {}

BfrtSdeWrapper::~BfrtSdeWrapper() {}

::util::Status BfrtSdeWrapper::InitializeSdk(int device_id) {
  if (!bf_pkt_is_inited(device_id)) {
    RETURN_IF_BFRT_ERROR(bf_pkt_init());
  }

  return ::util::OkStatus();
}
::util::Status BfrtSdeWrapper::StartLinkscan(int unit) {
  RETURN_ERROR(ERR_UNIMPLEMENTED)
      << "RegisterLinkscanEventWriter is not implemented.";
}

::util::Status BfrtSdeWrapper::StopLinkscan(int unit) {
  RETURN_ERROR(ERR_UNIMPLEMENTED)
      << "RegisterLinkscanEventWriter is not implemented.";
}

void BfrtSdeWrapper::OnLinkscanEvent(int unit, int port, PortState linkstatus) {
}

::util::StatusOr<int> BfrtSdeWrapper::RegisterLinkscanEventWriter(
    std::unique_ptr<ChannelWriter<LinkscanEvent>> writer, int priority) {
  RETURN_ERROR(ERR_UNIMPLEMENTED)
      << "RegisterLinkscanEventWriter is not implemented.";
}

::util::Status BfrtSdeWrapper::UnregisterLinkscanEventWriter(int id) {
  RETURN_ERROR(ERR_UNIMPLEMENTED)
      << "RegisterLinkscanEventWriter is not implemented.";
}

::util::Status BfrtSdeWrapper::TxPacket(int device_id,
                                        const std::string buffer) {
  bf_pkt* pkt = nullptr;
  RETURN_IF_BFRT_ERROR(
      bf_pkt_alloc(device_id, &pkt, buffer.size(), BF_DMA_CPU_PKT_TRANSMIT_0));
  auto pkt_cleaner =
      gtl::MakeCleanup([pkt, device_id]() { bf_pkt_free(device_id, pkt); });
  RETURN_IF_BFRT_ERROR(bf_pkt_data_copy(
      pkt, reinterpret_cast<const uint8*>(buffer.data()), buffer.size()));
  RETURN_IF_BFRT_ERROR(bf_pkt_tx(device_id, pkt, BF_PKT_TX_RING_0, pkt));
  pkt_cleaner.release();

  return ::util::OkStatus();
}

::util::Status BfrtSdeWrapper::StartPacketIo(int device_id) {
  for (int tx_ring = BF_PKT_TX_RING_0; tx_ring < BF_PKT_TX_RING_MAX;
       ++tx_ring) {
    RETURN_IF_BFRT_ERROR(bf_pkt_tx_done_notif_register(
        device_id, BfrtSdeWrapper::BfPktTxNotifyCallback,
        static_cast<bf_pkt_tx_ring_t>(tx_ring)));
  }

  for (int rx_ring = BF_PKT_RX_RING_0; rx_ring < BF_PKT_RX_RING_MAX;
       ++rx_ring) {
    RETURN_IF_BFRT_ERROR(
        bf_pkt_rx_register(device_id, BfrtSdeWrapper::BfPktRxNotifyCallback,
                           static_cast<bf_pkt_rx_ring_t>(rx_ring), this));
  }
  VLOG(1) << "Registered packetio callbacks on device " << device_id << ".";

  return ::util::OkStatus();
}

::util::Status BfrtSdeWrapper::StopPacketIo(int device_id) {
  for (int tx_ring = BF_PKT_TX_RING_0; tx_ring < BF_PKT_TX_RING_MAX;
       ++tx_ring) {
    RETURN_IF_BFRT_ERROR(bf_pkt_tx_done_notif_deregister(
        device_id, static_cast<bf_pkt_tx_ring_t>(tx_ring)));
  }

  for (int rx_ring = BF_PKT_RX_RING_0; rx_ring < BF_PKT_RX_RING_MAX;
       ++rx_ring) {
    RETURN_IF_BFRT_ERROR(bf_pkt_rx_deregister(
        device_id, static_cast<bf_pkt_rx_ring_t>(rx_ring)));
  }
  VLOG(1) << "Unregistered packetio callbacks on device " << device_id << ".";

  return ::util::OkStatus();
}

::util::Status BfrtSdeWrapper::RegisterPacketReceiveWriter(
    int device_id, std::unique_ptr<ChannelWriter<std::string>> writer) {
  absl::WriterMutexLock l(&packet_rx_callback_lock_);
  device_id_to_packet_rx_writer_[device_id] = std::move(writer);
  return ::util::OkStatus();
}

::util::Status BfrtSdeWrapper::UnregisterPacketReceiveWriter(int device_id) {
  absl::WriterMutexLock l(&packet_rx_callback_lock_);
  device_id_to_packet_rx_writer_.erase(device_id);
  return ::util::OkStatus();
}

BfrtSdeWrapper* BfrtSdeWrapper::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new BfrtSdeWrapper();
  }

  return singleton_;
}

::util::Status BfrtSdeWrapper::HandlePacketRx(bf_dev_id_t device_id,
                                              bf_pkt* pkt,
                                              bf_pkt_rx_ring_t rx_ring) {
  absl::ReaderMutexLock l(&packet_rx_callback_lock_);
  auto rx_writer = gtl::FindOrNull(device_id_to_packet_rx_writer_, device_id);
  CHECK_RETURN_IF_FALSE(rx_writer)
      << "No Rx callback registered for device id " << device_id << ".";

  std::string buffer(reinterpret_cast<const char*>(bf_pkt_get_pkt_data(pkt)),
                     bf_pkt_get_pkt_size(pkt));
  if (!(*rx_writer)->TryWrite(buffer).ok()) {
    LOG_EVERY_N(INFO, 500) << "Dropped packet received from CPU.";
  }

  VLOG(1) << "Received packet from CPU " << buffer.size() << " bytes "
          << StringToHex(buffer);

  return ::util::OkStatus();
}

bf_status_t BfrtSdeWrapper::BfPktTxNotifyCallback(bf_dev_id_t dev_id,
                                                  bf_pkt_tx_ring_t tx_ring,
                                                  uint64 tx_cookie,
                                                  uint32 status) {
  VLOG(1) << "Tx done notification for device_id: " << dev_id
          << " tx ring: " << tx_ring << " tx cookie: " << tx_cookie
          << " status: " << status;

  bf_pkt* pkt = reinterpret_cast<bf_pkt*>(tx_cookie);
  return bf_pkt_free(dev_id, pkt);
}

bf_status_t BfrtSdeWrapper::BfPktRxNotifyCallback(bf_dev_id_t dev_id,
                                                  bf_pkt* pkt, void* cookie,
                                                  bf_pkt_rx_ring_t rx_ring) {
  static_cast<BfrtSdeWrapper*>(cookie)->HandlePacketRx(dev_id, pkt, rx_ring);
  return bf_pkt_free(dev_id, pkt);
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
