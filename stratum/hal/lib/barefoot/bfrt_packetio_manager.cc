// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_packetio_manager.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

#include <deque>

#include "stratum/glue/gtl/map_util.h"
#include "stratum/hal/lib/barefoot/macros.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/utils.h"

extern "C" {
#include "pkt_mgr/pkt_mgr_intf.h"
#include "tofino/bf_pal/dev_intf.h"
}

namespace stratum {
namespace hal {
namespace barefoot {

BfrtPacketioManager::BfrtPacketioManager(int device_id)
    : device_id_(device_id) {}

BfrtPacketioManager::~BfrtPacketioManager() {}

std::unique_ptr<BfrtPacketioManager> BfrtPacketioManager::CreateInstance(
    int device_id) {
  return absl::WrapUnique(new BfrtPacketioManager(device_id));
}

::util::Status BfrtPacketioManager::PushChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  return ::util::OkStatus();
}

// TODO(max): handle multiple pushes
::util::Status BfrtPacketioManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config) {
  CHECK_RETURN_IF_FALSE(config.programs_size() == 1);
  const auto& program = config.programs(0);

  RETURN_IF_ERROR(BuildMetadataMapping(program.p4info()));

  for (int tx_ring = BF_PKT_TX_RING_0; tx_ring < BF_PKT_TX_RING_MAX;
       tx_ring++) {
    RETURN_IF_BFRT_ERROR(bf_pkt_tx_done_notif_register(
        device_id_, BfrtPacketioManager::BfPktTxNotifyCallback,
        static_cast<bf_pkt_tx_ring_t>(tx_ring)));
  }

  for (int rx_ring = BF_PKT_RX_RING_0; rx_ring < BF_PKT_RX_RING_MAX;
       ++rx_ring) {
    RETURN_IF_BFRT_ERROR(bf_pkt_rx_register(
        device_id_, BfrtPacketioManager::BfPktRxNotifyCallback,
        static_cast<bf_pkt_rx_ring_t>(rx_ring), this));
  }

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::VerifyChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  return ::util::OkStatus();
}

util::Status BfrtPacketioManager::Shutdown() {
  RETURN_IF_BFRT_ERROR(
      bf_pkt_tx_done_notif_deregister(device_id_, BF_PKT_TX_RING_0));

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::RegisterPacketReceiveWriter(
    const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer) {
  absl::WriterMutexLock l(&rx_writer_lock_);
  rx_writer_ = writer;
  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::UnregisterPacketReceiveWriter() {
  absl::WriterMutexLock l(&rx_writer_lock_);
  rx_writer_ = nullptr;
  return ::util::OkStatus();
}

namespace {
// BitBuffer implements a helper class for parsing and deparsing packet headers.
class BitBuffer {
 public:
  BitBuffer() = default;
  BitBuffer(const uint8* buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      AddField(sizeof(buf[i]) * 8, buf[i]);
    }
  }

  // Add a field to the back of the buffer.
  template <typename U>
  ::util::Status AddField(size_t bitwidth, U value) {
    const int kMaxBitWidth = sizeof(U) * 8;
    CHECK_RETURN_IF_FALSE(bitwidth <= kMaxBitWidth);
    for (int bit = bitwidth - 1; bit >= 0; --bit) {
      bits_.push_back((value >> bit) & 1);
    }
    return ::util::OkStatus();
  }

  // Removes and returns a field from the front of the buffer.
  std::string PopField(size_t bitwidth) {
    CHECK_LE(bitwidth, bits_.size());
    std::string out;
    uint8 byte_val = 0;
    for (int bit = bitwidth - 1; bit >= 0; --bit) {
      byte_val <<= 1;
      byte_val |= bits_.front();
      bits_.pop_front();
      if (!(bit % 8)) {
        out.push_back(byte_val);
        byte_val = 0;
      }
    }
    return out;
  }

  // Returns and empties the entire buffer.
  std::string PopField(void) { return PopField(bits_.size()); }

  // Returns a human readable representation of the buffer.
  std::string ToString() {
    std::string ret;
    for (size_t i = 0; i < bits_.size(); ++i) {
      if (i % 8 == 0) ret += " ";
      ret += std::to_string(static_cast<uint16>(bits_[i]));
    }
    return ret;
  }

 private:
  std::deque<uint8> bits_;
};
}  // namespace

::util::Status BfrtPacketioManager::TransmitPacket(
    const ::p4::v1::PacketOut& packet) {
  std::vector<char> buf;
  BitBuffer bit_buf;

  for (const auto& p : packetout_header_) {
    const auto id = p.first;
    const auto bitwidth = p.second;

    auto it = std::find_if(packet.metadata().begin(), packet.metadata().end(),
                           [&id](::p4::v1::PacketMetadata metadata) {
                             return metadata.metadata_id() == id;
                           });
    CHECK_RETURN_IF_FALSE(it != packet.metadata().end())
        << "Missing metadata with Id " << id << " in PacketOut "
        << packet.ShortDebugString();

    auto v = ByteStreamToUint<uint64>(it->value());
    VLOG(1) << "Encoded metadata field with id " << id << " bitwidth "
            << bitwidth << " value 0x" << std::hex << v;
    bit_buf.AddField(bitwidth, v);
  }
  auto hdr_buf = bit_buf.PopField();
  buf.insert(buf.end(), hdr_buf.begin(), hdr_buf.end());
  buf.insert(buf.end(), packet.payload().begin(), packet.payload().end());

  bf_pkt* pkt = nullptr;
  bf_pkt_tx_ring_t tx_ring = BF_PKT_TX_RING_0;
  RETURN_IF_BFRT_ERROR(
      bf_pkt_alloc(device_id_, &pkt, buf.size(), BF_DMA_CPU_PKT_TRANSMIT_0));
  // // TODO: error case cleanup
  RETURN_IF_BFRT_ERROR(bf_pkt_data_copy(
      pkt, reinterpret_cast<const uint8_t*>(buf.data()), buf.size()));
  RETURN_IF_BFRT_ERROR(bf_pkt_tx(device_id_, pkt, tx_ring, pkt));

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::HandlePacketRx(bf_dev_id_t dev_id,
                                                   bf_pkt* pkt,
                                                   bf_pkt_rx_ring_t rx_ring) {
  ::p4::v1::PacketIn packet;

  BitBuffer bit_buf(bf_pkt_get_pkt_data(pkt), packetin_header_size_);
  for (const auto& p : packetin_header_) {
    auto metadata = packet.add_metadata();
    metadata->set_metadata_id(p.first);
    metadata->set_value(bit_buf.PopField(p.second));
  }
  packet.set_payload(bf_pkt_get_pkt_data(pkt) + packetin_header_size_,
                     bf_pkt_get_pkt_size(pkt) - packetin_header_size_);
  {
    absl::WriterMutexLock l(&rx_writer_lock_);
    rx_writer_->Write(packet);
  }
  VLOG(1) << "Handled packet in: " << packet.ShortDebugString();

  return ::util::OkStatus();
}

// This function is based on P4TableMapper and implements a subset of its
// functionality.
::util::Status BfrtPacketioManager::BuildMetadataMapping(
    const p4::config::v1::P4Info& p4_info) {
  size_t packetin_bits = 0;
  size_t packetout_bits = 0;
  for (const auto& controller_packet_metadata :
       p4_info.controller_packet_metadata()) {
    const std::string& name = controller_packet_metadata.preamble().name();
    if (name != kIngressMetadataPreambleName &&
        name != kEgressMetadataPreambleName) {
      LOG(WARNING) << "Skipped unknown metadata preamble: " << name << ".";
      continue;
    }
    // The order in the P4Info is representative of the actual header structure.
    for (const auto& metadata : controller_packet_metadata.metadata()) {
      uint32 id = metadata.id();
      int bitwidth = metadata.bitwidth();
      if (name == kIngressMetadataPreambleName) {
        packetin_header_.push_back(std::make_pair(id, bitwidth));
        packetin_bits += bitwidth;
      } else {
        packetout_header_.push_back(std::make_pair(id, bitwidth));
        packetout_bits += bitwidth;
      }
    }
  }

  CHECK_RETURN_IF_FALSE(packetin_bits % 8 == 0)
      << "PacketIn header size must be multiple of 8 bits.";
  CHECK_RETURN_IF_FALSE(packetout_bits % 8 == 0)
      << "PacketOut header size must be multiple of 8 bits.";
  packetin_header_size_ = packetin_bits / 8;
  packetout_header_size_ = packetout_bits / 8;

  return ::util::OkStatus();
}

bf_status_t BfrtPacketioManager::BfPktTxNotifyCallback(bf_dev_id_t dev_id,
                                                       bf_pkt_tx_ring_t tx_ring,
                                                       uint64_t tx_cookie,
                                                       uint32_t status) {
  VLOG(1) << "BfPktTxNotifyCallback:" << dev_id << ":" << tx_ring << ":"
          << tx_cookie << ":" << status;

  bf_pkt* pkt = reinterpret_cast<bf_pkt*>(tx_cookie);
  return bf_pkt_free(dev_id, pkt);
}

bf_status_t BfrtPacketioManager::BfPktRxNotifyCallback(
    bf_dev_id_t dev_id, bf_pkt* pkt, void* cookie, bf_pkt_rx_ring_t rx_ring) {
  static_cast<BfrtPacketioManager*>(cookie)->HandlePacketRx(dev_id, pkt,
                                                            rx_ring);
  return bf_pkt_free(dev_id, pkt);
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
