// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bcm/bcm_packetio_manager.h"

#include <functional>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm_chassis_ro_mock.h"
#include "stratum/hal/lib/bcm/bcm_sdk_mock.h"
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/hal/lib/p4/p4_table_mapper_mock.h"
#include "stratum/lib/libcproxy/libcwrapper.h"
#include "stratum/lib/libcproxy/passthrough_proxy.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

// #include "util/libcproxy/libcproxy.h"
// #include "util/libcproxy/libcwrapper.h"
// #include "util/libcproxy/passthrough_proxy.h"

namespace stratum {
namespace hal {
namespace bcm {

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::WithArgs;

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

MATCHER_P(EqualsStatus, status, "") {
  return arg.error_code() == status.error_code() &&
         arg.error_message() == status.error_message();
}

namespace {

class LibcProxyMock : public PassthroughLibcProxy {
 public:
  ~LibcProxyMock() override {}

  // This class is defined as a singleton and is kept alive during the entire
  // test to keep TSAN happy.
  static LibcProxyMock* Instance() {
    static LibcProxyMock* instance = nullptr;
    if (instance == nullptr) {
      instance = new LibcProxyMock();
    }
    return instance;
  }

  bool ShouldProxyEpollCreate() override { return true; }

  int socket(int domain, int type, int protocol) override {
    return Socket(domain, type, protocol);
  }
  int setsockopt(int sockfd, int level, int optname, const void* optval,
                 socklen_t optlen) override {
    return SetSockOpt(sockfd, level, optname, optval, optlen);
  }
  int close(int fd) override {
    if (fds_.count(fd)) return Close(fd);
    // Must close the fd regardless of whether or not it's ours.
    return PassthroughLibcProxy::close(fd);
  }
  int ioctl(int fd, unsigned long int request, void* arg) override {  // NOLINT
    return Ioctl(fd, request, arg);
  }
  int bind(int sockfd, const struct sockaddr* my_addr,
           socklen_t addrlen) override {
    return Bind(sockfd, my_addr, addrlen);
  }
  ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) override {
    return SendMsg(sockfd, msg, flags);
  }
  ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) override {
    return RecvMsg(sockfd, msg, flags);
  }
  int epoll_create1(int flags) override { return EpollCreate1(flags); }
  int epoll_ctl(int efd, int op, int fd, struct epoll_event* event) override {
    return EpollCtl(efd, op, fd, event);
  }
  int epoll_wait(int efd, struct epoll_event* events, int maxevents,
                 int timeout) override {
    return EpollWait(efd, events, maxevents, timeout);
  }

  // Track a set of fds to make sure when close is called we call the mocked
  // version instead.
  void TrackFds(std::set<int> fds) { fds_ = fds; }

  MOCK_METHOD3(Socket, int(int domain, int type, int protocol));
  MOCK_METHOD5(SetSockOpt, int(int sockfd, int level, int optname,
                               const void* optval, socklen_t optlen));
  MOCK_METHOD1(Close, int(int fd));
  MOCK_METHOD3(Ioctl,
               int(int fd, unsigned long int request, void* arg));  // NOLINT
  MOCK_METHOD3(Bind, int(int sockfd, const struct sockaddr* my_addr,
                         socklen_t addrlen));
  MOCK_METHOD3(SendMsg,
               ssize_t(int sockfd, const struct msghdr* msg, int flags));
  MOCK_METHOD3(RecvMsg, ssize_t(int sockfd, struct msghdr* msg, int flags));
  MOCK_METHOD1(EpollCreate1, int(int flags));
  MOCK_METHOD4(EpollCtl,
               int(int efd, int op, int fd, struct epoll_event* event));
  MOCK_METHOD4(EpollWait, int(int efd, struct epoll_event* events,
                              int maxevents, int timeout));

 private:
  LibcProxyMock() {}
  std::set<int> fds_;
};

// Macros to quickly check RX/TX counters. To be called only within the test
// cases in this file.
#define CHECK_ZERO_TX_COUNTER(purpose, counter)            \
  do {                                                     \
    auto ret = bcm_packetio_manager_->GetTxStats(purpose); \
    ASSERT_TRUE(ret.ok()) << ret.status();                 \
    EXPECT_EQ(ret.ValueOrDie().counter, 0)                 \
        << "Unexpected non-zero counter: " << #counter;    \
  } while (0)

#define CHECK_NON_ZERO_TX_COUNTER(purpose, counter)        \
  do {                                                     \
    auto ret = bcm_packetio_manager_->GetTxStats(purpose); \
    ASSERT_TRUE(ret.ok()) << ret.status();                 \
    EXPECT_GT(ret.ValueOrDie().counter, 0)                 \
        << "Unexpected zero counter: " << #counter;        \
  } while (0)

#define CHECK_ZERO_RX_COUNTER(purpose, counter)            \
  do {                                                     \
    auto ret = bcm_packetio_manager_->GetRxStats(purpose); \
    ASSERT_TRUE(ret.ok()) << ret.status();                 \
    EXPECT_EQ(ret.ValueOrDie().counter, 0)                 \
        << "Unexpected non-zero counter: " << #counter;    \
  } while (0)

#define CHECK_NON_ZERO_RX_COUNTER(purpose, counter)        \
  do {                                                     \
    auto ret = bcm_packetio_manager_->GetRxStats(purpose); \
    ASSERT_TRUE(ret.ok()) << ret.status();                 \
    EXPECT_GT(ret.ValueOrDie().counter, 0)                 \
        << "Unexpected zero counter: " << #counter;        \
  } while (0)

}  // namespace

class BcmPacketioManagerTest : public ::testing::TestWithParam<OperationMode> {
 public:
  static void SetUpTestSuite() {
    LibcWrapper::SetLibcProxy(LibcProxyMock::Instance());
  }

  static void TearDownTestSuite() {
    LibcProxyMock* instance = LibcProxyMock::Instance();
    if (instance) {
      delete instance;
    }
    LibcWrapper::SetLibcProxy(nullptr);
  }

  bool RxComplete() {
    absl::ReaderMutexLock l(&rx_lock_);
    return rx_complete_;
  }

 protected:
  void SetUp() override {
    mode_ = GetParam();
    bcm_chassis_ro_mock_ = absl::make_unique<BcmChassisRoMock>();
    p4_table_mapper_mock_ = absl::make_unique<P4TableMapperMock>();
    bcm_sdk_mock_ = absl::make_unique<BcmSdkMock>();
    bcm_packetio_manager_ = BcmPacketioManager::CreateInstance(
        mode_, bcm_chassis_ro_mock_.get(), p4_table_mapper_mock_.get(),
        bcm_sdk_mock_.get(), kUnit1);
    {
      absl::WriterMutexLock l(&rx_lock_);
      rx_complete_ = false;
    }
    {
      absl::WriterMutexLock l(&chassis_lock);
      shutdown = false;
    }
  }

  ::util::Status PushChassisConfig(const ChassisConfig& config,
                                   uint64 node_id) {
    absl::WriterMutexLock l(&chassis_lock);
    return bcm_packetio_manager_->PushChassisConfig(config, node_id);
  }

  ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                     uint64 node_id) {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_packetio_manager_->VerifyChassisConfig(config, node_id);
  }

  ::util::Status Shutdown() {
    {
      absl::WriterMutexLock l(&chassis_lock);
      shutdown = true;
    }
    return bcm_packetio_manager_->Shutdown();
  }

  ::util::Status RegisterPacketReceiveWriter(
      GoogleConfig::BcmKnetIntfPurpose purpose,
      const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer) {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_packetio_manager_->RegisterPacketReceiveWriter(purpose, writer);
  }

  ::util::Status TransmitPacket(GoogleConfig::BcmKnetIntfPurpose purpose,
                                const ::p4::v1::PacketOut& packet) {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_packetio_manager_->TransmitPacket(purpose, packet);
  }

  ::util::Status PopulateChassisConfigAndPortMaps(
      uint64 node_id, ChassisConfig* config,
      std::map<uint32, SdkPort>* port_id_to_sdk_port) {
    if (config) {
      const std::string& config_text = absl::Substitute(
          kChassisConfigTemplate, kNodeId1, kNodeId2, kPortId1, kPort1,
          kPortId2, kPort2, kNodeId1, kNodeId1, kNodeId2);
      RETURN_IF_ERROR(ParseProtoFromString(config_text, config));
    }
    if (port_id_to_sdk_port) {
      if (node_id == kNodeId1) {
        // One port on unit1.
        port_id_to_sdk_port->insert({kPortId1, {kUnit1, kLogicalPort1}});
      } else if (node_id == kNodeId2) {
        // One port on unit2.
        port_id_to_sdk_port->insert({kPortId2, {kUnit2, kLogicalPort2}});
      }
    }

    return ::util::OkStatus();
  }

