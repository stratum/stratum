// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// This file contains the code for a version of Stratum stub intended to be
// used on the embedded switches. Therefore the code here does not use any
// non-portable google3 code (e.g. no use of //net/grpc). Note that although
// this is intended to run on embedded switches, we still build a host version
// of this binary as well for cases when we run this stub on local desktops.

#include <arpa/inet.h>
#include <linux/filter.h>
#include <linux/if_ether.h>
#include <net/ethernet.h>

#include <sstream>
#include <string>

#include "absl/base/macros.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/numeric/int128.h"
#include "absl/synchronization/mutex.h"
#include "gflags/gflags.h"
#include "gnmi/gnmi.grpc.pb.h"
#include "google/protobuf/any.pb.h"
#include "google/rpc/code.pb.h"
#include "grpcpp/grpcpp.h"
#include "openconfig/openconfig.pb.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/openconfig_converter.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/hal/lib/p4/p4_table_mapper.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

DEFINE_string(url, stratum::kLocalStratumUrl,
              "URL for Stratum server to connect to.");
DEFINE_bool(push_open_config, false,
            "Issue gNMI Set RPC to Stratum to push OpenConfig-based config "
            "data to the switch. This option is used only when Stratum is "
            "used in standalone mode.");
DEFINE_string(test_oc_device_file, "",
              "Path to a test oc::Device text proto file. The proto will be "
              "serialized in SetRequest send to Stratum by gNMI Set RPC when "
              "--push_open_config is given.");
DEFINE_bool(push_forwarding_pipeline_config, false,
            "Issue P4Runtime SetForwardingPipelineConfig RPC to Stratum.");
DEFINE_string(test_p4_info_file, "",
              "Path to an optional P4Info text proto file. If specified, file "
              "content will be serialzied into the p4info field in "
              "ForwardingPipelineConfig proto pushed to the switch when "
              "--push_forwarding_pipeline_config is given.");
DEFINE_string(test_p4_pipeline_config_file, "",
              "Path to an optional P4PipelineConfig bin proto file. If "
              "specified, file content will be serialzied into the "
              "p4_device_config field in ForwardingPipelineConfig proto "
              "pushed to the switch when "
              "--push_forwarding_pipeline_config is given.");
DEFINE_bool(write_forwarding_entries, false,
            "Issue P4Runtime Write RPC to Stratum.");
DEFINE_string(test_write_request_file, "",
              "Path to a test WriteRequest text proto file. The proto will be "
              "passed to switch directly via P4Runtime Write RPC.");
DEFINE_bool(read_forwarding_entries, false,
            "Issue P4Runtime Read RPC to Stratum.");
DEFINE_bool(start_controller_session, false,
            "Start the controller streaming channel to Stratum. When set to  "
            "true, a controller session using the election ID given by "
            "FLAGS_election_id is started towards the node with ID given by "
            "FLAGS_node_id. The session stays active until the program exits.");
DEFINE_bool(packetio, false,
            "Used only when FLAGS_start_controller_session is true. If set to "
            "true, a TX thread will be spawned for a packet I/O demo.");
DEFINE_bool(loopback, false,
            "Used only when FLAGS_start_controller_session is true. Determines "
            "whether in packet I/O demo we should we loop all the RX packets "
            "back to the switch?");
DEFINE_string(test_packet_type, "ipv4",
              "The type of the pre-generated test packet used for packet I/O. "
              "The following types are currently supported: lldp, ipv4.");
DEFINE_uint64(node_id, 0,
              "Node ID in case the operation is for a specific node only. Must "
              "be > 0 in case it is needed.");
DEFINE_uint64(port_id, 0,
              "Port ID for whenever port info is needed (e.g. packet TX). Must "
              "be > 0 in case it is needed.");
DEFINE_uint64(election_id, 0,
              "Election ID for the controller instance. Will be used in all "
              "P4Runtime RPCs sent to the switch. Note that election_id is 128 "
              "bits, but here we assume we only give the lower 64 bits only.");
DEFINE_bool(start_gnmi_subscription_session, false,
            "Start sample gNMI subscription for most basic interface events.");

