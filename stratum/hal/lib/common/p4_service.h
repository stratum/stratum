// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_P4_SERVICE_H_
#define STRATUM_HAL_LIB_COMMON_P4_SERVICE_H_

#include <pthread.h>

#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/numeric/int128.h"
#include "absl/synchronization/mutex.h"
#include "grpcpp/grpcpp.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/channel_writer_wrapper.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/hal/lib/common/switch_interface.h"
#include "stratum/hal/lib/p4/forwarding_pipeline_configs.pb.h"
#include "stratum/lib/p4runtime/sdn_controller_manager.h"
#include "stratum/lib/security/auth_policy_checker.h"

namespace stratum {
namespace hal {

// Typedefs for more readable reference.
typedef ::grpc::ServerReaderWriter<::p4::v1::StreamMessageResponse,
                                   ::p4::v1::StreamMessageRequest>
    ServerStreamChannelReaderWriter;

// The "P4Service" class implements P4Runtime::Service. It handles all
// the RPCs that are part of the P4-based PI API.
class P4Service final : public ::p4::v1::P4Runtime::Service {
 public:
  P4Service(OperationMode mode, SwitchInterface* switch_interface,
            AuthPolicyChecker* auth_policy_checker, ErrorBuffer* error_buffer);
  ~P4Service() override;

  // Sets up the service in coldboot and warmboot mode. In the coldboot mode,
  // the function initializes the class and pushes the saved forwarding pipeline
  // config to the switch. In the warmboot mode, it only restores the internal
  // state of the class.
  ::util::Status Setup(bool warmboot) LOCKS_EXCLUDED(config_lock_);

  // Tears down the class. Called in both warmboot or coldboot mode. It will
  // not alter any state on the hardware when called.
  ::util::Status Teardown() LOCKS_EXCLUDED(config_lock_, controller_lock_,
                                           stream_response_thread_lock_);

  // Public helper function called in Setup().
  ::util::Status PushSavedForwardingPipelineConfigs(bool warmboot)
      LOCKS_EXCLUDED(config_lock_);

  // Writes one or more forwarding entries on the target as part of P4 Runtime
  // API. Entries include tables entries, action profile members/groups, meter
  // entries, and counter entries.
  ::grpc::Status Write(::grpc::ServerContext* context,
                       const ::p4::v1::WriteRequest* req,
                       ::p4::v1::WriteResponse* resp) override;

  // Streams the forwarding entries, previously written on the target, out as
  // part of P4 Runtime API.
  ::grpc::Status Read(
      ::grpc::ServerContext* context, const ::p4::v1::ReadRequest* req,
      ::grpc::ServerWriter<::p4::v1::ReadResponse>* writer) override;

  // Pushes the P4-based forwarding pipeline configuration of one or more
  // switching nodes.
  ::grpc::Status SetForwardingPipelineConfig(
      ::grpc::ServerContext* context,
      const ::p4::v1::SetForwardingPipelineConfigRequest* req,
      ::p4::v1::SetForwardingPipelineConfigResponse* resp) override
      LOCKS_EXCLUDED(config_lock_);

  // Gets the P4-based forwarding pipeline configuration of one or more
  // switching nodes previously pushed to the switch.
  ::grpc::Status GetForwardingPipelineConfig(
      ::grpc::ServerContext* context,
      const ::p4::v1::GetForwardingPipelineConfigRequest* req,
      ::p4::v1::GetForwardingPipelineConfigResponse* resp) override
      LOCKS_EXCLUDED(config_lock_);

  // Bidirectional channel between controller and the switch for packet I/O,
  // master arbitration and stream errors.
  ::grpc::Status StreamChannel(
      ::grpc::ServerContext* context,
      ServerStreamChannelReaderWriter* stream) override;

  // Offers a mechanism through which a P4Runtime client can discover the
  // capabilities of the P4Runtime server implementation.
  ::grpc::Status Capabilities(
      ::grpc::ServerContext* context,
      const ::p4::v1::CapabilitiesRequest* request,
      ::p4::v1::CapabilitiesResponse* response) override;

  // P4Service is neither copyable nor movable.
  P4Service(const P4Service&) = delete;
  P4Service& operator=(const P4Service&) = delete;

 private:
  // ReaderArgs encapsulates the arguments for a Channel reader thread.
  template <typename T>
  struct ReaderArgs {
    P4Service* p4_service;
    std::unique_ptr<ChannelReader<T>> reader;
    uint64 node_id;
  };

  // Specifies the max number of controllers that can connect for a node.
  static constexpr size_t kMaxNumControllerPerNode = 5;

  // Checks and increments the number of active connections to make sure we do
  // not end with so many dangling threads. Called for every newly connected
  // controller, and before `AddOrModifyController`.
  ::util::Status CheckAndIncrementConnectionCount()
      LOCKS_EXCLUDED(controller_lock_);

  // Adds a new controller to the controller manager. If the election_id in the
  // 'arbitration' token is highest among the existing controllers (or if this
  // is the first controller that is connected), this controller will become
  // master. This functions also returns the appropriate resp back to the
  // remote controller client(s), while it has the controller_lock_ lock. This
  // will make sure the response is sent back to the client (in case a packet
  // is received right at the same time) before StreamResponseReceiveHandler()
  // takes the lock. After successful completion of this function, the
  // SdnControllerManager will have the master controller stream for packet I/O.
  ::util::Status AddOrModifyController(
      uint64 node_id, const ::p4::v1::MasterArbitrationUpdate& update,
      p4runtime::SdnConnection* controller) LOCKS_EXCLUDED(controller_lock_);