  void VerifyInternalStateAfterConfigPush(uint64 node_id) const {
    auto controller_purpose = GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER;
    auto sflow_purpose = GoogleConfig::BCM_KNET_INTF_PURPOSE_SFLOW;
    EXPECT_EQ(bcm_packetio_manager_->node_id_, node_id);
    if (node_id == kNodeId1) {
      EXPECT_EQ(bcm_packetio_manager_->unit_, kUnit1);
      const auto& purpose_to_knet_intf =
          bcm_packetio_manager_->purpose_to_knet_intf_;
      EXPECT_EQ(kSocket1, purpose_to_knet_intf.at(controller_purpose).tx_sock);
      EXPECT_EQ(kSocket1, purpose_to_knet_intf.at(controller_purpose).rx_sock);
      EXPECT_EQ(1, purpose_to_knet_intf.at(controller_purpose).cpu_queue);
      EXPECT_EQ(10, purpose_to_knet_intf.at(controller_purpose).vlan);
      EXPECT_EQ(std::set<int>({kCatchAllFilterId1}),
                purpose_to_knet_intf.at(controller_purpose).filter_ids);
      EXPECT_EQ(kSocket2, purpose_to_knet_intf.at(sflow_purpose).tx_sock);
      EXPECT_EQ(kSocket2, purpose_to_knet_intf.at(sflow_purpose).rx_sock);
      EXPECT_EQ(2, purpose_to_knet_intf.at(sflow_purpose).cpu_queue);
      EXPECT_EQ(10, purpose_to_knet_intf.at(sflow_purpose).vlan);
      EXPECT_EQ(std::set<int>({kSflowIngressFilterId1, kSflowEgressFilterId1}),
                purpose_to_knet_intf.at(sflow_purpose).filter_ids);

      ASSERT_EQ(1U, bcm_packetio_manager_->logical_port_to_port_id_.size());
      ASSERT_EQ(1U, bcm_packetio_manager_->port_id_to_logical_port_.size());
    } else if (node_id == kNodeId2) {
      EXPECT_EQ(bcm_packetio_manager_->unit_, kUnit2);
      const auto& purpose_to_knet_intf =
          bcm_packetio_manager_->purpose_to_knet_intf_;
      EXPECT_EQ(kSocket1, purpose_to_knet_intf.at(controller_purpose).tx_sock);
      EXPECT_EQ(kSocket1, purpose_to_knet_intf.at(controller_purpose).rx_sock);
      EXPECT_EQ(kDefaultCpuQueue,
                purpose_to_knet_intf.at(controller_purpose).cpu_queue);
      EXPECT_EQ(kDefaultVlan, purpose_to_knet_intf.at(controller_purpose).vlan);
      EXPECT_EQ(std::set<int>({kCatchAllFilterId1}),
                purpose_to_knet_intf.at(controller_purpose).filter_ids);
      EXPECT_FALSE(purpose_to_knet_intf.count(sflow_purpose));

      ASSERT_EQ(1U, bcm_packetio_manager_->logical_port_to_port_id_.size());
      ASSERT_EQ(1U, bcm_packetio_manager_->port_id_to_logical_port_.size());
    }
    /*
    EXPECT_THAT(bcm_packetio_manager_->bcm_rx_config_,
                EqualsProto(GoogleConfig::BcmRxConfig()));
    EXPECT_THAT(bcm_packetio_manager_->bcm_tx_config_,
                EqualsProto(GoogleConfig::BcmTxConfig()));
    EXPECT_THAT(bcm_packetio_manager_->bcm_knet_config_,
                EqualsProto(GoogleConfig::BcmKnetConfig()));
    EXPECT_THAT(bcm_packetio_manager_->bcm_rate_limit_config_,
                EqualsProto(GoogleConfig::BcmRateLimitConfig()));
    */
  }

  ::util::Status DefaultError() {
    return ::util::Status(StratumErrorSpace(), ERR_UNKNOWN, "some error");
  }

  void CheckNoTxStats() {
    absl::ReaderMutexLock l(&bcm_packetio_manager_->tx_stats_lock_);
    EXPECT_TRUE(bcm_packetio_manager_->purpose_to_tx_stats_.empty());
  }

  void CheckNoRxStats() {
    absl::ReaderMutexLock l(&bcm_packetio_manager_->rx_stats_lock_);
    EXPECT_TRUE(bcm_packetio_manager_->purpose_to_rx_stats_.empty());
  }

  // A configuration with 2 nodes (aka chips), 2 ports and vendor config.
  // The vendor config includes KNET config, RX config and rate limit config.
  static constexpr char kChassisConfigTemplate[] = R"(
      description: "Sample Generic Tomahawk config."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: $0
        slot: 1
      }
      nodes {
        id: $1
        slot: 1
      }
      singleton_ports {
        id: $2
        slot: 1
        port: $3
        speed_bps: 100000000000
      }
      singleton_ports {
        id: $4
        slot: 1
        port: $5
        speed_bps: 100000000000
      }
      vendor_config {
        google_config {
          node_id_to_knet_config {
            key: $6
            value {
              knet_intf_configs {
                mtu: 4000
                cpu_queue: 1
                vlan: 10
                purpose: BCM_KNET_INTF_PURPOSE_CONTROLLER
              }
              knet_intf_configs {
                mtu: 4000
                cpu_queue: 2
                vlan: 10
                purpose: BCM_KNET_INTF_PURPOSE_SFLOW
              }
            }
          }
          node_id_to_rx_config {
            key: $7
            value {
              rx_pool_pkt_count: 256
              rx_pool_bytes_per_pkt: 2048
              max_pkt_size_bytes: 2048
              pkts_per_chain: 4
              max_rate_pps: 1500
              max_burst_pkts: 256
              dma_channel_configs {
                key: 0
                value {
                  chains: 4
                  cos_set: 0
                  cos_set: 1
                  cos_set: 2
                  cos_set: 3
                  cos_set: 4
                  cos_set: 5
                  cos_set: 6
                }
              }
              dma_channel_configs {
                key: 1
                value {
                  chains: 4
                  cos_set: 7
                }
              }
            }
          }
          node_id_to_rate_limit_config {
            key: $8
            value {
              max_rate_pps: 1600
              max_burst_pkts: 512
              per_cos_rate_limit_configs {
                key: 0
                value {
                  max_rate_pps: 80
                  max_burst_pkts: 4
                }
              }
              per_cos_rate_limit_configs {
                key: 1
                value {
                  max_rate_pps: 80
                  max_burst_pkts: 4
                }
              }
              per_cos_rate_limit_configs {
                key: 2
                value {
                  max_rate_pps: 80
                  max_burst_pkts: 4
                }
              }
              per_cos_rate_limit_configs {
                key: 3
                value {
                  max_rate_pps: 80
                  max_burst_pkts: 4
                }
              }
              per_cos_rate_limit_configs {
                key: 4
                value {
                  max_rate_pps: 80
                  max_burst_pkts: 4
                }
              }
              per_cos_rate_limit_configs {
                key: 5
                value {
                  max_rate_pps: 80
                  max_burst_pkts: 4
                }
              }
              per_cos_rate_limit_configs {
                key: 6
                value {
                  max_rate_pps: 400
                  max_burst_pkts: 4
                }
              }
              per_cos_rate_limit_configs {
                key: 7
                value {
                  max_rate_pps: 1600
                  max_burst_pkts: 256
                }
              }
            }
          }
        }
      }
  )";

  // A test IPv4 packet. Was created using the following scapy command:
  // pkt = Ether(dst="02:32:00:00:00:01",src="00:00:00:00:00:01")/Dot1Q(vlan=1)/
  //       IP(src="10.0.1.1",dst="10.0.2.1",proto=254)/
  //       Raw(load="Test, Test, Test, Test!!!")
  static constexpr char kTestPacket[] =
      "\x02\x32\x00\x00\x00\x01\x00\x00\x00\x00\x00\x01\x81\x00\x00\x01\x08\x00"
      "\x45\x00\x00\x2d\x00\x01\x00\x00\x40\xfe\x62\xd1\x0a\x00\x01\x01\x0a\x00"
      "\x02\x01\x54\x65\x73\x74\x2c\x20\x54\x65\x73\x74\x2c\x20\x54\x65\x73\x74"
      "\x2c\x20\x54\x65\x73\x74\x21\x21\x21";

  // Sample fake metadata for testing packet in and out.
  static constexpr char kTestPacketMetadata1[] = R"(
      metadata_id: 123456
      value: "\x00\x01"
  )";
  static constexpr char kTestPacketMetadata2[] = R"(
      metadata_id: 654321
      value: "\x12"
  )";

  static constexpr uint64 kNodeId1 = 123123123;
  static constexpr uint64 kNodeId2 = 456456456;
  static constexpr int kUnit1 = 0;
  static constexpr int kUnit2 = 1;
  static constexpr uint32 kPortId1 = 1111;
  static constexpr uint32 kPortId2 = 2222;
  static constexpr uint32 kPortId3 = 3333;
  static constexpr uint32 kTrunkId1 = 975;
  static constexpr int kPort1 = 1;
  static constexpr int kPort2 = 2;
  static constexpr int kPort3 = 17;
  static constexpr int kLogicalPort1 = 33;
  static constexpr int kLogicalPort2 = 34;
  static constexpr int kLogicalPort3 = 55;
  static constexpr int kUnknownLogicalPort = 66;
  static constexpr BcmSdkInterface::KnetFilterType kFilterTypeCatchAll =
      BcmSdkInterface::KnetFilterType::CATCH_ALL;
  static constexpr BcmSdkInterface::KnetFilterType kFilterTypeNonSflow =
      BcmSdkInterface::KnetFilterType::CATCH_NON_SFLOW_FP_MATCH;
  static constexpr BcmSdkInterface::KnetFilterType kFilterTypeSflowIngress =
      BcmSdkInterface::KnetFilterType::CATCH_SFLOW_FROM_INGRESS_PORT;
  static constexpr BcmSdkInterface::KnetFilterType kFilterTypeSflowEgress =
      BcmSdkInterface::KnetFilterType::CATCH_SFLOW_FROM_EGRESS_PORT;
  static constexpr int kSocket1 = 987;
  static constexpr int kSocket2 = 654;
  static constexpr int kSocket3 = 321;
  static constexpr int kSocket4 = 123;
  static constexpr int kEfd = 159;
  static constexpr int kCatchAllFilterId1 = 10000;
  static constexpr int kNonSflowFilterId1 = 10001;
  static constexpr int kNonSflowFilterId2 = 10002;
  static constexpr int kSflowIngressFilterId1 = 10004;
  static constexpr int kSflowIngressFilterId2 = 10005;
  static constexpr int kSflowEgressFilterId1 = 10007;
  static constexpr int kSflowEgressFilterId2 = 10008;
  static constexpr size_t kTestKnetHeaderSize = 64;
  static constexpr size_t kTestPacketBodySize = 128;
  static constexpr int kNetifId = 199;

  OperationMode mode_;
  std::unique_ptr<BcmChassisRoMock> bcm_chassis_ro_mock_;
  std::unique_ptr<P4TableMapperMock> p4_table_mapper_mock_;
  std::unique_ptr<BcmSdkMock> bcm_sdk_mock_;
  std::unique_ptr<BcmPacketioManager> bcm_packetio_manager_;

  // A boolean showing that the RX handler in the class has received some
  // packets and it is done with validating them.
  bool rx_complete_ GUARDED_BY(rx_lock_);

  // A lock to protect rx_complete_ flag.
  mutable absl::Mutex rx_lock_;
};

