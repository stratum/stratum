// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_packetio_manager.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <deque>
#include <string>

#include "absl/strings/string_view.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/gtl/stl_util.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/utils.h"

// Must be included after net/if.h because of re-defines.
#include <linux/if.h>
#include <linux/if_tun.h>

DEFINE_bool(experimental_enable_bfrt_tofino_virtual_cpu_interface, false,
            "enable exposure of a virtual CPU interface on Tofino switches.");

namespace stratum {
namespace hal {
namespace barefoot {

namespace {
::util::StatusOr<int> CreateTapIntf(std::string name) {
  // Note: The canonical TUN device at /dev/net/tun failed to open. The BF team
  //       created a copy of the tun driver, bf_tun, which is loaded by default
  //       and does work.
  int fd = open("/dev/net/bf_tun", O_RDWR);
  RET_CHECK(fd >= 0) << "Failed to open: " << strerror(errno);
  struct ifreq ifr = {};
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ);
  if (ioctl(fd, TUNSETIFF, &ifr) == -1) {
    close(fd);
    return MAKE_ERROR(ERR_INTERNAL)
           << "Couldn't create TAP interface " << ifr.ifr_name << ": "
           << strerror(errno) << ".";
  }

  LOG(WARNING) << "Created TAP interface with name " << ifr.ifr_name << ".";
  CHECK_EQ(ifr.ifr_name, name) << "Actual and requested TAP intf name differ.";

  // Configure the new TAP interface.
  // We use a dummy socket and IOCTL to setup the interface.
  int sock = socket(AF_INET, SOCK_DGRAM, 0);

  // Set MAC address.
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ);
  ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
  const uint8 mac[6] = {'\x00', '\x00', '\x00', '\x33', '\x33', '\x33'};
  memcpy(ifr.ifr_hwaddr.sa_data, mac, 6);
  if (ioctl(sock, SIOCSIFHWADDR, &ifr) == -1) {
    close(sock);
    return MAKE_ERROR(ERR_INTERNAL)
           << "Couldn't set MAC address for TAP interface " << ifr.ifr_name
           << ": " << strerror(errno) << ".";
  }

  // Set interface to UP.
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ);
  if (ioctl(sock, SIOCGIFFLAGS, &ifr) == -1) {
    close(sock);
    return MAKE_ERROR(ERR_INTERNAL)
           << "Couldn't get IFFLAGS for TAP interface " << ifr.ifr_name << ": "
           << strerror(errno) << ".";
  }
  ifr.ifr_flags |= IFF_UP;
  if (ioctl(sock, SIOCSIFFLAGS, &ifr) == -1) {
    close(sock);
    return MAKE_ERROR(ERR_INTERNAL)
           << "Couldn't set IFFLAGS for TAP interface " << ifr.ifr_name << ": "
           << strerror(errno) << ".";
  }
  close(sock);

  return fd;
}

::util::Status SetOwnThreadName(std::string name) {
  name.resize(15);  // Linux limit.
  RET_CHECK(pthread_setname_np(pthread_self(), name.c_str()) == 0);
  return ::util::OkStatus();
}

std::string GetOwnThreadId() { return absl::StrCat(pthread_self()); }
}  // namespace

BfrtPacketioManager::BfrtPacketioManager(
    BfSdeInterface* bf_sde_interface,
    BfrtP4RuntimeTranslator* bfrt_p4runtime_translator, int device)
    : initialized_(false),
      rx_writer_(nullptr),
      packetin_header_(),
      packetout_header_(),
      packetin_header_size_(),
      packetout_header_size_(),
      packet_receive_channel_(nullptr),
      tap_intf_fd_(-1),
      sde_rx_thread_id_(),
      virtual_cpu_intf_rx_thread_id_(),
      bf_sde_interface_(ABSL_DIE_IF_NULL(bf_sde_interface)),
      bfrt_p4runtime_translator_(ABSL_DIE_IF_NULL(bfrt_p4runtime_translator)),
      device_(device) {}

BfrtPacketioManager::BfrtPacketioManager()
    : initialized_(false),
      rx_writer_(nullptr),
      packetin_header_(),
      packetout_header_(),
      packetin_header_size_(),
      packetout_header_size_(),
      packet_receive_channel_(nullptr),
      tap_intf_fd_(-1),
      sde_rx_thread_id_(),
      virtual_cpu_intf_rx_thread_id_(),
      bf_sde_interface_(nullptr),
      device_(-1) {}

BfrtPacketioManager::~BfrtPacketioManager() {}