namespace stratum {
namespace hal {
namespace stub {

using ClientStreamChannelReaderWriter =
    ::grpc::ClientReaderWriter<::p4::v1::StreamMessageRequest,
                               ::p4::v1::StreamMessageResponse>;
using ClientSubscribeReaderWriterInterface =
    std::unique_ptr<::grpc::ClientReaderWriterInterface<
        ::gnmi::SubscribeRequest, ::gnmi::SubscribeResponse>>;

namespace {

#define LOG_RETURN_IF_ERROR(expr)                                 \
  do {                                                            \
    const ::util::Status& status = (expr);                        \
    if (!status.ok()) {                                           \
      LOG(ERROR) << #expr << " failed with the following error: " \
                 << status.error_message();                       \
      return;                                                     \
    }                                                             \
  } while (0)

#define CALL_RPC_AND_CHECK_RESULTS(stub, rpc, context, req, resp, logger)     \
  do {                                                                        \
    Timer timer;                                                              \
    timer.Start();                                                            \
    ::grpc::Status status = stub->rpc(&context, req, &resp);                  \
    timer.Stop();                                                             \
    LOG(INFO) << #rpc << " execution time (ms): " << timer.Get() << ".";      \
    if (!status.ok()) {                                                       \
      LOG(ERROR) << #rpc << " failed with the following error details: "      \
                 << logger(status);                                           \
      break;                                                                  \
    }                                                                         \
    const std::string& msg = resp.DebugString();                              \
    if (msg.empty()) {                                                        \
      LOG(INFO) << #rpc << " status: Success.";                               \
    } else {                                                                  \
      LOG(INFO) << #rpc << " status: Finished with the following response:\n" \
                << msg;                                                       \
    }                                                                         \
  } while (0)

// A test IPv4 packet. Was created using the following scapy command:
// pkt = Ether(dst="02:32:00:00:00:01",src="00:00:00:00:00:01")/Dot1Q(vlan=1)/
//       IP(src="10.0.1.1",dst="10.0.2.1",proto=254)/
//       Raw(load="Test, Test, Test, Test!!!")
constexpr char kTestIpv4Packet[] =
    "\x02\x32\x00\x00\x00\x01\x00\x00\x00\x00\x00\x01\x81\x00\x00\x01\x08\x00"
    "\x45\x00\x00\x2d\x00\x01\x00\x00\x40\xfe\x62\xd1\x0a\x00\x01\x01\x0a\x00"
    "\x02\x01\x54\x65\x73\x74\x2c\x20\x54\x65\x73\x74\x2c\x20\x54\x65\x73\x74"
    "\x2c\x20\x54\x65\x73\x74\x21\x21\x21";

// A test LLDP packet. Was created using the following scapy command:
// pkt = Ether(dst="01:80:C2:00:00:0E",src="11:22:33:44:55:66",type=0x88cc)/
//       Raw(load="Test, Test, Test, Test!!!")
constexpr char kTestLldpPacket[] =
    "\x01\x80\xc2\x00\x00\x0e\x11\x22\x33\x44\x55\x66\x88\xcc\x54\x65\x73\x74"
    "\x2c\x20\x54\x65\x73\x74\x2c\x20\x54\x65\x73\x74\x2c\x20\x54\x65\x73\x74"
    "\x21\x21\x21";

// A set of different packet types used for testing.
static enum {
  LLDP,
  IPV4,
} test_packet_type;

static bool ValidatePacketType(const char* flagname, const std::string& value) {
  if (value == "lldp") {
    test_packet_type = LLDP;
    return true;
  } else if (value == "ipv4") {
    test_packet_type = IPV4;
    return true;
  }
  // Cannot use LOG(ERROR) at this stage.
  printf("Unsupported --%s: %s.\n", flagname, value.c_str());
  return false;
}

static bool dummy __attribute__((__unused__)) =
    gflags::RegisterFlagValidator(&FLAGS_test_packet_type, &ValidatePacketType);

// A helper that initializes correctly ::gnmi::Path.
class GetPath {
 public:
  explicit GetPath(const std::string& name) {
    auto* elem = path_.add_elem();
    elem->set_name(name);
  }
  GetPath(const std::string& name, const std::string& search) {
    auto* elem = path_.add_elem();
    elem->set_name(name);
    (*elem->mutable_key())["name"] = search;
  }