constexpr char BcmPacketioManagerTest::kChassisConfigTemplate[];
constexpr char BcmPacketioManagerTest::kTestPacket[];
constexpr char BcmPacketioManagerTest::kTestPacketMetadata1[];
constexpr char BcmPacketioManagerTest::kTestPacketMetadata2[];
constexpr uint64 BcmPacketioManagerTest::kNodeId1;
constexpr uint64 BcmPacketioManagerTest::kNodeId2;
constexpr int BcmPacketioManagerTest::kUnit1;
constexpr int BcmPacketioManagerTest::kUnit2;
constexpr uint32 BcmPacketioManagerTest::kPortId1;
constexpr uint32 BcmPacketioManagerTest::kPortId2;
constexpr uint32 BcmPacketioManagerTest::kPortId3;
constexpr uint32 BcmPacketioManagerTest::kTrunkId1;
constexpr int BcmPacketioManagerTest::kPort1;
constexpr int BcmPacketioManagerTest::kPort2;
constexpr int BcmPacketioManagerTest::kPort3;
constexpr int BcmPacketioManagerTest::kLogicalPort1;
constexpr int BcmPacketioManagerTest::kLogicalPort2;
constexpr int BcmPacketioManagerTest::kLogicalPort3;
constexpr int BcmPacketioManagerTest::kUnknownLogicalPort;
constexpr BcmSdkInterface::KnetFilterType
    BcmPacketioManagerTest::kFilterTypeNonSflow;
constexpr BcmSdkInterface::KnetFilterType
    BcmPacketioManagerTest::kFilterTypeCatchAll;
constexpr BcmSdkInterface::KnetFilterType
    BcmPacketioManagerTest::kFilterTypeSflowIngress;
constexpr BcmSdkInterface::KnetFilterType
    BcmPacketioManagerTest::kFilterTypeSflowEgress;
constexpr int BcmPacketioManagerTest::kSocket1;
constexpr int BcmPacketioManagerTest::kSocket2;
constexpr int BcmPacketioManagerTest::kSocket3;
constexpr int BcmPacketioManagerTest::kSocket4;
constexpr int BcmPacketioManagerTest::kEfd;
constexpr int BcmPacketioManagerTest::kCatchAllFilterId1;
constexpr int BcmPacketioManagerTest::kNonSflowFilterId1;
constexpr int BcmPacketioManagerTest::kNonSflowFilterId2;
constexpr int BcmPacketioManagerTest::kSflowIngressFilterId1;
constexpr int BcmPacketioManagerTest::kSflowIngressFilterId2;
constexpr int BcmPacketioManagerTest::kSflowEgressFilterId1;
constexpr int BcmPacketioManagerTest::kSflowEgressFilterId2;
constexpr size_t BcmPacketioManagerTest::kTestKnetHeaderSize;
constexpr size_t BcmPacketioManagerTest::kTestPacketBodySize;
constexpr int BcmPacketioManagerTest::kNetifId;

TEST_P(BcmPacketioManagerTest, PushChassisConfigThenVerifySuccessForNode1) {
  //--------------------------------------------------------------
  // Cover the NOOP config verify and push config in sim mode.
  //--------------------------------------------------------------

  if (mode_ == OPERATION_MODE_SIM) {
    ChassisConfig config;
    ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId1, &config, nullptr));
    ASSERT_OK(VerifyChassisConfig(config, kNodeId1));
    ASSERT_OK(PushChassisConfig(config, kNodeId1));
    {
      SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
      CheckNoTxStats();
      CheckNoRxStats();
    }
    ASSERT_OK(Shutdown());
    return;
  }

  //--------------------------------------------------------------
  // 1st config push
  //--------------------------------------------------------------
  ChassisConfig config;
  std::map<uint32, SdkPort> port_id_to_sdk_port = {};
  ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId1, &config,
                                             &port_id_to_sdk_port));

  // Expected calls to BcmChassisManager for first config push.
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId1))
      .WillOnce(Return(port_id_to_sdk_port));

  // Track the socket FDs; These Fds are used for sockets etc and we need to
  // make sure we close them properly. Tracking them at LibcProxyMock ensures
  // that we know these Fds are used by the functions under test so that we use
  // the mocked version of close() for them.
  LibcProxyMock::Instance()->TrackFds({kSocket1, kSocket2, kEfd});

  // Expected libc calls for first config push.
  EXPECT_CALL(*LibcProxyMock::Instance(), Socket(_, _, _))
      .Times(6)
      .WillOnce(Return(kSocket1))
      .WillOnce(Return(kSocket1))
      .WillOnce(Return(kSocket1))
      .WillOnce(Return(kSocket2))
      .WillOnce(Return(kSocket2))
      .WillOnce(Return(kSocket2));

  // TODO(max): the ioctl call for SIOCSIFMTU is currently disabled because
  // SDKLT doesn't support it. See the comment in SetupSingleKnetIntf.
  EXPECT_CALL(*LibcProxyMock::Instance(), Ioctl(kSocket1, _, _))
      .Times(4)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Ioctl(kSocket2, _, _))
      .Times(4)
      .WillRepeatedly(Return(0));

  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1)).WillOnce(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket2)).WillOnce(Return(0));

  EXPECT_CALL(*LibcProxyMock::Instance(), SetSockOpt(kSocket1, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), SetSockOpt(kSocket2, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(0));

  EXPECT_CALL(*LibcProxyMock::Instance(), Bind(kSocket1, _, _))
      .WillOnce(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Bind(kSocket2, _, _))
      .WillOnce(Return(0));

  // Expected calls to BcmSdkInterface for first config push.
  EXPECT_CALL(*bcm_sdk_mock_, StartRx(kUnit1, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetIntf(kUnit1, 10, _, _))
      .Times(2)
      .WillRepeatedly(
          DoAll(SetArgPointee<3>(kNetifId), Return(::util::OkStatus())));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetFilter(kUnit1, _, kFilterTypeCatchAll))
      .WillOnce(Return(kCatchAllFilterId1));
  EXPECT_CALL(*bcm_sdk_mock_,
              CreateKnetFilter(kUnit1, _, kFilterTypeSflowIngress))
      .WillOnce(Return(kSflowIngressFilterId1));
  EXPECT_CALL(*bcm_sdk_mock_,
              CreateKnetFilter(kUnit1, _, kFilterTypeSflowEgress))
      .WillOnce(Return(kSflowEgressFilterId1));

  // Possible libc calls (triggered only if in the RX thread is spawned).
  EXPECT_CALL(*LibcProxyMock::Instance(), EpollCreate1(0))
      .WillRepeatedly(Return(kEfd));
  EXPECT_CALL(*LibcProxyMock::Instance(),
              EpollCtl(kEfd, EPOLL_CTL_ADD, kSocket1, _))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(),
              EpollCtl(kEfd, EPOLL_CTL_ADD, kSocket2, _))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), EpollWait(kEfd, _, 1, _))
      .WillRepeatedly(Return(0));  // 0 means no packet

  // Call PushChassisConfig for the first time for kNodeId1 and make sure
  // everything is OK.
  ASSERT_OK(VerifyChassisConfig(config, kNodeId1));
  ASSERT_OK(PushChassisConfig(config, kNodeId1));

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CheckNoRxStats();
  }

  //--------------------------------------------------------------
  // 2nd config push
  //--------------------------------------------------------------

  // Expected calls to BcmChassisManager for second config push.
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId1))
      .WillOnce(Return(port_id_to_sdk_port));

  // Calling PushChassisConfig again must get the node/port config from
  // BcmChassisManager and apply the rate limits again (if any). Node with ID
  // kNodeId1 does not have any rate limit config so no rate limit will be
  // set again.
  ASSERT_OK(VerifyChassisConfig(config, kNodeId1));
  ASSERT_OK(PushChassisConfig(config, kNodeId1));

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CheckNoRxStats();
  }

  //--------------------------------------------------------------
  // Verify internal maps.
  //--------------------------------------------------------------

  VerifyInternalStateAfterConfigPush(kNodeId1);

  //--------------------------------------------------------------
  // Verify for a different node or for a change in the non reconfigurable part
  // of the config will report reboot required
  //--------------------------------------------------------------

  ::util::Status status = VerifyChassisConfig(config, kNodeId2);
  EXPECT_EQ(ERR_REBOOT_REQUIRED, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Detected a change in the node_id"));

  config.clear_vendor_config();  // clear all the vendor config maps
  status = VerifyChassisConfig(config, kNodeId1);
  EXPECT_EQ(ERR_REBOOT_REQUIRED, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Detected a change"));

  //--------------------------------------------------------------
  // Shutdown
  //--------------------------------------------------------------

  // Expected calls to BcmSdkInterface for shutdown.
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1))
      .Times(2)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket2))
      .Times(2)
      .WillRepeatedly(Return(0));

  // Expected calls to BcmSdkInterface for shutdown.
  EXPECT_CALL(*bcm_sdk_mock_, StopRx(kUnit1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetFilter(kUnit1, kCatchAllFilterId1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetFilter(kUnit1, kSflowIngressFilterId1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetFilter(kUnit1, kSflowEgressFilterId1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetIntf(kUnit1, kNetifId))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));

  // Possible libc calls (triggered only if in the RX thread is spawned).
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kEfd))
      .WillRepeatedly(Return(0));

  ASSERT_OK(Shutdown());

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CheckNoRxStats();
  }
}