  // Removes an existing controller from the controller manager given its
  // stream. To be called after stream from an existing controller is broken
  // (e.g. controller is disconnected).
  void RemoveController(uint64 node_id, p4runtime::SdnConnection* connection)
      LOCKS_EXCLUDED(controller_lock_);

  // Returns true if given (election_id, role) for a Write request belongs to
  // the master controller stream for a node given by its node ID.
  ::grpc::Status IsWritePermitted(uint64 node_id,
                                  const ::p4::v1::WriteRequest& req) const
      LOCKS_EXCLUDED(controller_lock_);
  ::grpc::Status IsWritePermitted(
      uint64 node_id,
      const ::p4::v1::SetForwardingPipelineConfigRequest& req) const
      LOCKS_EXCLUDED(controller_lock_);

  // Returns true if given role for a Read request is allowed to read the
  // requested entities.
  ::grpc::Status IsReadPermitted(uint64 node_id,
                                 const ::p4::v1::ReadRequest& req) const
      LOCKS_EXCLUDED(controller_lock_);

  // Returns true if the given role and election_id belongs to the master
  // controller stream for a node given by its node ID.
  bool IsMasterController(
      uint64 node_id, const absl::optional<std::string>& role_name,
      const absl::optional<absl::uint128>& election_id) const
      LOCKS_EXCLUDED(controller_lock_);

  // Return the stored forwarding pipeline for the given node.
  ::util::StatusOr<::p4::v1::ForwardingPipelineConfig>
  DoGetForwardingPipelineConfig(uint64 node_id) const
      LOCKS_EXCLUDED(config_lock_);

  // Expands a generic wildcard request into individual entity wildcard reads.
  ::p4::v1::ReadRequest ExpandWildcardsInReadRequest(
      const ::p4::v1::ReadRequest& req,
      const ::p4::config::v1::P4Info& p4info) const
      LOCKS_EXCLUDED(controller_lock_);

  // Thread function for handling stream response RX.
  static void* StreamResponseReceiveThreadFunc(void* arg)
      LOCKS_EXCLUDED(controller_lock_);

  // Blocks on the Channel registered with SwitchInterface to read received
  // responses.
  void* ReceiveStreamRespones(
      uint64 node_id,
      std::unique_ptr<ChannelReader<::p4::v1::StreamMessageResponse>> reader)
      LOCKS_EXCLUDED(controller_lock_);

  // Callback to be called whenever we receive a stream response on the
  // specified node which is destined to controller.
  void StreamResponseReceiveHandler(uint64 node_id,
                                    const ::p4::v1::StreamMessageResponse& resp)
      LOCKS_EXCLUDED(controller_lock_);

  // Mutex lock used to protect node_id_to_controller_manager_ which is accessed
  // every time a controller connects, disconnects or wants to acquire
  // mastership. Additionally we read it whenever we need to check for
  // mastership authorization on a request.
  mutable absl::Mutex controller_lock_;

  // Mutex lock for protecting the internal forwarding pipeline configs pushed
  // to the switch.
  mutable absl::Mutex config_lock_;

  // Mutex which protects the creation and destruction of the stream response RX
  // Channels and threads.
  mutable absl::Mutex stream_response_thread_lock_;

  // P4Runtime can accept multiple connections to a single switch for
  // redundancy. When there is >1 connection the switch chooses a primary which
  // is used for PacketIO, and is the only connection allowed to write updates.
  //
  // It is possible for connections to be made for specific roles. In which case
  // one primary connection is allowed for each distinct role.
  std::unordered_map<uint64, p4runtime::SdnControllerManager>
      node_id_to_controller_manager_ ABSL_GUARDED_BY(controller_lock_);

  // Holds the number of currently open StreamChannels across all nodes. This is
  // tracked for resource limiting. Note that this count can be different from
  // the sum of connected controllers reported by all controller managers, as
  // a P4Runtime client can connect, but never send a arbitration message.
  int num_controller_connections_ GUARDED_BY(controller_lock_);

  // List of threads which send received responses up to the controller.
  std::vector<pthread_t> stream_response_reader_tids_
      GUARDED_BY(stream_response_thread_lock_);

  // Map of per-node Channels which are used to forward received responses to
  // P4Service.
  absl::flat_hash_map<uint64,
                      std::shared_ptr<Channel<::p4::v1::StreamMessageResponse>>>
      stream_response_channels_ GUARDED_BY(stream_response_thread_lock_);

  // Forwarding pipeline configs of all the switching nodes. Updated as we push
  // forwarding pipeline configs for new or existing nodes.
  std::unique_ptr<ForwardingPipelineConfigs> forwarding_pipeline_configs_
      GUARDED_BY(config_lock_);

  // Determines the mode of operation:
  // - OPERATION_MODE_STANDALONE: when Stratum stack runs independently and
  // therefore needs to do all the SDK initialization itself.
  // - OPERATION_MODE_COUPLED: when Stratum stack runs as part of Sandcastle
  // stack, coupled with the rest of stack processes.
  // - OPERATION_MODE_SIM: when Stratum stack runs in simulation mode.
  // Note that this variable is set upon initialization and is never changed
  // afterwards.
  OperationMode mode_;

  // Pointer to SwitchInterface implementation, which encapsulates all the
  // switch capabilities. Not owned by this class.
  SwitchInterface* switch_interface_;

  // Pointer to AuthPolicyChecker. Not owned by this class.
  AuthPolicyChecker* auth_policy_checker_;

  // Pointer to ErrorBuffer to save any critical errors we encounter. Not owned
  // by this class.
  ErrorBuffer* error_buffer_;

  friend class P4ServiceTest;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_P4_SERVICE_H_