  GetPath operator()(const std::string& name) {
    auto* elem = path_.add_elem();
    elem->set_name(name);
    return *this;
  }

  GetPath operator()(const std::string& name, const std::string& search) {
    auto* elem = path_.add_elem();
    elem->set_name(name);
    (*elem->mutable_key())["name"] = search;
    return *this;
  }

  ::gnmi::Path operator()() { return path_; }
  ::gnmi::Path path_;
};

// Helper to convert a gRPC status with error details to a string. Assumes
// ::grpc::Status includes a binary error detail which is encoding a serialized
// version of ::google::rpc::Status proto in which the details are captured
// using proto any messages.
// TODO(unknown): As soon as we can use internal libraries here,
// move to common/lib.
std::string ToString(const ::grpc::Status& status) {
  std::stringstream ss;
  if (!status.error_details().empty()) {
    ss << "(overall error code: "
       << ::google::rpc::Code_Name(ToGoogleRpcCode(status.error_code()))
       << ", overall error message: "
       << (status.error_message().empty() ? "None" : status.error_message())
       << "). Error details: ";
    ::google::rpc::Status details;
    if (!details.ParseFromString(status.error_details())) {
      ss << "Failed to parse ::google::rpc::Status from GRPC status details.";
    } else {
      for (int i = 0; i < details.details_size(); ++i) {
        ::google::rpc::Status detail;
        if (details.details(i).UnpackTo(&detail)) {
          ss << "\n(error #" << i + 1 << ": error code: "
             << ::google::rpc::Code_Name(ToGoogleRpcCode(detail.code()))
             << ", error message: "
             << (detail.message().empty() ? "None" : detail.message()) << ") ";
        }
      }
    }
  } else {
    ss << "(error code: "
       << ::google::rpc::Code_Name(ToGoogleRpcCode(status.error_code()))
       << ", error message: "
       << (status.error_message().empty() ? "None" : status.error_message())
       << ").";
  }

  return ss.str();
}

}  // namespace

class HalServiceClient {
 public:
  // Encapsulates the data passed to the TX thread.
  struct TxThreadData {
    uint64 node_id;
    ClientStreamChannelReaderWriter* stream;
    P4TableMapper* p4_table_mapper;
    TxThreadData() : node_id(0), stream(nullptr), p4_table_mapper(nullptr) {}
  };
  explicit HalServiceClient(const std::string& url)
      : config_monitoring_service_stub_(::gnmi::gNMI::NewStub(
            ::grpc::CreateChannel(url, ::grpc::InsecureChannelCredentials()))),
        p4_service_stub_(::p4::v1::P4Runtime::NewStub(::grpc::CreateChannel(
            url, ::grpc::InsecureChannelCredentials()))) {}

  void PushOpenConfig(const std::string& oc_device_file) {
    ::gnmi::SetRequest req;
    ::gnmi::SetResponse resp;
    ::grpc::ClientContext context;
    auto* replace = req.add_replace();
    ::openconfig::Device oc_device;
    LOG_RETURN_IF_ERROR(ReadProtoFromTextFile(oc_device_file, &oc_device));
    oc_device.SerializeToString(replace->mutable_val()->mutable_bytes_val());
    CALL_RPC_AND_CHECK_RESULTS(config_monitoring_service_stub_, Set, context,
                               req, resp, ToString);
  }