TEST_P(BcmPacketioManagerTest, PushChassisConfigThenVerifySuccessForNode2) {
  //--------------------------------------------------------------
  // Recreate the class for kUnit2
  //--------------------------------------------------------------

  bcm_packetio_manager_ = BcmPacketioManager::CreateInstance(
      mode_, bcm_chassis_ro_mock_.get(), p4_table_mapper_mock_.get(),
      bcm_sdk_mock_.get(), kUnit2);

  //--------------------------------------------------------------
  // Cover the NOOP config verify and push config in sim mode.
  //--------------------------------------------------------------

  if (mode_ == OPERATION_MODE_SIM) {
    ChassisConfig config;
    ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId2, &config, nullptr));
    ASSERT_OK(VerifyChassisConfig(config, kNodeId2));
    ASSERT_OK(PushChassisConfig(config, kNodeId2));
    {
      SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
      CheckNoTxStats();
      CheckNoRxStats();
    }
    ASSERT_OK(Shutdown());
    return;
  }

  //--------------------------------------------------------------
  // 1st config push
  //--------------------------------------------------------------

  ChassisConfig config;
  std::map<uint32, SdkPort> port_id_to_sdk_port = {};
  ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId2, &config,
                                             &port_id_to_sdk_port));

  // Expected calls to BcmChassisManager for first config push.
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId2))
      .WillOnce(Return(port_id_to_sdk_port));

  // Track the socket FDs; These Fds are used for sockets etc and we need to
  // make sure we close them properly. Tracking them at LibcProxyMock ensures
  // that we know these Fds are used by the functions under test so that we use
  // the mocked version of close() for them.
  LibcProxyMock::Instance()->TrackFds({kSocket1, kEfd});

  // Expected libc calls for first config push.
  EXPECT_CALL(*LibcProxyMock::Instance(), Socket(_, _, _))
      .Times(3)
      .WillOnce(Return(kSocket1))
      .WillOnce(Return(kSocket1))
      .WillOnce(Return(kSocket1));

  // TODO(max): the ioctl call for SIOCSIFMTU is currently disabled because
  // SDKLT doesn't support it. See the comment in SetupSingleKnetIntf.
  EXPECT_CALL(*LibcProxyMock::Instance(), Ioctl(kSocket1, _, _))
      .Times(4)
      .WillRepeatedly(Return(0));

  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1)).WillOnce(Return(0));

  EXPECT_CALL(*LibcProxyMock::Instance(), SetSockOpt(kSocket1, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(0));

  EXPECT_CALL(*LibcProxyMock::Instance(), Bind(kSocket1, _, _))
      .WillOnce(Return(0));

  // Expected calls to BcmSdkInterface for first config push.
  EXPECT_CALL(*bcm_sdk_mock_, StartRx(kUnit2, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetIntf(kUnit2, kDefaultVlan, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(kNetifId), Return(::util::OkStatus())));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetFilter(kUnit2, _, kFilterTypeCatchAll))
      .WillOnce(Return(kCatchAllFilterId1));
  EXPECT_CALL(*bcm_sdk_mock_, SetRateLimit(kUnit2, _))
      .WillOnce(Return(::util::OkStatus()));

  // Possible libc calls (triggered only if in the RX thread is spawned).
  EXPECT_CALL(*LibcProxyMock::Instance(), EpollCreate1(0))
      .WillRepeatedly(Return(kEfd));
  EXPECT_CALL(*LibcProxyMock::Instance(),
              EpollCtl(kEfd, EPOLL_CTL_ADD, kSocket1, _))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), EpollWait(kEfd, _, 1, _))
      .WillRepeatedly(Return(0));  // 0 means no packet

  // Call PushChassisConfig for the first time for kNodeId1 and make sure
  // everything is OK.
  ASSERT_OK(VerifyChassisConfig(config, kNodeId2));
  ASSERT_OK(PushChassisConfig(config, kNodeId2));

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CheckNoRxStats();
  }

  //--------------------------------------------------------------
  // 2nd config push
  //--------------------------------------------------------------

  // Expected calls to BcmChassisManager for second config push.
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId2))
      .WillOnce(Return(port_id_to_sdk_port));

  // Expected calls to BcmSdkInterface for second config push.
  EXPECT_CALL(*bcm_sdk_mock_, SetRateLimit(kUnit2, _))
      .WillOnce(Return(::util::OkStatus()));

  // Calling PushChassisConfig again must get the node/port config from
  // BcmChassisManager and apply the rate limits again (if any).
  ASSERT_OK(PushChassisConfig(config, kNodeId2));

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CheckNoRxStats();
  }

  //--------------------------------------------------------------
  // Verify internal maps.
  //--------------------------------------------------------------

  VerifyInternalStateAfterConfigPush(kNodeId2);

  //--------------------------------------------------------------
  // Verify for a different node or for a change in the non reconfigurable part
  // of the config will report reboot required
  //--------------------------------------------------------------

  ::util::Status status = VerifyChassisConfig(config, kNodeId1);
  EXPECT_EQ(ERR_REBOOT_REQUIRED, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Detected a change in the node_id"));

  config.clear_vendor_config();  // clear all the vendor config maps
  EXPECT_OK(VerifyChassisConfig(config, kNodeId2));

  //--------------------------------------------------------------
  // Shutdown
  //--------------------------------------------------------------

  // Expected libc calls for shutdown.
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1))
      .Times(2)
      .WillRepeatedly(Return(0));

  // Expected calls to BcmSdkInterface for shutdown.
  EXPECT_CALL(*bcm_sdk_mock_, StopRx(kUnit2))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetFilter(kUnit2, kCatchAllFilterId1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetIntf(kUnit2, kNetifId))
      .WillOnce(Return(::util::OkStatus()));

  // Possible libc calls (triggered only if in the RX thread is spawned).
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kEfd))
      .WillRepeatedly(Return(0));

  ASSERT_OK(Shutdown());

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CheckNoRxStats();
  }
}

