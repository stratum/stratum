// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/tdi/tdi_packetio_manager.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

#include <deque>
#include <string>

#include "absl/cleanup/cleanup.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/utils.h"

DECLARE_bool(incompatible_enable_tdi_legacy_bytestring_responses);

namespace stratum {
namespace hal {
namespace tdi {

TdiPacketioManager::TdiPacketioManager(TdiSdeInterface* tdi_sde_interface,
                                       int device)
    : initialized_(false),
      rx_writer_(nullptr),
      packetin_header_(),
      packetout_header_(),
      packetin_header_size_(),
      packetout_header_size_(),
      packet_receive_channel_(nullptr),
      sde_rx_thread_id_(0),
      tdi_sde_interface_(ABSL_DIE_IF_NULL(tdi_sde_interface)),
      device_(device) {}

TdiPacketioManager::~TdiPacketioManager() {}

std::unique_ptr<TdiPacketioManager> TdiPacketioManager::CreateInstance(
    TdiSdeInterface* tdi_sde_interface_, int device) {
  return absl::WrapUnique(new TdiPacketioManager(tdi_sde_interface_, device));
}

::util::Status TdiPacketioManager::PushChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  return ::util::OkStatus();
}

::util::Status TdiPacketioManager::PushForwardingPipelineConfig(
    const TdiDeviceConfig& config) {
  RET_CHECK(config.programs_size() == 1)
      << "Only one program is supported.";
  const auto& program = config.programs(0);
  {
    absl::WriterMutexLock l(&data_lock_);
    RETURN_IF_ERROR(BuildMetadataMapping(program.p4info()));
    // PushForwardingPipelineConfig resets the bf_pkt driver.
    RETURN_IF_ERROR(tdi_sde_interface_->StartPacketIo(device_));
    if (!initialized_) {
      packet_receive_channel_ = Channel<std::string>::Create(128);
      if (sde_rx_thread_id_ == 0) {
        int ret = pthread_create(&sde_rx_thread_id_, nullptr,
                                 &TdiPacketioManager::SdeRxThreadFunc, this);
        if (ret != 0) {
          return MAKE_ERROR(ERR_INTERNAL)
              << "Failed to spawn RX thread for SDE wrapper for device with ID "
              << device_ << ". Err: " << ret << ".";
        }
      }
      RETURN_IF_ERROR(tdi_sde_interface_->RegisterPacketReceiveWriter(
          device_,
          ChannelWriter<std::string>::Create(packet_receive_channel_)));
    }
    initialized_ = true;
  }

  return ::util::OkStatus();
}

::util::Status TdiPacketioManager::VerifyChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  return ::util::OkStatus();
}

::util::Status TdiPacketioManager::Shutdown() {
  ::util::Status status;
  {
    absl::WriterMutexLock l(&rx_writer_lock_);
    rx_writer_ = nullptr;
  }
  {
    absl::WriterMutexLock l(&data_lock_);
    if (initialized_) {
      APPEND_STATUS_IF_ERROR(status, tdi_sde_interface_->StopPacketIo(device_));
      APPEND_STATUS_IF_ERROR(
          status, tdi_sde_interface_->UnregisterPacketReceiveWriter(device_));
      if (!packet_receive_channel_ || !packet_receive_channel_->Close()) {
        ::util::Status error = MAKE_ERROR(ERR_INTERNAL)
                               << "Packet Rx channel is already closed.";
        APPEND_STATUS_IF_ERROR(status, error);
      }
    }
    packetin_header_.clear();
    packetout_header_.clear();
    packetin_header_size_ = 0;
    packetout_header_size_ = 0;
    packet_receive_channel_.reset();
    initialized_ = false;
  }
  // TODO(max): we release the locks between closing the channel and joining the
  // thread to prevent deadlocks with the RX handler. But there might still be a
  // bug hiding here.
  {
    absl::ReaderMutexLock l(&data_lock_);
    if (sde_rx_thread_id_ != 0 &&
        pthread_join(sde_rx_thread_id_, nullptr) != 0) {
      ::util::Status error = MAKE_ERROR(ERR_INTERNAL)
                             << "Failed to join thread " << sde_rx_thread_id_;
      APPEND_STATUS_IF_ERROR(status, error);
    }
  }
  {
    absl::WriterMutexLock l(&data_lock_);
    sde_rx_thread_id_ = 0;
  }
  return ::util::OkStatus();
}

::util::Status TdiPacketioManager::RegisterPacketReceiveWriter(
    const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer) {
  absl::WriterMutexLock l(&rx_writer_lock_);
  rx_writer_ = writer;
  return ::util::OkStatus();
}

::util::Status TdiPacketioManager::UnregisterPacketReceiveWriter() {
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
    RET_CHECK(bytestring.size() <=
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
      RET_CHECK(new_bits.front() == 0)
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
    CHECK_EQ(bits_.size() % 8, 0);
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

::util::Status TdiPacketioManager::DeparsePacketOut(
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
    RET_CHECK(it != packet.metadata().end())
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

::util::Status TdiPacketioManager::ParsePacketIn(const std::string& buffer,
                                                 ::p4::v1::PacketIn* packet) {
  absl::ReaderMutexLock l(&data_lock_);
  RET_CHECK(buffer.size() >= packetin_header_size_)
      << "Received packet is too small.";

  BitBuffer bit_buf;
  RETURN_IF_ERROR(bit_buf.PushBack(buffer.substr(0, packetin_header_size_),
                                   packetin_header_size_ * 8));
  for (const auto& p : packetin_header_) {
    auto metadata = packet->add_metadata();
    metadata->set_metadata_id(p.first);
    metadata->set_value(bit_buf.PopField(p.second));
    if (!FLAGS_incompatible_enable_tdi_legacy_bytestring_responses) {
      *metadata->mutable_value() =
          ByteStringToP4RuntimeByteString(metadata->value());
    }
    VLOG(1) << "Encoded PacketIn metadata field with id " << p.first
            << " bitwidth " << p.second << " value 0x"
            << StringToHex(metadata->value());
  }
  packet->set_payload(buffer.data() + packetin_header_size_,
                      buffer.size() - packetin_header_size_);

  return ::util::OkStatus();
}

::util::Status TdiPacketioManager::TransmitPacket(
    const ::p4::v1::PacketOut& packet) {
  {
    absl::ReaderMutexLock l(&data_lock_);
    if (!initialized_) return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized.";
  }
  std::string buf;
  RETURN_IF_ERROR(DeparsePacketOut(packet, &buf));

  RETURN_IF_ERROR(tdi_sde_interface_->TxPacket(device_, buf));

  return ::util::OkStatus();
}

// TODO(max): drop Sde in name?
::util::Status TdiPacketioManager::HandleSdePacketRx() {
  std::unique_ptr<ChannelReader<std::string>> reader;
  {
    absl::ReaderMutexLock l(&data_lock_);
    if (!initialized_) return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized.";
    reader = ChannelReader<std::string>::Create(packet_receive_channel_);
  }

  while (true) {
    std::string buffer;
    int code = reader->Read(&buffer, absl::InfiniteDuration()).error_code();
    if (code == ERR_CANCELLED) break;
    if (code == ERR_ENTRY_NOT_FOUND) {
      LOG(ERROR) << "Read with infinite timeout failed with ENTRY_NOT_FOUND.";
      continue;
    }

    ::p4::v1::PacketIn packet_in;
    // FIXME: returning here in case of parsing errors might not be the best
    // solution.
    RETURN_IF_ERROR(ParsePacketIn(buffer, &packet_in));
    {
      absl::WriterMutexLock l(&rx_writer_lock_);
      rx_writer_->Write(packet_in);
    }
    VLOG(1) << "Handled PacketIn: " << packet_in.ShortDebugString();
  }

  return ::util::OkStatus();
}

// This function is based on P4TableMapper and implements a subset of its
// functionality.
// TODO(max): Check and reject if a mapping cannot be handled at runtime
::util::Status TdiPacketioManager::BuildMetadataMapping(
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

  RET_CHECK(packetin_bits % 8 == 0)
      << "PacketIn header size must be multiple of 8 bits.";
  RET_CHECK(packetout_bits % 8 == 0)
      << "PacketOut header size must be multiple of 8 bits.";
  packetin_header_ = std::move(packetin_header);
  packetout_header_ = std::move(packetout_header);
  packetin_header_size_ = packetin_bits / 8;
  packetout_header_size_ = packetout_bits / 8;

  return ::util::OkStatus();
}

void* TdiPacketioManager::SdeRxThreadFunc(void* arg) {
  TdiPacketioManager* mgr = reinterpret_cast<TdiPacketioManager*>(arg);
  ::util::Status status = mgr->HandleSdePacketRx();
  if (!status.ok()) {
    LOG(ERROR) << "Non-OK exit of RX thread for SDE interface.";
  }

  return nullptr;
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
