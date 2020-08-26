// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_packetio_manager.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

#include <deque>
#include <string>

#include "stratum/glue/gtl/cleanup.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/hal/lib/barefoot/macros.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

BfrtPacketioManager::BfrtPacketioManager(int device_id)
    : packetin_header_(),
      packetout_header_(),
      packetin_header_size_(),
      packetout_header_size_(),
      initialized_(false),
      device_id_(device_id) {}

BfrtPacketioManager::~BfrtPacketioManager() {}

std::unique_ptr<BfrtPacketioManager> BfrtPacketioManager::CreateInstance(
    int device_id) {
  return absl::WrapUnique(new BfrtPacketioManager(device_id));
}

::util::Status BfrtPacketioManager::PushChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  if (!bf_pkt_is_inited(device_id_)) {
    RETURN_IF_BFRT_ERROR(bf_pkt_init());
  }

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config) {
  CHECK_RETURN_IF_FALSE(config.programs_size() == 1)
      << "Only one program is supported.";
  const auto& program = config.programs(0);
  {
    absl::WriterMutexLock l(&data_lock_);
    RETURN_IF_ERROR(BuildMetadataMapping(program.p4info()));
    // PushForwardingPipelineConfig resets the bf_pkt driver.
    RETURN_IF_ERROR(StartIo());
    initialized_ = true;
  }

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::VerifyChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::StartIo() {
  for (int tx_ring = BF_PKT_TX_RING_0; tx_ring < BF_PKT_TX_RING_MAX;
       ++tx_ring) {
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
  VLOG(1) << "Registered packetio callbacks on device " << device_id_ << ".";

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::StopIo() {
  for (int tx_ring = BF_PKT_TX_RING_0; tx_ring < BF_PKT_TX_RING_MAX;
       ++tx_ring) {
    RETURN_IF_BFRT_ERROR(bf_pkt_tx_done_notif_deregister(
        device_id_, static_cast<bf_pkt_tx_ring_t>(tx_ring)));
  }

  for (int rx_ring = BF_PKT_RX_RING_0; rx_ring < BF_PKT_RX_RING_MAX;
       ++rx_ring) {
    RETURN_IF_BFRT_ERROR(bf_pkt_rx_deregister(
        device_id_, static_cast<bf_pkt_rx_ring_t>(rx_ring)));
  }
  VLOG(1) << "Unregistered packetio callbacks on device " << device_id_ << ".";

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::Shutdown() {
  RETURN_IF_ERROR(StopIo());
  {
    absl::WriterMutexLock l(&rx_writer_lock_);
    rx_writer_ = nullptr;
  }
  {
    absl::WriterMutexLock l(&data_lock_);
    packetin_header_.clear();
    packetout_header_.clear();
    packetin_header_size_ = 0;
    packetout_header_size_ = 0;
    initialized_ = false;
  }

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

  // Add a bytestring to the back of the buffer.
  ::util::Status PushBack(const std::string& bytestring, size_t bitwidth) {
    CHECK_RETURN_IF_FALSE(bytestring.size() <=
                          (bitwidth + kBitsPerByte - 1) / kBitsPerByte)
        << "Bytestring " << StringToHex(bytestring) << " overflows bit width "
        << bitwidth << ".";

    // Push all bits to a new buffer.
    std::deque<uint8> new_bits;
    for (const uint8 c : bytestring) {
      new_bits.push_back((c >> 7) & 1u);
      new_bits.push_back((c >> 6) & 1u);
      new_bits.push_back((c >> 5) & 1u);
      new_bits.push_back((c >> 4) & 1u);
      new_bits.push_back((c >> 3) & 1u);
      new_bits.push_back((c >> 2) & 1u);
      new_bits.push_back((c >> 1) & 1u);
      new_bits.push_back((c >> 0) & 1u);
    }
    // Remove bits from partial byte at the front.
    while (new_bits.size() > bitwidth) {
      CHECK_RETURN_IF_FALSE(new_bits.front() == 0)
          << "Bytestring " << StringToHex(bytestring) << " overflows bit width "
          << bitwidth << ".";
      new_bits.pop_front();
    }
    // Pad to full width.
    while (new_bits.size() < bitwidth) {
      new_bits.push_front(0);
    }
    // Append to internal buffer.
    bits_.insert(bits_.end(), new_bits.begin(), new_bits.end());

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
  std::string PopAll(void) {
    CHECK(bits_.size() % 8 == 0);
    return PopField(bits_.size());
  }

  // Returns a human-readable representation of the buffer.
  std::string ToString() {
    std::string ret;
    for (size_t i = 0; i < bits_.size(); ++i) {
      if (i % 8 == 0) ret += " ";
      ret += std::to_string(static_cast<uint16>(bits_[i]));
    }
    return ret;
  }

 private:
  const int kBitsPerByte = 8;
  std::deque<uint8> bits_;
};
}  // namespace

::util::Status BfrtPacketioManager::DeparsePacketOut(
    const ::p4::v1::PacketOut& packet, std::string* buffer) {
  absl::ReaderMutexLock l(&data_lock_);
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
    RETURN_IF_ERROR(bit_buf.PushBack(it->value(), bitwidth));
    VLOG(1) << "Encoded PacketOut metadata field with id " << id << " bitwidth "
            << bitwidth << " value 0x" << StringToHex(it->value());
  }
  auto hdr_buf = bit_buf.PopAll();
  buffer->resize(0);
  buffer->insert(buffer->end(), hdr_buf.begin(), hdr_buf.end());
  buffer->insert(buffer->end(), packet.payload().begin(),
                 packet.payload().end());

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::ParsePacketIn(const std::string& buffer,
                                                  ::p4::v1::PacketIn* packet) {
  absl::ReaderMutexLock l(&data_lock_);
  CHECK_RETURN_IF_FALSE(buffer.size() >= packetin_header_size_)
      << "Received packet is too small.";

  BitBuffer bit_buf;
  RETURN_IF_ERROR(bit_buf.PushBack(buffer.substr(0, packetin_header_size_),
                                   packetin_header_size_ * 8));
  for (const auto& p : packetin_header_) {
    auto metadata = packet->add_metadata();
    metadata->set_metadata_id(p.first);
    metadata->set_value(bit_buf.PopField(p.second));
  }
  packet->set_payload(buffer.data() + packetin_header_size_,
                      buffer.size() - packetin_header_size_);

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::TransmitPacket(
    const ::p4::v1::PacketOut& packet) {
  {
    absl::ReaderMutexLock l(&data_lock_);
    if (!initialized_) RETURN_ERROR(ERR_NOT_INITIALIZED) << "Not initialized.";
  }
  std::string buf;
  RETURN_IF_ERROR(DeparsePacketOut(packet, &buf));

  bf_pkt* pkt = nullptr;
  bf_pkt_tx_ring_t tx_ring = BF_PKT_TX_RING_0;
  RETURN_IF_BFRT_ERROR(
      bf_pkt_alloc(device_id_, &pkt, buf.size(), BF_DMA_CPU_PKT_TRANSMIT_0));
  auto pkt_cleaner =
      gtl::MakeCleanup([pkt, this]() { bf_pkt_free(device_id_, pkt); });
  RETURN_IF_BFRT_ERROR(bf_pkt_data_copy(
      pkt, reinterpret_cast<const uint8_t*>(buf.data()), buf.size()));
  RETURN_IF_BFRT_ERROR(bf_pkt_tx(device_id_, pkt, tx_ring, pkt));
  pkt_cleaner.release();

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::HandlePacketRx(bf_dev_id_t dev_id,
                                                   bf_pkt* pkt,
                                                   bf_pkt_rx_ring_t rx_ring) {
  {
    absl::ReaderMutexLock l(&data_lock_);
    if (!initialized_) RETURN_ERROR(ERR_NOT_INITIALIZED) << "Not initialized.";
  }
  ::p4::v1::PacketIn packet;
  std::string buffer(reinterpret_cast<const char*>(bf_pkt_get_pkt_data(pkt)),
                     bf_pkt_get_pkt_size(pkt));
  RETURN_IF_ERROR(ParsePacketIn(buffer, &packet));
  {
    absl::WriterMutexLock l(&rx_writer_lock_);
    rx_writer_->Write(packet);
  }
  VLOG(1) << "Handled packet in: " << packet.ShortDebugString();

  return ::util::OkStatus();
}

// This function is based on P4TableMapper and implements a subset of its
// functionality.
// TODO(max): Check and reject if a mapping cannot be handled at runtime
::util::Status BfrtPacketioManager::BuildMetadataMapping(
    const p4::config::v1::P4Info& p4_info) {
  std::vector<std::pair<uint32, int>> packetin_header;
  std::vector<std::pair<uint32, int>> packetout_header;
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
        packetin_header.push_back(std::make_pair(id, bitwidth));
        packetin_bits += bitwidth;
      } else {
        packetout_header.push_back(std::make_pair(id, bitwidth));
        packetout_bits += bitwidth;
      }
    }
  }

  CHECK_RETURN_IF_FALSE(packetin_bits % 8 == 0)
      << "PacketIn header size must be multiple of 8 bits.";
  CHECK_RETURN_IF_FALSE(packetout_bits % 8 == 0)
      << "PacketOut header size must be multiple of 8 bits.";
  packetin_header_ = std::move(packetin_header);
  packetout_header_ = std::move(packetout_header);
  packetin_header_size_ = packetin_bits / 8;
  packetout_header_size_ = packetout_bits / 8;

  return ::util::OkStatus();
}

bf_status_t BfrtPacketioManager::BfPktTxNotifyCallback(bf_dev_id_t dev_id,
                                                       bf_pkt_tx_ring_t tx_ring,
                                                       uint64 tx_cookie,
                                                       uint32 status) {
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