TEST_P(BcmPacketioManagerTest, PushChassisConfigFailureForErrorInStartRx) {
  if (mode_ == OPERATION_MODE_SIM) return;  // no need to run in sim mode

  ChassisConfig config;
  ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId1, &config, nullptr));

  // Expected calls to BcmSdkInterface.
  EXPECT_CALL(*bcm_sdk_mock_, StartRx(kUnit1, _))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bcm_sdk_mock_, StopRx(kUnit1))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_THAT(PushChassisConfig(config, kNodeId1),
              EqualsStatus(DefaultError()));

  ASSERT_OK(Shutdown());
}

TEST_P(BcmPacketioManagerTest,
       PushChassisConfigFailureForBadDataFromChassisManager) {
  if (mode_ == OPERATION_MODE_SIM) return;  // no need to run in sim mode

  //--------------------------------------------------------------
  // Config push when the data got from BcmChassisManager is bad
  //--------------------------------------------------------------
  ChassisConfig config;
  std::map<uint32, SdkPort> port_id_to_sdk_port = {};
  ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId1, &config,
                                             &port_id_to_sdk_port));
  // Add a port which does not belong to node1.
  port_id_to_sdk_port.insert({kPortId2, {kUnit2, kLogicalPort2}});

  // Expected calls to BcmChassisManager for first config push.
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId1))
      .WillOnce(Return(port_id_to_sdk_port));

  // Track the socket FDs; These Fds are used for sockets etc and we need to
  // make sure we close them properly. Tracking them at LibcProxyMock ensures
  // that we know these Fds are used by the functions under test so that we use
  // the mocked version of close() for them.
  LibcProxyMock::Instance()->TrackFds({kSocket1, kSocket2, kEfd});

  // Expected libc calls for first config push.
  EXPECT_CALL(*LibcProxyMock::Instance(), Socket(_, _, _))
      .Times(6)
      .WillOnce(Return(kSocket1))
      .WillOnce(Return(kSocket1))
      .WillOnce(Return(kSocket1))
      .WillOnce(Return(kSocket2))
      .WillOnce(Return(kSocket2))
      .WillOnce(Return(kSocket2));

  // TODO(max): the ioctl call for SIOCSIFMTU is currently disabled because
  // SDKLT doesn't support it. See the comment in SetupSingleKnetIntf.
  EXPECT_CALL(*LibcProxyMock::Instance(), Ioctl(kSocket1, _, _))
      .Times(4)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Ioctl(kSocket2, _, _))
      .Times(4)
      .WillRepeatedly(Return(0));

  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1)).WillOnce(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket2)).WillOnce(Return(0));

  EXPECT_CALL(*LibcProxyMock::Instance(), SetSockOpt(kSocket1, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), SetSockOpt(kSocket2, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(0));

  EXPECT_CALL(*LibcProxyMock::Instance(), Bind(kSocket1, _, _))
      .WillOnce(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Bind(kSocket2, _, _))
      .WillOnce(Return(0));

  // Expected calls to BcmSdkInterface for first config push.
  EXPECT_CALL(*bcm_sdk_mock_, StartRx(kUnit1, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetIntf(kUnit1, 10, _, _))
      .Times(2)
      .WillRepeatedly(
          DoAll(SetArgPointee<3>(kNetifId), Return(::util::OkStatus())));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetFilter(kUnit1, _, kFilterTypeCatchAll))
      .WillOnce(Return(kCatchAllFilterId1));
  EXPECT_CALL(*bcm_sdk_mock_,
              CreateKnetFilter(kUnit1, _, kFilterTypeSflowIngress))
      .WillOnce(Return(kSflowIngressFilterId1));
  EXPECT_CALL(*bcm_sdk_mock_,
              CreateKnetFilter(kUnit1, _, kFilterTypeSflowEgress))
      .WillOnce(Return(kSflowEgressFilterId1));

  // Possible libc calls (triggered only if in the RX thread is spawned).
  EXPECT_CALL(*LibcProxyMock::Instance(), EpollCreate1(0))
      .WillRepeatedly(Return(kEfd));
  EXPECT_CALL(*LibcProxyMock::Instance(),
              EpollCtl(kEfd, EPOLL_CTL_ADD, kSocket1, _))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(),
              EpollCtl(kEfd, EPOLL_CTL_ADD, kSocket2, _))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), EpollWait(kEfd, _, 1, _))
      .WillRepeatedly(Return(0));  // 0 means no packet

  // Push the config and check the failure.
  ::util::Status status = PushChassisConfig(config, kNodeId1);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("1 != 0 for a singleton port"));

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CheckNoRxStats();
  }

  //--------------------------------------------------------------
  // Shutdown
  //--------------------------------------------------------------

  // Expected calls to BcmSdkInterface for shutdown.
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1))
      .Times(2)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket2))
      .Times(2)
      .WillRepeatedly(Return(0));

  // Expected calls to BcmSdkInterface for shutdown.
  EXPECT_CALL(*bcm_sdk_mock_, StopRx(kUnit1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetFilter(kUnit1, kCatchAllFilterId1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetFilter(kUnit1, kSflowIngressFilterId1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetFilter(kUnit1, kSflowEgressFilterId1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetIntf(kUnit1, kNetifId))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));

  // Possible libc calls (triggered only if in the RX thread is spawned).
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kEfd))
      .WillRepeatedly(Return(0));

  ASSERT_OK(Shutdown());

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CheckNoRxStats();
  }
}

TEST_P(BcmPacketioManagerTest, PushChassisConfigFailureForErrorInStartTx) {
  // TODO(unknown): At the moment, there is nothing to configure for TX. Add
  // test if we add things for StartTx.
}

TEST_P(BcmPacketioManagerTest,
       PushChassisConfigFailureForErrorInCreateKnetIntf) {
  if (mode_ == OPERATION_MODE_SIM) return;  // no need to run in sim mode

  ChassisConfig config;
  ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId1, &config, nullptr));

  // Expected calls to BcmSdkInterface.
  EXPECT_CALL(*bcm_sdk_mock_, StartRx(kUnit1, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, StopRx(kUnit1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetIntf(kUnit1, 10, _, _))
      .Times(1)
      .WillRepeatedly(Return(DefaultError()));

  EXPECT_THAT(PushChassisConfig(config, kNodeId1),
              EqualsStatus(DefaultError()));

  ASSERT_OK(Shutdown());
}

TEST_P(BcmPacketioManagerTest,
       PushChassisConfigFailureForErrorInCreateKnetFilter) {
  if (mode_ == OPERATION_MODE_SIM) return;  // no need to run in sim mode

  ChassisConfig config;
  ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId1, &config, nullptr));

  // Track the socket FDs;
  LibcProxyMock::Instance()->TrackFds({kSocket1});

  // Expected libc calls.
  EXPECT_CALL(*LibcProxyMock::Instance(), Socket(_, _, _))
      .Times(1)
      .WillOnce(Return(kSocket1));

  // TODO(max): the ioctl call for SIOCSIFMTU is currently disabled because
  // SDKLT doesn't support it. See the comment in SetupSingleKnetIntf.
  EXPECT_CALL(*LibcProxyMock::Instance(), Ioctl(kSocket1, _, _))
      .Times(4)
      .WillRepeatedly(Return(0));

  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1))
      .Times(1)
      .WillOnce(Return(0));

  // Expected calls to BcmSdkInterface.
  EXPECT_CALL(*bcm_sdk_mock_, StartRx(kUnit1, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetIntf(kUnit1, 10, _, _))
      .Times(1)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetFilter(kUnit1, _, kFilterTypeCatchAll))
      .WillOnce(Return(DefaultError()));
  EXPECT_CALL(*bcm_sdk_mock_, StopRx(kUnit1))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_THAT(PushChassisConfig(config, kNodeId1),
              EqualsStatus(DefaultError()));

  ASSERT_OK(Shutdown());
}

TEST_P(BcmPacketioManagerTest, VerifyChassisConfigSuccessBeforePush) {
  ChassisConfig config;
  ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId1, &config, nullptr));

  ASSERT_OK(VerifyChassisConfig(config, kNodeId1));
}

TEST_P(BcmPacketioManagerTest, VerifyChassisConfigFailureForInvalidNodeId) {
  ChassisConfig config;
  ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId1, &config, nullptr));

  ::util::Status status = VerifyChassisConfig(config, 0);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid node ID"));
}

TEST_P(BcmPacketioManagerTest,
       RegisterPacketReceiveWriterBeforeChassisConfigPush) {
  auto writer = std::make_shared<WriterMock<::p4::v1::PacketIn>>();
  ::util::Status status = RegisterPacketReceiveWriter(
      GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER, writer);
  if (mode_ == OPERATION_MODE_SIM) {
    // Skipped in case of sim mode.
    ASSERT_TRUE(status.ok());
  } else {
    ASSERT_FALSE(status.ok());
    EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
    EXPECT_THAT(
        status.error_message(),
        HasSubstr(
            "KNET interface with purpose BCM_KNET_INTF_PURPOSE_CONTROLLER "
            "does not exist for node with ID"));
  }
}