  void SetForwardingPipelineConfig(uint64 node_id, absl::uint128 election_id,
                                   const std::string& p4_info_file,
                                   const std::string& p4_pipeline_config_file) {
    if (node_id == 0 || election_id == 0) {
      LOG(ERROR) << "Need positive node_id and election_id. Got " << node_id
                 << " and " << election_id << ".";
      return;
    }

    ::p4::v1::SetForwardingPipelineConfigRequest req;
    ::p4::v1::SetForwardingPipelineConfigResponse resp;
    ::grpc::ClientContext context;
    req.set_device_id(node_id);
    req.mutable_election_id()->set_high(absl::Uint128High64(election_id));
    req.mutable_election_id()->set_low(absl::Uint128Low64(election_id));
    req.set_action(
        ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);
    LOG_RETURN_IF_ERROR(ReadProtoFromTextFile(
        p4_info_file, req.mutable_config()->mutable_p4info()));
    LOG_RETURN_IF_ERROR(
        ReadFileToString(p4_pipeline_config_file,
                         req.mutable_config()->mutable_p4_device_config()));
    CALL_RPC_AND_CHECK_RESULTS(p4_service_stub_, SetForwardingPipelineConfig,
                               context, req, resp, ToString);
  }

  void WriteForwardingEntries(uint64 node_id, absl::uint128 election_id,
                              const std::string& write_request_file) {
    if (node_id == 0 || election_id == 0) {
      LOG(ERROR) << "Need positive node_id and election_id. Got " << node_id
                 << " and " << election_id << ".";
      return;
    }

    ::p4::v1::WriteRequest req;
    ::p4::v1::WriteResponse resp;
    ::grpc::ClientContext context;
    LOG_RETURN_IF_ERROR(ReadProtoFromTextFile(write_request_file, &req));
    req.set_device_id(node_id);
    req.mutable_election_id()->set_high(absl::Uint128High64(election_id));
    req.mutable_election_id()->set_low(absl::Uint128Low64(election_id));
    CALL_RPC_AND_CHECK_RESULTS(p4_service_stub_, Write, context, req, resp,
                               ToString);
  }

  void ReadForwardingEntries(uint64 node_id) {
    if (node_id == 0) {
      LOG(ERROR) << "Need positive node_id. Got " << node_id << ".";
      return;
    }

    ::grpc::ClientContext context;
    ::p4::v1::ReadRequest req;
    ::p4::v1::ReadResponse resp;
    req.set_device_id(node_id);
    req.add_entities()->mutable_table_entry();
    req.add_entities()->mutable_action_profile_group();
    req.add_entities()->mutable_action_profile_member();
    std::unique_ptr<::grpc::ClientReader<::p4::v1::ReadResponse>> reader =
        p4_service_stub_->Read(&context, req);
    while (reader->Read(&resp)) {
      LOG(INFO) << "Read the following entities:\n" << resp.DebugString();
    }
    ::grpc::Status status = reader->Finish();
    if (!status.ok()) {
      LOG(ERROR) << "Failed to read the forwarding entries with the following "
                 << "error details: " << ToString(status);
    }
  }

  static void* TxPacket(void* arg) {
    TxThreadData* data = static_cast<TxThreadData*>(arg);
    // FIXME(boc) might use CHECK_NOTNULL instead of ABSL_DIE_IF_NULL
    ClientStreamChannelReaderWriter* stream = ABSL_DIE_IF_NULL(data->stream);
    P4TableMapper* p4_table_mapper = ABSL_DIE_IF_NULL(data->p4_table_mapper);
    ::p4::v1::StreamMessageRequest req;
    switch (test_packet_type) {
      case LLDP:
        req.mutable_packet()->set_payload(
            std::string(kTestLldpPacket, sizeof(kTestLldpPacket)));
        break;
      case IPV4:
        req.mutable_packet()->set_payload(
            std::string(kTestIpv4Packet, sizeof(kTestIpv4Packet)));
        break;
      default:
        LOG(FATAL) << "You should not get to this point!!!";
    }
    // Add egress port we got from FLAGS_port_id as metadata.
    MappedPacketMetadata mapped_packet_metadata;
    mapped_packet_metadata.set_type(P4_FIELD_TYPE_EGRESS_PORT);
    mapped_packet_metadata.set_u32(static_cast<uint32>(FLAGS_port_id));
    ::util::Status status = p4_table_mapper->DeparsePacketOutMetadata(
        mapped_packet_metadata, req.mutable_packet()->add_metadata());
    if (!status.ok()) {
      LOG(ERROR) << "DeparsePacketOutMetadata error: "
                 << status.error_message();
      return nullptr;
    }

    // NOTE: Write method seems to be thread-safe per gRPC code. Yet calling
    // Write concurrently in two thread leads to a crash. Had to use a lock.
    while (1) {
      absl::MutexLock l(&lock_);
      if (!stream->Write(req)) {
        LOG(ERROR) << "Failed to transmit packet '"
                   << req.packet().ShortDebugString() << "' to switch.";
        break;
      }
    }
    return nullptr;
  }