std::unique_ptr<BfrtPacketioManager> BfrtPacketioManager::CreateInstance(
    BfSdeInterface* bf_sde_interface_,
    BfrtP4RuntimeTranslator* bfrt_p4runtime_translator, int device) {
  return absl::WrapUnique(new BfrtPacketioManager(
      bf_sde_interface_, bfrt_p4runtime_translator, device));
}

::util::Status BfrtPacketioManager::PushChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::PushForwardingPipelineConfig(
    const BfrtDeviceConfig& config) {
  absl::WriterMutexLock l(&data_lock_);
  RET_CHECK(config.programs_size() == 1) << "Only one program is supported.";
  const auto& program = config.programs(0);
  ASSIGN_OR_RETURN(
      const auto& p4info,
      bfrt_p4runtime_translator_->TranslateP4Info(program.p4info()));
  RETURN_IF_ERROR(BuildMetadataMapping(p4info));
  // PushForwardingPipelineConfig resets the bf_pkt driver.
  RETURN_IF_ERROR(bf_sde_interface_->StartPacketIo(device_));
  if (!initialized_) {
    packet_receive_channel_ = Channel<std::string>::Create(1024);
    if (sde_rx_thread_id_ == 0) {
      int ret = pthread_create(&sde_rx_thread_id_, nullptr,
                               &BfrtPacketioManager::SdeRxThreadFunc, this);
      if (ret != 0) {
        return MAKE_ERROR(ERR_INTERNAL) << "Failed to spawn RX thread for SDE "
                                        << "wrapper for device with ID "
                                        << device_ << ". Err: " << ret << ".";
      }
    }
    RETURN_IF_ERROR(bf_sde_interface_->RegisterPacketReceiveWriter(
        device_, ChannelWriter<std::string>::Create(packet_receive_channel_)));
    // Create TAP interface and start rx/tx handler.
    if (FLAGS_experimental_enable_bfrt_tofino_virtual_cpu_interface) {
      ASSIGN_OR_RETURN(tap_intf_fd_, CreateTapIntf("tapSwCpu"));
      if (virtual_cpu_intf_rx_thread_id_ == 0) {
        int ret = pthread_create(
            &virtual_cpu_intf_rx_thread_id_, nullptr,
            &BfrtPacketioManager::VirtualCpuIntfRxThreadFunc, this);
        if (ret != 0) {
          return MAKE_ERROR(ERR_INTERNAL)
                 << "Failed to spawn RX thread for virtual CPU interface "
                 << "for device with ID " << device_ << ". Err: " << ret << ".";
        }
      }
    }

    initialized_ = true;
  }

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::VerifyChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::Shutdown() {
  ::util::Status status;
  {
    absl::WriterMutexLock l(&rx_writer_lock_);
    rx_writer_ = nullptr;
  }
  {
    absl::WriterMutexLock l(&data_lock_);
    if (initialized_) {
      APPEND_STATUS_IF_ERROR(status, bf_sde_interface_->StopPacketIo(device_));
      APPEND_STATUS_IF_ERROR(
          status, bf_sde_interface_->UnregisterPacketReceiveWriter(device_));
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
  absl::SleepFor(absl::Milliseconds(500));
  {
    absl::ReaderMutexLock l(&data_lock_);
    if (sde_rx_thread_id_ != 0 &&
        pthread_join(sde_rx_thread_id_, nullptr) != 0) {
      ::util::Status error = MAKE_ERROR(ERR_INTERNAL)
                             << "Failed to join thread " << sde_rx_thread_id_;
      APPEND_STATUS_IF_ERROR(status, error);
    }
    if (FLAGS_experimental_enable_bfrt_tofino_virtual_cpu_interface) {
      if (virtual_cpu_intf_rx_thread_id_ != 0 &&
          pthread_join(virtual_cpu_intf_rx_thread_id_, nullptr) != 0) {
        ::util::Status error = MAKE_ERROR(ERR_INTERNAL)
                               << "Failed to join thread "
                               << virtual_cpu_intf_rx_thread_id_;
        APPEND_STATUS_IF_ERROR(status, error);
      }
      // Only close the interface after the threads have been joined. Otherwise
      // this is a race condition.
      close(tap_intf_fd_);
      VLOG(1) << "Closed TAP interface.";
    }
  }
  {
    absl::WriterMutexLock l(&data_lock_);
    sde_rx_thread_id_ = 0;
    virtual_cpu_intf_rx_thread_id_ = 0;
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
    RET_CHECK(bytestring.size() <= (bitwidth + kBitsPerByte - 1) / kBitsPerByte)
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

::util::Status BfrtPacketioManager::ParsePacketIn(const std::string& buffer,
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
    metadata->set_value(
        ByteStringToP4RuntimeByteString(bit_buf.PopField(p.second)));
    VLOG(1) << "Encoded PacketIn metadata field with id " << p.first
            << " bitwidth " << p.second << " value 0x"
            << StringToHex(metadata->value());
  }
  packet->set_payload(buffer.data() + packetin_header_size_,
                      buffer.size() - packetin_header_size_);

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::TransmitPacket(
    const ::p4::v1::PacketOut& packet) {
  {
    absl::ReaderMutexLock l(&data_lock_);
    if (!initialized_)
      return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized.";
  }
  ASSIGN_OR_RETURN(const auto& translated_packet_out,
                   bfrt_p4runtime_translator_->TranslatePacketOut(packet));
  std::string buf;
  RETURN_IF_ERROR(DeparsePacketOut(translated_packet_out, &buf));

  RETURN_IF_ERROR(bf_sde_interface_->TxPacket(device_, buf));

  return ::util::OkStatus();
}

namespace {

// This enum defines the possible pre-header values on packets on the PCIe bus.
enum class PacketioPreHeader : char {
  INVALID = 0,
  REGULAR_PACKETIO = 1,  // This is a regular P4Runtime PacketIn/Out.
  VIRTUAL_CPU_INTF = 2,  // This packet is from/to the virtual CPU interface.
};

::util::Status IsVirtualCpuIntfTraffic(const std::string& buffer) {
  if (buffer.length() < 1) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Packet too short.";
  }
  if (buffer[0] == static_cast<char>(PacketioPreHeader::VIRTUAL_CPU_INTF)) {
    return ::util::OkStatus();
  } else if (buffer[0] ==
             static_cast<char>(PacketioPreHeader::REGULAR_PACKETIO)) {
    return MAKE_ERROR(ERR_INVALID_PARAM).with_logging() << "Normal traffic.";
  } else {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Unknown pre packet header.";
  }
}

}  // namespace

::util::Status BfrtPacketioManager::SendToVirtualCpuIntf(
    absl::string_view buffer) const {
  absl::ReaderMutexLock l(&data_lock_);
  if (!FLAGS_experimental_enable_bfrt_tofino_virtual_cpu_interface) {
    return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE)
           << "Virtual CPU interface not enabled.";
  }

  if (tap_intf_fd_ < 0) {
    return MAKE_ERROR(ERR_INTERNAL) << "TAP interface not initialized";
  }

  RET_CHECK(write(tap_intf_fd_, buffer.data(), buffer.size()) > 0)
      << "write to TAP interface failed";

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::HandleVirtualCpuIntfPacketRx() {
  SetOwnThreadName("HandleVirtualCpuIntfPacketRx");
  LOG(WARNING) << "HandleVirtualCpuIntfPacketRx tid: " << GetOwnThreadId();
  static constexpr size_t kMaxRxBufferSize = 32768;

  int fd = -1;  // Copy the fd to avoid locking the mutex inside the loop.
  {
    absl::ReaderMutexLock l(&data_lock_);
    if (!FLAGS_experimental_enable_bfrt_tofino_virtual_cpu_interface) {
      return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE)
             << "Virtual CPU interface not enabled.";
    }
    RET_CHECK(tap_intf_fd_ > 0) << "TAP interface not initialized";
    fd = tap_intf_fd_;
  }

  // Create buffer with fake Ethernet header already in place.
  // Insert fake ethernet header to help parsing.
  constexpr size_t kPreHeaderLength = 14;  // 6 + 6 + 2.
  const std::string fake_mac("\x00\x00\x00\x00\x00\x00", 6);
  const std::string fake_eth_type("\xbf\x02", 2);  // Reserved ether type.
  std::string buf = absl::StrCat(fake_mac, fake_mac, fake_eth_type);
  buf.resize(kMaxRxBufferSize);            // Pad with zeros.
  char* buf_ptr = &buf[kPreHeaderLength];  // 1st byte after the header.

  while (true) {
    // This is the graceful shutdown check.
    {
      absl::ReaderMutexLock l(&data_lock_);
      if (!initialized_) break;
    }

    buf.resize(kMaxRxBufferSize);  // Pad with zeros.
    int ret = read(fd, buf_ptr, kMaxRxBufferSize - kPreHeaderLength);
    if (ret < 0) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "read from TAP interface failed: " << strerror(errno) << ".";
    }
    if (ret == 0) {
      LOG(ERROR) << "read zero bytes TAP interface?";
      continue;
    }
    buf.resize(kPreHeaderLength + ret);  // Trim trailing zero bytes.
    RETURN_IF_ERROR(bf_sde_interface_->TxPacket(device_, buf));
    // VLOG(1) << "Read buf from TAP interface and sent to pcie cpu port: "
    //         << StringToHex(buf) << ".";
  }

  LOG(INFO) << "Killed RX thread for virtual CPU interface.";

  return ::util::OkStatus();
}

::util::Status BfrtPacketioManager::HandleSdePacketRx() {
  SetOwnThreadName("HandleSdePacketRx");
  LOG(WARNING) << "HandleSdePacketRx tid: " << GetOwnThreadId();
  std::unique_ptr<ChannelReader<std::string>> reader;
  int fd = -1;  // Copy the fd to avoid locking the mutex inside the loop.
  {
    absl::ReaderMutexLock l(&data_lock_);
    if (!initialized_)
      return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized.";
    reader = ChannelReader<std::string>::Create(packet_receive_channel_);
    if (!reader) return MAKE_ERROR(ERR_INTERNAL) << "Failed to create reader.";
    fd = tap_intf_fd_;
  }

  bool virtual_cpu_interface_enabled =
      FLAGS_experimental_enable_bfrt_tofino_virtual_cpu_interface;

  while (true) {
    {
      absl::ReaderMutexLock l(&chassis_lock);
      if (shutdown) break;
    }
    std::string buffer;
    int code = reader->Read(&buffer, absl::InfiniteDuration()).error_code();
    if (code == ERR_CANCELLED) break;
    if (code == ERR_ENTRY_NOT_FOUND) {
      LOG(ERROR) << "Read with infinite timeout failed with ENTRY_NOT_FOUND.";
      continue;
    }

    // Check if this packet is to be forwarded to the virtual CPU interface.
    if (virtual_cpu_interface_enabled && IsVirtualCpuIntfTraffic(buffer).ok()) {
      // Skip pre header
      // buffer.erase(0 /*position*/, 1 /*count*/);
      // absl::string_view view(buffer.data() + 1, buffer.size() - 1);
      // SendToVirtualCpuIntf(view);
      // if (!status.ok()) {
      //   LOG(ERROR) << "Failed to send to virtual cpu interface: " << status;
      // }

      int ret = write(fd, buffer.data() + 1, buffer.size() - 1);
      if (ret < 0) {
        LOG(ERROR) << "write to TAP interface failed: " << ret;
      }

      // VLOG(1) << "read buf from pcie cpu interface and sent to tap interface:
      // "
      //         << StringToHex(buffer) << ".";
      continue;
    }

    ::p4::v1::PacketIn packet_in;
    ::util::Status status = ParsePacketIn(buffer, &packet_in);
    if (!status.ok()) {
      LOG(ERROR) << "ParsePacketIn failed: " << status;
      continue;
    }
    const auto& translated_packet_in =
        bfrt_p4runtime_translator_->TranslatePacketIn(packet_in);
    if (!translated_packet_in.ok()) {
      LOG(ERROR) << "TranslatePacketIn failed: " << status;
      continue;
    }
    {
      absl::WriterMutexLock l(&rx_writer_lock_);
      rx_writer_->Write(translated_packet_in.ValueOrDie());
    }
    VLOG(1) << "Handled PacketIn: " << packet_in.ShortDebugString();
  }

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

void* BfrtPacketioManager::SdeRxThreadFunc(void* arg) {
  // SetOwnThreadName("SdeRxThreadFunc");
  BfrtPacketioManager* mgr = reinterpret_cast<BfrtPacketioManager*>(arg);
  ::util::Status status = mgr->HandleSdePacketRx();
  if (!status.ok()) {
    LOG(ERROR) << "Non-OK exit of RX thread for SDE interface.";
  }

  return nullptr;
}

void* BfrtPacketioManager::VirtualCpuIntfRxThreadFunc(void* arg) {
  // SetOwnThreadName("VirtualCpuIntfRxThreadFunc");
  BfrtPacketioManager* mgr = reinterpret_cast<BfrtPacketioManager*>(arg);
  ::util::Status status = mgr->HandleVirtualCpuIntfPacketRx();
  if (!status.ok()) {
    LOG(ERROR) << "Non-OK exit of RX thread for virtual CPU interface.";
  }

  return nullptr;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