TEST_P(BcmPacketioManagerTest,
       RegisterPacketReceiveWriterAndReceivePacketAfterChassisConfigPush) {
  if (mode_ == OPERATION_MODE_SIM) return;  // no need to run in sim mode

  //--------------------------------------------------------------
  // Config push
  //--------------------------------------------------------------

  ChassisConfig config;
  std::map<uint32, SdkPort> port_id_to_sdk_port = {};
  ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId1, &config,
                                             &port_id_to_sdk_port));
  config.clear_vendor_config();  // default config

  // Expected calls to BcmChassisManager for first config push.
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId1))
      .WillOnce(Return(port_id_to_sdk_port));

  // Track the socket FDs;
  LibcProxyMock::Instance()->TrackFds({kSocket1, kEfd});

  // Expected libc calls for config push.
  EXPECT_CALL(*LibcProxyMock::Instance(), Socket(_, _, _))
      .Times(3)
      .WillRepeatedly(Return(kSocket1));
  // TODO(max): the ioctl call for SIOCSIFMTU is currently disabled because
  // SDKLT doesn't support it. See the comment in SetupSingleKnetIntf.
  EXPECT_CALL(*LibcProxyMock::Instance(), Ioctl(kSocket1, _, _))
      .Times(4)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1)).WillOnce(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), SetSockOpt(kSocket1, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Bind(kSocket1, _, _))
      .WillOnce(Return(0));

  // Expected calls to BcmSdkInterface for config push.
  EXPECT_CALL(*bcm_sdk_mock_, StartRx(kUnit1, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetIntf(kUnit1, kDefaultVlan, _, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<3>(kNetifId), Return(::util::OkStatus())));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetFilter(kUnit1, _, kFilterTypeCatchAll))
      .WillOnce(Return(kCatchAllFilterId1));

  // libc calls triggered by RX thread.
  EXPECT_CALL(*LibcProxyMock::Instance(), EpollCreate1(0))
      .WillRepeatedly(Return(kEfd));
  EXPECT_CALL(*LibcProxyMock::Instance(),
              EpollCtl(kEfd, EPOLL_CTL_ADD, kSocket1, _))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), EpollWait(kEfd, _, 1, _))
      .WillRepeatedly(DoAll(WithArgs<1>(Invoke([](struct epoll_event* p) {
                              p[0].events = EPOLLIN;
                            })),
                            Return(1)));  // 1 means RX packet is available
  EXPECT_CALL(*LibcProxyMock::Instance(), RecvMsg(kSocket1, _, _))
      .WillRepeatedly(DoAll(WithArgs<1>(Invoke([](struct msghdr* msg) {
                              // Any modification to msg goes here. Not needed
                              // At the moment.
                            })),
                            Return(kTestKnetHeaderSize + kTestPacketBodySize)));

  // BcmSdkInterface calls triggered by RX thread.
  EXPECT_CALL(*bcm_sdk_mock_, GetKnetHeaderSizeForRx(kUnit1))
      .WillRepeatedly(Return(kTestKnetHeaderSize));

  EXPECT_CALL(*bcm_sdk_mock_, ParseKnetHeaderForRx(kUnit1, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(kLogicalPort1),
                            SetArgPointee<3>(kCpuLogicalPort),
                            SetArgPointee<4>(5), Return(::util::OkStatus())));

  // BcmChassisRoInterface calls triggered by RX thread.
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetParentTrunkId(kNodeId1, kPortId1))
      .WillRepeatedly(Return(kTrunkId1));

  // P4TableMapper calls triggered by RX thread.
  MappedPacketMetadata mapped_packet_metadata1, mapped_packet_metadata2,
      mapped_packet_metadata3;
  mapped_packet_metadata1.set_type(P4_FIELD_TYPE_INGRESS_PORT);
  mapped_packet_metadata1.set_u32(kPortId1);
  mapped_packet_metadata2.set_type(P4_FIELD_TYPE_INGRESS_TRUNK);
  mapped_packet_metadata2.set_u32(kTrunkId1);
  mapped_packet_metadata3.set_type(P4_FIELD_TYPE_EGRESS_PORT);
  mapped_packet_metadata3.set_u32(kCpuPortId);

  EXPECT_CALL(*p4_table_mapper_mock_,
              DeparsePacketInMetadata(EqualsProto(mapped_packet_metadata1), _))
      .WillRepeatedly(
          DoAll(WithArgs<1>(Invoke([](::p4::v1::PacketMetadata* m) {
                  ParseProtoFromString(kTestPacketMetadata1, m).IgnoreError();
                })),
                Return(::util::OkStatus())));
  EXPECT_CALL(*p4_table_mapper_mock_,
              DeparsePacketInMetadata(EqualsProto(mapped_packet_metadata2), _))
      .WillRepeatedly(
          DoAll(WithArgs<1>(Invoke([](::p4::v1::PacketMetadata* m) {
                  ParseProtoFromString(kTestPacketMetadata1, m).IgnoreError();
                })),
                Return(::util::OkStatus())));
  EXPECT_CALL(*p4_table_mapper_mock_,
              DeparsePacketInMetadata(EqualsProto(mapped_packet_metadata3), _))
      .WillRepeatedly(
          DoAll(WithArgs<1>(Invoke([](::p4::v1::PacketMetadata* m) {
                  ParseProtoFromString(kTestPacketMetadata1, m).IgnoreError();
                })),
                Return(::util::OkStatus())));

  // Call PushChassisConfig to initialize the class. The RX thread will be
  // initialized as part of config push.
  ASSERT_OK(PushChassisConfig(config, kNodeId1));

  //--------------------------------------------------------------
  // Register packet receive handler
  //--------------------------------------------------------------
  auto writer = std::make_shared<WriterMock<::p4::v1::PacketIn>>();
  EXPECT_CALL(*writer, Write(_))
      .WillOnce(DoAll(InvokeWithoutArgs([this] {
                        absl::WriterMutexLock l(&rx_lock_);
                        rx_complete_ = true;
                      }),
                      Return(true)))
      .WillRepeatedly(Return(false));
  ASSERT_OK(RegisterPacketReceiveWriter(
      GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER, writer));

  // We now wait until a few packets are sent to the receive handler.
  while (!RxComplete()) {
  }  // no sleep, check as fast as possible

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CHECK_NON_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              all_rx);
    CHECK_NON_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              rx_accepts);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_errors_epoll_wait_failures);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_errors_internal_read_failures);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_errors_sock_shutdown);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_errors_incomplete_read);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_errors_invalid_packet);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_drops_knet_header_parse_error);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_drops_metadata_deparse_error);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_drops_unknown_ingress_port);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_drops_unknown_egress_port);
  }

  //--------------------------------------------------------------
  // Shutdown
  //--------------------------------------------------------------

  // Expected libc calls for shutdown.
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1))
      .Times(2)
      .WillRepeatedly(Return(0));

  // Expected calls to BcmSdkInterface for shutdown.
  EXPECT_CALL(*bcm_sdk_mock_, StopRx(kUnit1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetFilter(kUnit1, kCatchAllFilterId1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetIntf(kUnit1, kNetifId))
      .WillOnce(Return(::util::OkStatus()));

  // libc calls triggered by RX thread.
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kEfd))
      .WillRepeatedly(Return(0));

  ASSERT_OK(Shutdown());

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CheckNoRxStats();
  }
}