  void StartControllerSession(uint64 node_id, absl::uint128 election_id,
                              bool packetio, bool loopback,
                              const std::string& oc_device_file,
                              const std::string& p4_info_file,
                              const std::string& p4_pipeline_config_file) {
    // In case packet_io = true, p4_table_mapper pointer is passed to a TX
    // thread. Make sure you join the thread before you exit the function.
    std::unique_ptr<P4TableMapper> p4_table_mapper =
        P4TableMapper::CreateInstance();
    ::grpc::ClientContext context;
    std::unique_ptr<ClientStreamChannelReaderWriter> stream =
        p4_service_stub_->StreamChannel(&context);

    // The function does the following:
    // 1- Acts as a controller and send the election_id to the switch to
    //    participate in the master election. Depending on the election_id
    //    it will be either master or slave.
    // 2- Listen to all the packets received from the switch. And loops the
    //    packets back to the switch.
    ::p4::v1::StreamMessageRequest req;
    ::p4::v1::StreamMessageResponse resp;
    req.mutable_arbitration()->set_device_id(node_id);
    req.mutable_arbitration()->mutable_election_id()->set_high(
        absl::Uint128High64(election_id));
    req.mutable_arbitration()->mutable_election_id()->set_low(
        absl::Uint128Low64(election_id));
    if (!stream->Write(req)) {
      LOG(ERROR) << "Failed to send request '" << req.ShortDebugString()
                 << "' to switch.";
      return;
    }

    pthread_t tid = 0;  // will not be destroyed before the thread is joined.
    TxThreadData data;  // will not be destroyed before the thread is joined.
    if (packetio) {
      // OK, if packetio is requested, we will be using P4TableMapper instance
      // and before being able to use it we need to push configs to it. So read
      // the config from the file and push it to P4TableMapper before doing
      // any packet I/O.
      ::openconfig::Device oc_device;
      LOG_RETURN_IF_ERROR(ReadProtoFromTextFile(oc_device_file, &oc_device));
      auto ret = OpenconfigConverter::OcDeviceToChassisConfig(oc_device);
      if (!ret.ok()) {
        LOG(ERROR) << "Failed to convert ::oc::Device to ChassisConfig: "
                   << ret.status().error_message();
        return;
      }
      ChassisConfig chassis_config = ret.ValueOrDie();
      ::p4::v1::ForwardingPipelineConfig forwarding_pipeline_config;
      LOG_RETURN_IF_ERROR(ReadProtoFromTextFile(
          p4_info_file, forwarding_pipeline_config.mutable_p4info()));
      LOG_RETURN_IF_ERROR(ReadFileToString(
          p4_pipeline_config_file,
          forwarding_pipeline_config.mutable_p4_device_config()));
      LOG_RETURN_IF_ERROR(
          p4_table_mapper->PushChassisConfig(chassis_config, node_id));
      LOG_RETURN_IF_ERROR(p4_table_mapper->PushForwardingPipelineConfig(
          forwarding_pipeline_config));

      // Now create a thread to TX packets in parallel. We dont care if we are
      // master or not. We just blast the switch with packets :)
      data.node_id = node_id;
      data.stream = stream.get();
      data.p4_table_mapper = p4_table_mapper.get();
      if (pthread_create(&tid, nullptr, &TxPacket, &data) != 0) {
        LOG(ERROR) << "Failed to create the TX thread.";
        stream->Finish();
        return;
      }
    }

    bool master = false;  // Am I master for the switch?
    bool exit = false;
    while (stream->Read(&resp)) {
      switch (resp.update_case()) {
        case ::p4::v1::StreamMessageResponse::kArbitration: {
          master = (resp.arbitration().status().code() == ::google::rpc::OK);
          LOG(INFO) << "Mastership change. I am now "
                    << (master ? "MASTER!" : "SLAVE!");
          break;
        }
        case ::p4::v1::StreamMessageResponse::kPacket: {
          // First try to find the ingress port by parsing the packet metadata.
          uint32 ingress_port = 0;
          for (const auto& metadata : resp.packet().metadata()) {
            MappedPacketMetadata mapped_packet_metadata;
            ::util::Status status = p4_table_mapper->ParsePacketInMetadata(
                metadata, &mapped_packet_metadata);
            if (!status.ok()) {
              LOG(ERROR) << "ParsePacketInMetadata error: "
                         << status.error_message();
              break;
            }
            switch (mapped_packet_metadata.type()) {
              case P4_FIELD_TYPE_INGRESS_PORT:
                ingress_port = mapped_packet_metadata.u32();
                break;
              default:
                break;
            }
          }
          if (ingress_port == 0) {
            LOG(ERROR) << "Unknown ingress port: "
                       << resp.packet().ShortDebugString() << ".";
            break;
          }
          LOG_EVERY_N(INFO, 500)
              << "Received packet while being "
              << (master ? "MASTER on port " : "SLAVE on port ") << ingress_port
              << ":\n"
              << StringToHex(resp.packet().payload());

          if (master && loopback) {
            // Send the packet back to the same switch port.
            req.Clear();
            req.mutable_packet()->set_payload(resp.packet().payload());
            // Add egress port as metadata and set it to ingress port where
            // packet was received from.
            MappedPacketMetadata mapped_packet_metadata;
            mapped_packet_metadata.set_type(P4_FIELD_TYPE_EGRESS_PORT);
            mapped_packet_metadata.set_u32(ingress_port);
            ::util::Status status = p4_table_mapper->DeparsePacketOutMetadata(
                mapped_packet_metadata, req.mutable_packet()->add_metadata());
            if (!status.ok()) {
              LOG(ERROR) << "DeparsePacketOutMetadata error: "
                         << status.error_message();
              break;
            }
            {
              absl::MutexLock l(&lock_);
              if (!stream->Write(req)) {
                LOG(ERROR) << "Failed to transmit packet '"
                           << req.packet().ShortDebugString() << "' to switch.";
                exit = true;
              }
            }
          }
          break;
        }
        case ::p4::v1::StreamMessageResponse::kDigest:
        case ::p4::v1::StreamMessageResponse::kIdleTimeoutNotification:
        case ::p4::v1::StreamMessageResponse::UPDATE_NOT_SET:
          // TODO(stratum-dev): Handle kDigest and kIdleTimeoutNotification.
          LOG(ERROR) << "Invalid message received from the switch: "
                     << resp.ShortDebugString();
          break;
      }
      if (exit) break;
    }
    ::grpc::Status status = stream->Finish();
    if (!status.ok()) {
      LOG(ERROR) << "Stream failed with the following error: "
                 << status.error_message();
    }

    if (packetio) {
      // Make sure TX thread exits.
      if (pthread_join(tid, nullptr) != 0) {
        LOG(ERROR) << "Failed to join the TX thread.";
      }
    }
  }

  // A most basic scenario used by the controller:
  // - get names of all known interfaces using ONCE request
  // - subscribe for 'state/oper-state' leaf for all known interfaces using
  //   STREAM:ON_CHANGE
  // - subscribe for 'state/counters' sub-tree for one randomly selected known
  //   interface using STREAM:TARGET_DEFINED (which is for now translated to
  //   STREAM:SAMPLE)
  void StartGnmiSubscriptionSession() {
    LOG(INFO)
        << "Part 1: Use ONCE subscription to \"/interfaces/intrface/...\" to "
           "learn names of all known interfaces.";
    ::grpc::ClientContext context[3];
    auto stream = config_monitoring_service_stub_->Subscribe(&context[0]);
    if (!stream) {
      LOG(ERROR) << "cannot create a stream!";
      return;
    }

    // Build an ONCE subscription request for subtrees that are supported.
    ::gnmi::SubscribeRequest req;
    *req.mutable_subscribe()->add_subscription()->mutable_path() =
        GetPath("interfaces")("interface")("...")();
    req.mutable_subscribe()->set_mode(::gnmi::SubscriptionList::ONCE);

    // A map translating port ID into port name.
    absl::flat_hash_map<uint64, std::string> id_to_name;

    LOG(INFO) << "Sending ONCE subscription: " << req.ShortDebugString();
    if (!stream->Write(req)) {
      LOG(ERROR) << "Writing original subscribe request failed.";
      // Close the stream.
      stream->WritesDone();
      return;
    }

    // Now process all responses until 'sync_response' == true.
    ::gnmi::SubscribeResponse resp;
    while (!resp.sync_response()) {
      if (stream->Read(&resp)) {
        LOG(INFO) << "resp: " << resp.ShortDebugString();
        // Process all updates and store name of the interface listed within.
        for (const auto& update : resp.update().update()) {
          const auto& path = update.path();
          int len = path.elem_size();
          // Is this /interfaces/interface[name=<name>/status/ifindex?
          if (len > 1 && path.elem(len - 1).name().compare("ifindex") == 0 &&
              path.elem(1).name().compare("interface") == 0 &&
              path.elem(1).key().count("name")) {
            // Save mapping between 'ifindex' and 'name'
            id_to_name[update.val().uint_val()] = path.elem(1).key().at("name");
          }
        }
      }
    }
    // Close this stream.
    stream->WritesDone();

    std::string msg =
        absl::StrCat("Found ", id_to_name.size(),
                     id_to_name.size() != 1 ? " interfaces:" : " interface:");
    for (const auto& entry : id_to_name) {
      absl::StrAppend(&msg, "  ", entry.second);
    }
    LOG(INFO) << msg;

    // Second part of the scenario - subscribe for status of all known
    // interfaces.
    if (!id_to_name.empty()) {
      LOG(INFO) << "Part 2: STREAM:ON_CHANGE subscription to "
                   "\"/interfaces/interface/status/oper-status\" for all known "
                   "interfaces.";
      stream = config_monitoring_service_stub_->Subscribe(&context[1]);
      if (!stream) {
        LOG(ERROR) << "cannot create a stream!";
        return;
      }
      // Build a subscription request that will subscribe to all known
      // interfaces' "state/oper-status" leaf.
      ::gnmi::SubscribeRequest req;
      for (const auto& entry : id_to_name) {
        // Build STREAM:ON_CHANGE
        // /interfaces/interface[name=<interface_name>]/status/oper-status
        // subscription request.
        std::string interface_name = entry.second;
        ::gnmi::Subscription* subscription =
            req.mutable_subscribe()->add_subscription();
        *subscription->mutable_path() = GetPath("interfaces")(
            "interface", interface_name)("state")("oper-status")();
        subscription->set_mode(::gnmi::SubscriptionMode::ON_CHANGE);
      }
      req.mutable_subscribe()->set_mode(::gnmi::SubscriptionList::STREAM);

      LOG(INFO) << "Sending STREAM:ON_CHANGE subscription: "
                << req.ShortDebugString();
      if (!stream->Write(req)) {
        LOG(ERROR) << "Writing STREAM:ON_CHANGE subscribe request failed.";
        // Close the stream.
        stream->WritesDone();
        return;
      }
      ::gnmi::SubscribeResponse resp;

      // Now process all responses until 'sync_response' == true.
      int resp_count = 0;
      while (!resp.sync_response()) {
        if (stream->Read(&resp)) {
          LOG(INFO) << "resp: " << resp.ShortDebugString();
          // It should be an update. Let's process it.
          for (const auto& update : resp.update().update()) {
            const auto& path = update.path();
            int len = path.elem_size();
            // Is this one of the
            // "/interfaces/interface[name=<interface-name>]/" sub-tree leafs?
            if (len > 1 && path.elem(1).name() == "interface" &&
                gtl::ContainsKey(path.elem(1).key(), "name")) {
              ++resp_count;
            }
          }
        } else {
          // Something went wrong.
          LOG(ERROR) << "Read() operation returned FALSE.";
          break;
        }
      }
      LOG(INFO) << "Received " << resp_count << " initial responses.";

      // Close the stream.
      stream->WritesDone();
    }

    // Third part of the scenario - subscribe for counters on one of the known
    // interfaces.
    if (!id_to_name.empty()) {
      // Select one interface - it is not important which, so, let it be
      // the first from the map.
      std::string interface_name = (*id_to_name.begin()).second;
      LOG(INFO) << "Part 3: STREAM:TARGET_DEFINED subscription for: "
                << interface_name;
      stream = config_monitoring_service_stub_->Subscribe(&context[2]);
      if (!stream) {
        LOG(ERROR) << "cannot create a stream!";
        return;
      }
      // Build STREAM:TARGET_DEFINED /interfaces/interface/status/counters
      // subscription request.
      ::gnmi::SubscribeRequest req;
      ::gnmi::Subscription* subscription =
          req.mutable_subscribe()->add_subscription();
      *subscription->mutable_path() = GetPath("interfaces")(
          "interface", interface_name)("state")("counters")();
      subscription->set_mode(::gnmi::SubscriptionMode::TARGET_DEFINED);
      req.mutable_subscribe()->set_mode(::gnmi::SubscriptionList::STREAM);

      LOG(INFO) << "STREAM:TARGET_DEFINED subscription: "
                << req.ShortDebugString();
      if (!stream->Write(req)) {
        LOG(ERROR) << "Writing STREAM:TARGET_DEFINED subscribe request failed.";
        // Close the stream.
        stream->WritesDone();
        return;
      }

      // Now process 4 full sets of responses; each set has 14 counters.
      static constexpr int kNumStatisticsPerInterface = 14;
      for (int i = 0; i < 4 * kNumStatisticsPerInterface; ++i) {
        if (stream->Read(&resp)) {
          LOG(INFO) << "resp: " << resp.ShortDebugString();
        }
      }
      // Close the stream.
      stream->WritesDone();
    }
  }