TEST_P(BcmPacketioManagerTest,
       RegisterPacketReceiveWriterAndHandleReceiveErrors) {
  if (mode_ == OPERATION_MODE_SIM) return;  // no need to run in sim mode

  //--------------------------------------------------------------
  // Config push
  //--------------------------------------------------------------

  ChassisConfig config;
  std::map<uint32, SdkPort> port_id_to_sdk_port = {};
  ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId1, &config,
                                             &port_id_to_sdk_port));
  config.clear_vendor_config();  // default config

  // Expected calls to BcmChassisManager for first config push.
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId1))
      .WillOnce(Return(port_id_to_sdk_port));

  // Track the socket FDs;
  LibcProxyMock::Instance()->TrackFds({kSocket1, kEfd});

  // Expected libc calls for config push.
  EXPECT_CALL(*LibcProxyMock::Instance(), Socket(_, _, _))
      .Times(3)
      .WillRepeatedly(Return(kSocket1));
  // TODO(max): the ioctl call for SIOCSIFMTU is currently disabled because
  // SDKLT doesn't support it. See the comment in SetupSingleKnetIntf.
  EXPECT_CALL(*LibcProxyMock::Instance(), Ioctl(kSocket1, _, _))
      .Times(4)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1)).WillOnce(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), SetSockOpt(kSocket1, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Bind(kSocket1, _, _))
      .WillOnce(Return(0));

  // Expected calls to BcmSdkInterface for config push.
  EXPECT_CALL(*bcm_sdk_mock_, StartRx(kUnit1, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetIntf(kUnit1, kDefaultVlan, _, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<3>(kNetifId), Return(::util::OkStatus())));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetFilter(kUnit1, _, kFilterTypeCatchAll))
      .WillOnce(Return(kCatchAllFilterId1));

  // libc calls triggered by RX thread.
  EXPECT_CALL(*LibcProxyMock::Instance(), EpollCreate1(0))
      .WillRepeatedly(Return(kEfd));
  EXPECT_CALL(*LibcProxyMock::Instance(),
              EpollCtl(kEfd, EPOLL_CTL_ADD, kSocket1, _))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), EpollWait(kEfd, _, 1, _))
      .WillRepeatedly(DoAll(WithArgs<1>(Invoke([](struct epoll_event* p) {
                              p[0].events = EPOLLIN;
                            })),
                            Return(1)));  // 1 means RX packet is available
  EXPECT_CALL(*LibcProxyMock::Instance(), RecvMsg(kSocket1, _, _))
      .WillRepeatedly(DoAll(WithArgs<1>(Invoke([](struct msghdr* msg) {
                              // Any modification to msg goes here. Not needed
                              // At the moment.
                            })),
                            Return(kTestKnetHeaderSize + kTestPacketBodySize)));

  // BcmSdkInterface calls triggered by RX thread.
  EXPECT_CALL(*bcm_sdk_mock_, GetKnetHeaderSizeForRx(kUnit1))
      .WillRepeatedly(Return(kTestKnetHeaderSize));

  // We expect the 4+ calls from ParseKnetHeaderForRx:
  // 1- An error in parsing --. will increment rx_drops_knet_header_parse_error
  // 2- Returns bad ingress port --> increments rx_drops_unknown_ingress_port
  // 3- Returns bad egress port --> increments rx_drops_unknown_egress_port
  // 4+- Return OK
  EXPECT_CALL(*bcm_sdk_mock_, ParseKnetHeaderForRx(kUnit1, _, _, _, _))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")))
      .WillOnce(DoAll(SetArgPointee<2>(kUnknownLogicalPort),
                      SetArgPointee<3>(kCpuLogicalPort), SetArgPointee<4>(5),
                      Return(::util::OkStatus())))
      .WillOnce(DoAll(SetArgPointee<2>(kLogicalPort1),  // unknown
                      SetArgPointee<3>(kUnknownLogicalPort),
                      SetArgPointee<4>(5), Return(::util::OkStatus())))
      .WillRepeatedly(DoAll(SetArgPointee<2>(kLogicalPort1),
                            SetArgPointee<3>(kCpuLogicalPort),
                            SetArgPointee<4>(5), Return(::util::OkStatus())));

  // We expect that all the calls to GetParentTrunkId fail due to port not
  // being part of any trunk.
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetParentTrunkId(kNodeId1, kPortId1))
      .WillRepeatedly(Return(
          ::util::Status(StratumErrorSpace(), ERR_INVALID_PARAM, "Blah")));

  // P4TableMapper calls triggered by RX thread.
  // We expect the 4+ calls from ParseKnetHeaderForRx:
  // 1-3- Returns OK. Corresponds to bad outputs from ParseKnetHeaderForRx.
  // 4- Return error --> increments rx_drops_metadata_deparse_error
  // 5+ - Returns OK
  EXPECT_CALL(*p4_table_mapper_mock_, DeparsePacketInMetadata(_, _))
      .WillOnce(Return(::util::OkStatus()))
      .WillOnce(Return(::util::OkStatus()))
      .WillOnce(Return(::util::OkStatus()))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")))
      .WillRepeatedly(
          DoAll(WithArgs<1>(Invoke([](::p4::v1::PacketMetadata* m) {
                  ParseProtoFromString(kTestPacketMetadata1, m).IgnoreError();
                })),
                Return(::util::OkStatus())));

  // Call PushChassisConfig to initialize the class. The RX thread will be
  // initialized as part of config push.
  ASSERT_OK(PushChassisConfig(config, kNodeId1));

  //--------------------------------------------------------------
  // Register packet receive handler
  //--------------------------------------------------------------
  auto writer = std::make_shared<WriterMock<::p4::v1::PacketIn>>();
  EXPECT_CALL(*writer, Write(_))
      .WillOnce(DoAll(InvokeWithoutArgs([this] {
                        absl::WriterMutexLock l(&rx_lock_);
                        rx_complete_ = true;
                      }),
                      Return(true)))
      .WillRepeatedly(Return(false));
  ASSERT_OK(RegisterPacketReceiveWriter(
      GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER, writer));

  // We now wait until a few packets are sent to the receive handler.
  while (!RxComplete()) {
  }  // no sleep, check as fast as possible

  // Based on the set of expections above, we will have the following situation:
  // - The 1st RX will increment rx_drops_knet_header_parse_error
  // - The 2nd RX will increment rx_drops_unknown_ingress_port
  // - The 3rd RX will increment rx_drops_unknown_egress_port
  // - The 4th RX will increment rx_drops_metadata_deparse_error
  // - All other will increment rx_accepts.
  // Also all RXs will increment all_rx as well.
  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CHECK_NON_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              all_rx);
    CHECK_NON_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              rx_accepts);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_errors_epoll_wait_failures);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_errors_internal_read_failures);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_errors_sock_shutdown);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_errors_incomplete_read);
    CHECK_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          rx_errors_invalid_packet);
    CHECK_NON_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              rx_drops_knet_header_parse_error);
    CHECK_NON_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              rx_drops_metadata_deparse_error);
    CHECK_NON_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              rx_drops_unknown_ingress_port);
    CHECK_NON_ZERO_RX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              rx_drops_unknown_egress_port);
  }

  //--------------------------------------------------------------
  // Shutdown
  //--------------------------------------------------------------

  // Expected libc calls for shutdown.
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1))
      .Times(2)
      .WillRepeatedly(Return(0));

  // Expected calls to BcmSdkInterface for shutdown.
  EXPECT_CALL(*bcm_sdk_mock_, StopRx(kUnit1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetFilter(kUnit1, kCatchAllFilterId1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetIntf(kUnit1, kNetifId))
      .WillOnce(Return(::util::OkStatus()));

  // libc calls triggered by RX thread.
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kEfd))
      .WillRepeatedly(Return(0));

  ASSERT_OK(Shutdown());

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CheckNoRxStats();
  }
}

TEST_P(BcmPacketioManagerTest, TransmitPacketBeforeChassisConfigPush) {
  ::p4::v1::PacketOut packet;

  ::util::Status status =
      TransmitPacket(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER, packet);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(
      status.error_message(),
      HasSubstr("KNET interface with purpose BCM_KNET_INTF_PURPOSE_CONTROLLER "
                "does not exist for node with ID"));

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CheckNoRxStats();
  }
}