 protected:
  std::unique_ptr<::gnmi::gNMI::Stub> config_monitoring_service_stub_;
  std::unique_ptr<::p4::v1::P4Runtime::Stub> p4_service_stub_;
  // Synchronizes Write method in two streaming channels.
  static absl::Mutex lock_;
};

ABSL_CONST_INIT absl::Mutex HalServiceClient::lock_(absl::kConstInit);

int Main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();
  HalServiceClient client(FLAGS_url);
  if (FLAGS_push_open_config) {
    client.PushOpenConfig(FLAGS_test_oc_device_file);
  } else if (FLAGS_push_forwarding_pipeline_config) {
    client.SetForwardingPipelineConfig(
        FLAGS_node_id, absl::uint128(static_cast<uint64>(FLAGS_election_id)),
        FLAGS_test_p4_info_file, FLAGS_test_p4_pipeline_config_file);
  } else if (FLAGS_write_forwarding_entries) {
    client.WriteForwardingEntries(
        FLAGS_node_id, absl::uint128(static_cast<uint64>(FLAGS_election_id)),
        FLAGS_test_write_request_file);
  } else if (FLAGS_read_forwarding_entries) {
    client.ReadForwardingEntries(FLAGS_node_id);
  } else if (FLAGS_start_controller_session) {
    client.StartControllerSession(
        FLAGS_node_id, absl::uint128(static_cast<uint64>(FLAGS_election_id)),
        FLAGS_packetio, FLAGS_loopback, FLAGS_test_oc_device_file,
        FLAGS_test_p4_info_file, FLAGS_test_p4_pipeline_config_file);
  } else if (FLAGS_start_gnmi_subscription_session) {
    client.StartGnmiSubscriptionSession();
  } else {
    LOG(ERROR) << "Invalid command.";
  }

  return 0;
}

}  // namespace stub
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  FLAGS_logtostderr = true;
  return stratum::hal::stub::Main(argc, argv);
}