TEST_P(BcmPacketioManagerTest, TransmitPacketAfterChassisConfigPush) {
  if (mode_ == OPERATION_MODE_SIM) return;  // no need to run in sim mode

  //--------------------------------------------------------------
  // Config push
  //--------------------------------------------------------------

  ChassisConfig config;
  std::map<uint32, SdkPort> port_id_to_sdk_port = {};
  ASSERT_OK(PopulateChassisConfigAndPortMaps(kNodeId1, &config,
                                             &port_id_to_sdk_port));
  config.clear_vendor_config();  // default config

  // Expected calls to BcmChassisManager for first config push.
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId1))
      .WillOnce(Return(port_id_to_sdk_port));

  // Track the socket FDs;
  LibcProxyMock::Instance()->TrackFds({kSocket1, kEfd});

  // Expected libc calls for config push.
  EXPECT_CALL(*LibcProxyMock::Instance(), Socket(_, _, _))
      .Times(3)
      .WillRepeatedly(Return(kSocket1));
  // TODO(max): the ioctl call for SIOCSIFMTU is currently disabled because
  // SDKLT doesn't support it. See the comment in SetupSingleKnetIntf.
  EXPECT_CALL(*LibcProxyMock::Instance(), Ioctl(kSocket1, _, _))
      .Times(4)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1)).WillOnce(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), SetSockOpt(kSocket1, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), Bind(kSocket1, _, _))
      .WillOnce(Return(0));

  // Expected calls to BcmSdkInterface for config push.
  EXPECT_CALL(*bcm_sdk_mock_, StartRx(kUnit1, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetIntf(kUnit1, kDefaultVlan, _, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<3>(kNetifId), Return(::util::OkStatus())));
  EXPECT_CALL(*bcm_sdk_mock_, CreateKnetFilter(kUnit1, _, kFilterTypeCatchAll))
      .WillOnce(Return(kCatchAllFilterId1));

  // Possible libc calls (triggered only if in the RX thread is spawned).
  EXPECT_CALL(*LibcProxyMock::Instance(), EpollCreate1(0))
      .WillRepeatedly(Return(kEfd));
  EXPECT_CALL(*LibcProxyMock::Instance(),
              EpollCtl(kEfd, EPOLL_CTL_ADD, kSocket1, _))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*LibcProxyMock::Instance(), EpollWait(kEfd, _, 1, _))
      .WillRepeatedly(Return(0));  // 0 means no packet

  // Call PushChassisConfig to initialize the class.
  ASSERT_OK(PushChassisConfig(config, kNodeId1));

  //--------------------------------------------------------------
  // Packet TX
  //--------------------------------------------------------------
  ::p4::v1::PacketOut packet;

  // 1- A packet with bad (unknown) port and some unknown meta (discarded).
  packet.set_payload(std::string(kTestPacket, sizeof(kTestPacket)));
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata1, packet.add_metadata()));
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata2, packet.add_metadata()));

  EXPECT_CALL(*p4_table_mapper_mock_,
              ParsePacketOutMetadata(EqualsProto(packet.metadata(0)), _))
      .WillOnce(DoAll(WithArgs<1>(Invoke([](MappedPacketMetadata* x) {
                        x->set_type(P4_FIELD_TYPE_EGRESS_PORT);
                        x->set_u32(9999);
                      })),
                      Return(::util::OkStatus())));
  EXPECT_CALL(*p4_table_mapper_mock_,
              ParsePacketOutMetadata(EqualsProto(packet.metadata(1)), _))
      .WillOnce(DoAll(WithArgs<1>(Invoke([](MappedPacketMetadata* x) {
                        x->set_type(P4_FIELD_TYPE_VRF);  // unknown meta
                      })),
                      Return(::util::OkStatus())));
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortState(kNodeId1, 9999))
      .WillOnce(Return(PORT_STATE_UP));

  ::util::Status status =
      TransmitPacket(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER, packet);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Port ID 9999 not found"));

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoRxStats();
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              all_tx);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_accepts_ingress_pipeline);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_accepts_direct);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_errors_internal_send_failures);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_errors_incomplete_send);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_drops_metadata_parse_error);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_unknown_port);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_drops_down_port);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_drops_down_trunk);
  }

  // 2- A packet with one metadata field poiting to a down port.
  packet.clear_metadata();
  ASSERT_OK(ParseProtoFromString(kTestPacketMetadata1, packet.add_metadata()));

  EXPECT_CALL(*p4_table_mapper_mock_,
              ParsePacketOutMetadata(EqualsProto(packet.metadata(0)), _))
      .WillOnce(DoAll(WithArgs<1>(Invoke([](MappedPacketMetadata* x) {
                        x->set_type(P4_FIELD_TYPE_EGRESS_PORT);
                        x->set_u32(kPortId1);
                      })),
                      Return(::util::OkStatus())));
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortState(kNodeId1, kPortId1))
      .WillOnce(Return(PORT_STATE_DOWN));

  status =
      TransmitPacket(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER, packet);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("is not UP"));

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoRxStats();
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              all_tx);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_accepts_ingress_pipeline);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_accepts_direct);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_errors_internal_send_failures);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_errors_incomplete_send);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_drops_metadata_parse_error);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_unknown_port);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_down_port);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_drops_down_trunk);
  }

  // 3- A packet with up port.
  EXPECT_CALL(*p4_table_mapper_mock_,
              ParsePacketOutMetadata(EqualsProto(packet.metadata(0)), _))
      .WillOnce(DoAll(WithArgs<1>(Invoke([](MappedPacketMetadata* x) {
                        x->set_type(P4_FIELD_TYPE_EGRESS_PORT);
                        x->set_u32(kPortId1);
                      })),
                      Return(::util::OkStatus())));
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortState(kNodeId1, kPortId1))
      .WillOnce(Return(PORT_STATE_UP));
  EXPECT_CALL(*bcm_sdk_mock_, GetKnetHeaderForDirectTx(kUnit1, kLogicalPort1,
                                                       kDefaultCos, _, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*LibcProxyMock::Instance(), SendMsg(kSocket1, _, _))
      .WillOnce(Return(64));  // 64 is tot_len of the packet.

  ASSERT_OK(
      TransmitPacket(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER, packet));

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoRxStats();
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              all_tx);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_accepts_ingress_pipeline);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_accepts_direct);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_errors_internal_send_failures);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_errors_incomplete_send);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_drops_metadata_parse_error);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_unknown_port);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_down_port);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_drops_down_trunk);
  }

  // 4- A packet pointing to a trunk with non empty members.
  EXPECT_CALL(*p4_table_mapper_mock_,
              ParsePacketOutMetadata(EqualsProto(packet.metadata(0)), _))
      .WillOnce(DoAll(WithArgs<1>(Invoke([](MappedPacketMetadata* x) {
                        x->set_type(P4_FIELD_TYPE_EGRESS_TRUNK);
                        x->set_u32(kTrunkId1);
                      })),
                      Return(::util::OkStatus())));

  EXPECT_CALL(*bcm_chassis_ro_mock_, GetTrunkMembers(kNodeId1, kTrunkId1))
      .WillOnce(Return(std::set<uint32>({kPortId1, kPortId2})));
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortState(kNodeId1, kPortId1))
      .WillOnce(Return(PORT_STATE_UP));
  EXPECT_CALL(*bcm_sdk_mock_, GetKnetHeaderForDirectTx(kUnit1, kLogicalPort1,
                                                       kDefaultCos, _, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*LibcProxyMock::Instance(), SendMsg(kSocket1, _, _))
      .WillOnce(Return(64));  // 64 is tot_len of the packet.

  ASSERT_OK(
      TransmitPacket(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER, packet));

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoRxStats();
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              all_tx);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_accepts_ingress_pipeline);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_accepts_direct);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_errors_internal_send_failures);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_errors_incomplete_send);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_drops_metadata_parse_error);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_unknown_port);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_down_port);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_drops_down_trunk);
  }

  // 5- A packet pointing to an empty trunk.
  EXPECT_CALL(*p4_table_mapper_mock_,
              ParsePacketOutMetadata(EqualsProto(packet.metadata(0)), _))
      .WillOnce(DoAll(WithArgs<1>(Invoke([](MappedPacketMetadata* x) {
                        x->set_type(P4_FIELD_TYPE_EGRESS_TRUNK);
                        x->set_u32(kTrunkId1);
                      })),
                      Return(::util::OkStatus())));

  EXPECT_CALL(*bcm_chassis_ro_mock_, GetTrunkMembers(kNodeId1, kTrunkId1))
      .WillOnce(Return(std::set<uint32>()));

  status =
      TransmitPacket(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER, packet);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("does not have any UP port"));

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoRxStats();
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              all_tx);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_accepts_ingress_pipeline);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_accepts_direct);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_errors_internal_send_failures);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_errors_incomplete_send);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_drops_metadata_parse_error);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_unknown_port);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_down_port);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_down_trunk);
  }

  // 6- A packet sent to ingress pipeline
  packet.clear_metadata();  // no metadata will send packet to ingress pipeline
  EXPECT_CALL(*bcm_sdk_mock_,
              GetKnetHeaderForIngressPipelineTx(kUnit1, _, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*LibcProxyMock::Instance(), SendMsg(kSocket1, _, _))
      .WillOnce(Return(64));  // 64 is tot_len of the packet.

  ASSERT_OK(
      TransmitPacket(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER, packet));

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoRxStats();
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              all_tx);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_accepts_ingress_pipeline);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_accepts_direct);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_errors_internal_send_failures);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_errors_incomplete_send);
    CHECK_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                          tx_drops_metadata_parse_error);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_unknown_port);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_down_port);
    CHECK_NON_ZERO_TX_COUNTER(GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER,
                              tx_drops_down_trunk);
  }

  //--------------------------------------------------------------
  // Shutdown
  //--------------------------------------------------------------

  // Expected libc calls for shutdown.
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kSocket1))
      .Times(2)
      .WillRepeatedly(Return(0));

  // Expected calls to BcmSdkInterface for shutdown.
  EXPECT_CALL(*bcm_sdk_mock_, StopRx(kUnit1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetFilter(kUnit1, kCatchAllFilterId1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DestroyKnetIntf(kUnit1, kNetifId))
      .WillOnce(Return(::util::OkStatus()));

  // Possible libc calls (triggered only if in the RX thread is spawned).
  EXPECT_CALL(*LibcProxyMock::Instance(), Close(kEfd))
      .WillRepeatedly(Return(0));

  ASSERT_OK(Shutdown());

  {
    SCOPED_TRACE(bcm_packetio_manager_->DumpStats());
    CheckNoTxStats();
    CheckNoRxStats();
  }
}

INSTANTIATE_TEST_SUITE_P(BcmPacketioManagerTestWithMode, BcmPacketioManagerTest,
                         ::testing::Values(OPERATION_MODE_STANDALONE,
                                           OPERATION_MODE_COUPLED,
                                           OPERATION_MODE_SIM));

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
