// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/common/p4_service.h"

#include <functional>
#include <sstream>  // IWYU pragma: keep
#include <utility>

#include "absl/cleanup/cleanup.h"
#include "absl/memory/memory.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "gflags/gflags.h"
#include "google/protobuf/any.pb.h"
#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/common/server_writer_wrapper.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

DEFINE_string(forwarding_pipeline_configs_file,
              "/etc/stratum/pipeline_cfg.pb.txt",
              "The latest set of verified ForwardingPipelineConfig protos "
              "pushed to the switch. This file is updated whenever "
              "ForwardingPipelineConfig proto for switching node is added or "
              "modified.");
DEFINE_string(write_req_log_file, "/var/log/stratum/p4_writes.pb.txt",
              "The log file for all the individual write request updates and "
              "the corresponding result. The format for each line is: "
              "<timestamp>;<node_id>;<update proto>;<status>.");
DEFINE_string(read_req_log_file, "/var/log/stratum/p4_reads.pb.txt",
              "The log file for all the individual read request and "
              "the corresponding result. The format for each line is: "
              "<timestamp>;<node_id>;<request proto>;<status>.");
DEFINE_int32(max_num_controllers_per_node, 5,
             "Max number of controllers that can manage a node.");
DEFINE_int32(max_num_controller_connections, 20,
             "Max number of active/inactive streaming connections from outside "
             "controllers (for all of the nodes combined).");

namespace stratum {
namespace hal {

// TODO(unknown): This class moves possibly big configs in memory. See if there
// is a way to make this more efficient.

P4Service::P4Service(OperationMode mode, SwitchInterface* switch_interface,
                     AuthPolicyChecker* auth_policy_checker,
                     ErrorBuffer* error_buffer)
    : node_id_to_controller_manager_(),
      num_controller_connections_(),
      forwarding_pipeline_configs_(nullptr),
      mode_(mode),
      switch_interface_(ABSL_DIE_IF_NULL(switch_interface)),
      auth_policy_checker_(ABSL_DIE_IF_NULL(auth_policy_checker)),
      error_buffer_(ABSL_DIE_IF_NULL(error_buffer)) {}

P4Service::~P4Service() {}

::util::Status P4Service::Setup(bool warmboot) {
  // If we are in coupled mode and are coldbooting, we wait for the controller
  // to push the forwarding pipeline config. We do not do anything here.
  // TODO(unknown): This will be removed when we transition completely to
  // standalone mode.
  if (!warmboot && mode_ == OPERATION_MODE_COUPLED) {
    LOG(INFO) << "Skipped pushing the saved forwarding pipeline config(s) in "
              << "coupled mode when coldbooting.";
    return ::util::OkStatus();
  }

  return PushSavedForwardingPipelineConfigs(warmboot);
}

::util::Status P4Service::Teardown() {
  {
    absl::WriterMutexLock l(&controller_lock_);
    node_id_to_controller_manager_.clear();
    num_controller_connections_ = 0;
  }
  {
    absl::WriterMutexLock l(&stream_response_thread_lock_);
    // Unregister writers and close PacketIn Channels.
    for (const auto& pair : stream_response_channels_) {
      auto status =
          switch_interface_->UnregisterStreamMessageResponseWriter(pair.first);
      if (!status.ok()) {
        LOG(ERROR) << status;
      }
      pair.second->Close();
    }
    stream_response_channels_.clear();
    // Join threads.
    for (const auto& tid : stream_response_reader_tids_) {
      int ret = pthread_join(tid, nullptr);
      if (ret) {
        LOG(ERROR) << "Failed to join thread " << tid << " with error " << ret
                   << ".";
      }
    }
  }
  {
    absl::WriterMutexLock l(&config_lock_);
    forwarding_pipeline_configs_ = nullptr;
  }

  return ::util::OkStatus();
}

::util::Status P4Service::PushSavedForwardingPipelineConfigs(bool warmboot) {
  // Try to read the saved forwarding pipeline configs for all the nodes and
  // push them to the nodes.
  LOG(INFO) << "Pushing the saved forwarding pipeline configs read from "
            << FLAGS_forwarding_pipeline_configs_file << "...";
  absl::WriterMutexLock l(&config_lock_);
  ForwardingPipelineConfigs configs;
  ::util::Status status =
      ReadProtoFromTextFile(FLAGS_forwarding_pipeline_configs_file, &configs);
  if (!status.ok()) {
    if (!warmboot && status.error_code() == ERR_FILE_NOT_FOUND) {
      // Not a critical error. If coldboot, we don't even return error.
      LOG(WARNING) << "No saved forwarding pipeline config found at "
                   << FLAGS_forwarding_pipeline_configs_file
                   << ". This is normal when the switch is just installed and "
                   << "no master controller is connected yet.";
      return ::util::OkStatus();
    }
    error_buffer_->AddError(
        status,
        "Could not read the saved forwarding pipeline configs: ", GTL_LOC);
    return status;
  }
  if (configs.node_id_to_config_size() == 0) {
    LOG(WARNING) << "Empty forwarding pipeline configs file: "
                 << FLAGS_forwarding_pipeline_configs_file << ".";
    return ::util::OkStatus();
  }

  // Push the forwarding pipeline config for all the nodes we know about. Push
  // the config to hardware only if it is a coldboot setup.
  forwarding_pipeline_configs_ = absl::make_unique<ForwardingPipelineConfigs>();
  if (!warmboot) {
    for (const auto& e : configs.node_id_to_config()) {
      ::util::Status error =
          switch_interface_->PushForwardingPipelineConfig(e.first, e.second);
      if (!error.ok()) {
        error_buffer_->AddError(
            error,
            absl::StrCat("Failed to push the saved forwarding pipeline configs "
                         "for node ",
                         e.first, ": "),
            GTL_LOC);
        APPEND_STATUS_IF_ERROR(status, error);
      } else {
        (*forwarding_pipeline_configs_->mutable_node_id_to_config())[e.first] =
            e.second;
      }
    }
  } else {
    // In the case of warmboot, the assumption is that the configs saved into
    // file are the latest configs which were already pushed to one or more
    // nodes.
    *forwarding_pipeline_configs_ = configs;
  }

  return status;
}

namespace {

// Run a command that returns a ::grpc::Status.  If the called code returns an
// error status, return that status up out of this method too.
//
// Example:
//   RETURN_IF_GRPC_ERROR(DoGrpcThings(4));
#define RETURN_IF_GRPC_ERROR(expr)                                           \
  do {                                                                       \
    /* Using _status below to avoid capture problems if expr is "status". */ \
    const ::grpc::Status _status = (expr);                                   \
    if (ABSL_PREDICT_FALSE(!_status.ok())) {                                 \
      return _status;                                                        \
    }                                                                        \
  } while (0)

// TODO(unknown): This needs to be changed later per p4 runtime error
// reporting scheme.
::grpc::Status ToGrpcStatus(const ::util::Status& status,
                            const std::vector<::util::Status>& details) {
  // We need to create a ::google::rpc::Status and populate it with all the
  // details, then convert it to ::grpc::Status.
  ::google::rpc::Status from;
  if (!status.ok()) {
    from.set_code(ToGoogleRpcCode(status.CanonicalCode()));
    from.set_message(status.error_message());
    // Add individual errors only when the top level error code is not OK.
    for (const auto& detail : details) {
      // Each individual detail is converted to another ::google::rpc::Status,
      // which is then serialized as one proto any in 'from' message above.
      ::p4::v1::Error error;
      if (!detail.ok()) {
        error.set_canonical_code(ToGoogleRpcCode(detail.CanonicalCode()));
        error.set_code(detail.error_code());
        error.set_message(detail.error_message());
      } else {
        error.set_code(::google::rpc::OK);
      }
      from.add_details()->PackFrom(error);
    }
  } else {
    from.set_code(::google::rpc::OK);
  }

  return ::grpc::Status(ToGrpcCode(from.code()), from.message(),
                        from.SerializeAsString());
}

// Helper to facilitate logging the write requests to the desired log file.
void LogWriteRequest(uint64 node_id, const ::p4::v1::WriteRequest& req,
                     const std::vector<::util::Status>& results,
                     const absl::Time timestamp) {
  if (FLAGS_write_req_log_file.empty()) {
    return;
  }
  if (results.empty()) {
    // Nothing to log as the switch interface did not fill in any error details.
    // TODO(max): Consider logging the requests with the overall status in this
    //            case. But keep in mind that LogWriteRequest will not be called
    //            for auth errors or invalid device IDs.
    return;
  }
  if (results.size() != req.updates_size()) {
    LOG(ERROR) << "Size mismatch: " << results.size()
               << " != " << req.updates_size() << ". Did not log anything!";
    return;
  }
  std::string msg = "";
  std::string ts =
      absl::FormatTime("%Y-%m-%d %H:%M:%E6S", timestamp, absl::LocalTimeZone());
  for (size_t i = 0; i < results.size(); ++i) {
    absl::StrAppend(&msg, ts, ";", node_id, ";",
                    req.updates(i).ShortDebugString(), ";",
                    results[i].error_message(), "\n");
  }
  ::util::Status status =
      WriteStringToFile(msg, FLAGS_write_req_log_file, /*append=*/true);
  LOG_IF_EVERY_N(ERROR, !status.ok(), 50)
      << "Failed to log the write request: " << status.error_message();
}

// Helper to facilitate logging the read requests to the desired log file.
void LogReadRequest(uint64 node_id, const ::p4::v1::ReadRequest& req,
                    const std::vector<::util::Status>& results,
                    const absl::Time timestamp) {
  if (FLAGS_read_req_log_file.empty()) {
    return;
  }
  if (results.empty()) {
    // Nothing to log as the switch interface did not fill in any error details.
    // TODO(max): Consider logging the requests with the overall status in this
    //            case. But keep in mind that LogReadRequest will not be called
    //            for auth errors or invalid device IDs.
    return;
  }
  if (results.size() != req.entities_size()) {
    LOG(ERROR) << "Size mismatch: " << results.size()
               << " != " << req.entities_size() << ". Did not log anything!";
    return;
  }
  std::string msg = "";
  std::string ts =
      absl::FormatTime("%Y-%m-%d %H:%M:%E6S", timestamp, absl::LocalTimeZone());
  for (size_t i = 0; i < results.size(); ++i) {
    absl::StrAppend(&msg, ts, ";", node_id, ";",
                    req.entities(i).ShortDebugString(), ";",
                    results[i].error_message(), "\n");
  }
  ::util::Status status =
      WriteStringToFile(msg, FLAGS_read_req_log_file, /*append=*/true);
  LOG_IF_EVERY_N(ERROR, !status.ok(), 50)
      << "Failed to log the read request: " << status.error_message();
}

// Helper function to generate a StreamMessageResponse from a failed Status.
::p4::v1::StreamMessageResponse ToStreamMessageResponse(
    const ::util::Status& status) {
  CHECK(!status.ok());
  ::p4::v1::StreamMessageResponse resp;
  auto stream_error = resp.mutable_error();
  stream_error->set_canonical_code(ToGoogleRpcCode(status.CanonicalCode()));
  stream_error->set_message(status.error_message());
  stream_error->set_code(status.error_code());

  return resp;
}

}  // namespace

::grpc::Status P4Service::Write(::grpc::ServerContext* context,
                                const ::p4::v1::WriteRequest* req,
                                ::p4::v1::WriteResponse* resp) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, P4Service, Write, context);

  if (!req->updates_size()) return ::grpc::Status::OK;  // Nothing to do.

  // device_id is nothing but the node_id specified in the config for the node.
  uint64 node_id = req->device_id();
  if (node_id == 0) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "Invalid device ID.");
  }

  // Check that a forwarding config is present.
  auto ret = DoGetForwardingPipelineConfig(node_id);
  if (!ret.ok()) {
    return ::grpc::Status(ToGrpcCode(ret.status().CanonicalCode()),
                          ret.status().error_message());
  }

  // Require valid election_id for Write.
  absl::uint128 election_id =
      absl::MakeUint128(req->election_id().high(), req->election_id().low());
  if (election_id == 0) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "Invalid election ID.");
  }

  // Verify the request comes from the primary connection.
  RETURN_IF_GRPC_ERROR(IsWritePermitted(req->device_id(), *req));

  std::vector<::util::Status> results = {};
  absl::Time timestamp = absl::Now();
  ::util::Status status =
      switch_interface_->WriteForwardingEntries(*req, &results);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to write forwarding entries to node " << node_id
               << ": " << status.error_message();
  }

  // Log debug info for future debugging.
  LogWriteRequest(node_id, *req, results, timestamp);

  return ToGrpcStatus(status, results);
}

::grpc::Status P4Service::Read(
    ::grpc::ServerContext* context, const ::p4::v1::ReadRequest* req,
    ::grpc::ServerWriter<::p4::v1::ReadResponse>* writer) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, P4Service, Read, context);

  if (!req->entities_size()) return ::grpc::Status::OK;
  // device_id is nothing but the node_id specified in the config for the node.
  uint64 node_id = req->device_id();
  if (node_id == 0) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "Invalid device ID.");
  }

  // Check that a forwarding config is present.
  auto ret = DoGetForwardingPipelineConfig(node_id);
  if (!ret.ok()) {
    return ::grpc::Status(ToGrpcCode(ret.status().CanonicalCode()),
                          ret.status().error_message());
  }

  // To allow role config read filtering in wildcard requests, we have to expand
  // wildcard reads targeting all tables into individual table wildcards. At the
  // same time, we must not include entities disallowed by the role config, else
  // the request fill be denied erroneously later.
  const ::p4::v1::ReadRequest* original_req = req;  // For later logging.
  ::p4::v1::ReadRequest expanded_req;
  if (!req->role().empty()) {
    expanded_req =
        ExpandWildcardsInReadRequest(*req, ret.ValueOrDie().p4info());
    req = &expanded_req;
    VLOG(1) << "Expanded wildcard read into "
            << expanded_req.ShortDebugString();
  }

  // Verify the request only contains entities allowed by the role config.
  RETURN_IF_GRPC_ERROR(IsReadPermitted(req->device_id(), *req));

  ServerWriterWrapper<::p4::v1::ReadResponse> wrapper(writer);
  std::vector<::util::Status> details = {};
  absl::Time timestamp = absl::Now();
  ::util::Status status =
      switch_interface_->ReadForwardingEntries(*req, &wrapper, &details);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to read forwarding entries from node " << node_id
               << ": " << status.error_message();
  }

  // Log debug info for future debugging.
  LogReadRequest(node_id, *original_req, details, timestamp);

  return ToGrpcStatus(status, details);
}

::grpc::Status P4Service::SetForwardingPipelineConfig(
    ::grpc::ServerContext* context,
    const ::p4::v1::SetForwardingPipelineConfigRequest* req,
    ::p4::v1::SetForwardingPipelineConfigResponse* resp) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, P4Service,
                           SetForwardingPipelineConfig, context);

  // device_id is nothing but the node_id specified in the config for the node.
  uint64 node_id = req->device_id();
  if (node_id == 0) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "Invalid device ID.");
  }

  // We need valid election ID for SetForwardingPipelineConfig RPC
  absl::uint128 election_id =
      absl::MakeUint128(req->election_id().high(), req->election_id().low());
  if (election_id == 0) {
    return ::grpc::Status(
        ::grpc::StatusCode::INVALID_ARGUMENT,
        absl::StrCat("Invalid election ID for node ", node_id, "."));
  }

  // Make sure this node already has a master controller and the given
  // election_id and the role of the client matches those of the master.
  // According to the P4Runtime specification, only master can perform
  // SetForwardingPipelineConfig RPC.
  RETURN_IF_GRPC_ERROR(IsWritePermitted(req->device_id(), *req));

  ::util::Status status = ::util::OkStatus();
  switch (req->action()) {
    case ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY:
      APPEND_STATUS_IF_ERROR(status,
                             switch_interface_->VerifyForwardingPipelineConfig(
                                 node_id, req->config()));
      break;
    case ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT:
    case ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_SAVE: {
      absl::WriterMutexLock l(&config_lock_);
      // configs_to_save_in_file will have a copy of the configs that will be
      // saved in file at the end. Note that this copy may NOT be the same as
      // forwarding_pipeline_configs_.
      ForwardingPipelineConfigs configs_to_save_in_file;
      if (forwarding_pipeline_configs_ != nullptr) {
        configs_to_save_in_file = *forwarding_pipeline_configs_;
      } else {
        forwarding_pipeline_configs_ =
            absl::make_unique<ForwardingPipelineConfigs>();
      }
      ::util::Status error;
      if (req->action() ==
          ::p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT) {
        error = switch_interface_->PushForwardingPipelineConfig(node_id,
                                                                req->config());
      } else {
        // VERIFY_AND_SAVE
        error = switch_interface_->SaveForwardingPipelineConfig(node_id,
                                                                req->config());
      }
      APPEND_STATUS_IF_ERROR(status, error);
      // If the config push was successful or reported reboot required, save
      // the config in file. But only mutate the internal copy if we status
      // was OK.
      // TODO(unknown): this may not be appropriate for the VERIFY_AND_SAVE ->
      // COMMIT sequence of operations.
      if (error.ok() || error.error_code() == ERR_REBOOT_REQUIRED) {
        (*configs_to_save_in_file.mutable_node_id_to_config())[node_id] =
            req->config();
        APPEND_STATUS_IF_ERROR(
            status,
            WriteProtoToTextFile(configs_to_save_in_file,
                                 FLAGS_forwarding_pipeline_configs_file));
      }
      if (error.ok()) {
        (*forwarding_pipeline_configs_->mutable_node_id_to_config())[node_id] =
            req->config();
      }
      break;
    }
    case ::p4::v1::SetForwardingPipelineConfigRequest::COMMIT: {
      ::util::Status error =
          switch_interface_->CommitForwardingPipelineConfig(node_id);
      APPEND_STATUS_IF_ERROR(status, error);
      break;
    }
    case ::p4::v1::SetForwardingPipelineConfigRequest::RECONCILE_AND_COMMIT:
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED,
                            "RECONCILE_AND_COMMIT action not supported yet");
    default:
      return ::grpc::Status(
          ::grpc::StatusCode::INVALID_ARGUMENT,
          absl::StrCat("Invalid action passed for node ", node_id, "."));
  }

  if (!status.ok()) {
    error_buffer_->AddError(
        status,
        absl::StrCat("Failed to set forwarding pipeline config for node ",
                     node_id, ": "),
        GTL_LOC);
    return ::grpc::Status(ToGrpcCode(status.CanonicalCode()),
                          status.error_message());
  }

  return ::grpc::Status::OK;
}

::grpc::Status P4Service::GetForwardingPipelineConfig(
    ::grpc::ServerContext* context,
    const ::p4::v1::GetForwardingPipelineConfigRequest* req,
    ::p4::v1::GetForwardingPipelineConfigResponse* resp) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, P4Service,
                           GetForwardingPipelineConfig, context);

  // device_id is nothing but the node_id specified in the config for the node.
  uint64 node_id = req->device_id();
  if (node_id == 0) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "Invalid device ID.");
  }

  auto status = DoGetForwardingPipelineConfig(node_id);
  if (!status.ok()) {
    return ::grpc::Status(ToGrpcCode(status.status().CanonicalCode()),
                          status.status().error_message());
  }
  const ::p4::v1::ForwardingPipelineConfig& config = status.ValueOrDie();

  switch (req->response_type()) {
    case ::p4::v1::GetForwardingPipelineConfigRequest::ALL: {
      *resp->mutable_config() = config;
      break;
    }
    case ::p4::v1::GetForwardingPipelineConfigRequest::COOKIE_ONLY: {
      *resp->mutable_config()->mutable_cookie() = config.cookie();
      break;
    }
    case ::p4::v1::GetForwardingPipelineConfigRequest::P4INFO_AND_COOKIE: {
      *resp->mutable_config()->mutable_p4info() = config.p4info();
      *resp->mutable_config()->mutable_cookie() = config.cookie();
      break;
    }
    case ::p4::v1::GetForwardingPipelineConfigRequest::
        DEVICE_CONFIG_AND_COOKIE: {
      *resp->mutable_config()->mutable_p4_device_config() =
          config.p4_device_config();
      *resp->mutable_config()->mutable_cookie() = config.cookie();
      break;
    }
    default:
      return ::grpc::Status(
          ::grpc::StatusCode::INVALID_ARGUMENT,
          absl::StrCat("Invalid action passed for node ", node_id, "."));
  }

  return ::grpc::Status::OK;
}

::grpc::Status P4Service::StreamChannel(
    ::grpc::ServerContext* context, ServerStreamChannelReaderWriter* stream) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, P4Service, StreamChannel,
                           context);

  // Here are the rules:
  // 1- When a client (aka controller) connects for the first time, we do not do
  //    anything until a MasterArbitrationUpdate proto is received.
  // 2- After MasterArbitrationUpdate is received at any time (we can receive
  //    this many time), the controller becomes/stays master or slave.
  // 3- At any point of time, only the master stream is capable of sending
  //    and receiving packets.

  // First thing to do is to ensure that we're not already handling too many
  // connections and increment the counter by one.
  auto ret = CheckAndIncrementConnectionCount();
  if (!ret.ok()) {
    return ::grpc::Status(ToGrpcCode(ret.CanonicalCode()), ret.error_message());
  }

  // We create a unique SDN connection object for every active connection.
  auto sdn_connection =
      absl::make_unique<p4runtime::SdnConnection>(context, stream);

  // The ID of the node this stream channel corresponds to. This is MUST NOT
  // change after it is set for the first time.
  uint64 node_id = 0;

  // The cleanup object. Will call RemoveController() upon exit.
  auto cleaner = absl::MakeCleanup([this, &node_id, &sdn_connection]() {
    this->RemoveController(node_id, sdn_connection.get());
  });

  ::p4::v1::StreamMessageRequest req;
  while (stream->Read(&req)) {
    switch (req.update_case()) {
      case ::p4::v1::StreamMessageRequest::kArbitration: {
        if (req.arbitration().device_id() == 0) {
          return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                                "Invalid node (aka device) ID.");
        } else if (node_id == 0) {
          node_id = req.arbitration().device_id();
        }
        absl::uint128 election_id =
            absl::MakeUint128(req.arbitration().election_id().high(),
                              req.arbitration().election_id().low());
        if (election_id == 0) {
          return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                                "Invalid election ID.");
        }
        // Try to add the controller to controllers_.
        auto status = AddOrModifyController(node_id, req.arbitration(),
                                            sdn_connection.get());
        if (!status.ok()) {
          return ::grpc::Status(ToGrpcCode(status.CanonicalCode()),
                                status.error_message());
        }
        LOG(INFO) << "Controller " << sdn_connection->GetName()
                  << " is connected as "
                  << (IsMasterController(node_id, sdn_connection->GetRoleName(),
                                         sdn_connection->GetElectionId())
                          ? "MASTER"
                          : "SLAVE")
                  << " for node (aka device) with ID " << node_id << ".";
        break;
      }
      case ::p4::v1::StreamMessageRequest::kPacket: {
        // If this stream is not the master stream generate a stream error.
        ::util::Status status;
        if (!IsMasterController(node_id, sdn_connection->GetRoleName(),
                                sdn_connection->GetElectionId())) {
          status = MAKE_ERROR(ERR_PERMISSION_DENIED).without_logging()
                   << "Controller " << sdn_connection->GetName()
                   << " is not a master";
        } else {
          // If master, try to transmit the packet.
          status = switch_interface_->HandleStreamMessageRequest(node_id, req);
        }
        if (!status.ok()) {
          LOG_EVERY_N(INFO, 500) << "Failed to transmit packet: " << status;
          auto resp = ToStreamMessageResponse(status);
          *resp.mutable_error()->mutable_packet_out()->mutable_packet_out() =
              req.packet();
          sdn_connection->SendStreamMessageResponse(resp);  // Best effort.
        }
        break;
      }
      case ::p4::v1::StreamMessageRequest::kDigestAck: {
        // If this stream is not the master stream generate a stream error.
        ::util::Status status;
        if (!IsMasterController(node_id, sdn_connection->GetRoleName(),
                                sdn_connection->GetElectionId())) {
          status = MAKE_ERROR(ERR_PERMISSION_DENIED).without_logging()
                   << "Controller " << sdn_connection->GetName()
                   << " is not a master";
        } else {
          // If master, try to ack the digest.
          status = switch_interface_->HandleStreamMessageRequest(node_id, req);
        }
        if (!status.ok()) {
          LOG(INFO) << "Failed to ack digest: " << status;
          // TODO(max): investigate if creating responses for every failure is
          // too resource intensive.
          auto resp = ToStreamMessageResponse(status);
          *resp.mutable_error()
               ->mutable_digest_list_ack()
               ->mutable_digest_list_ack() = req.digest_ack();
          sdn_connection->SendStreamMessageResponse(resp);  // Best effort.
        }
        break;
      }
      case ::p4::v1::StreamMessageRequest::UPDATE_NOT_SET:
      case ::p4::v1::StreamMessageRequest::kOther:
        return ::grpc::Status(
            ::grpc::StatusCode::INVALID_ARGUMENT,
            "Need to specify either arbitration, packet or digest ack.");
    }
  }

  return ::grpc::Status::OK;
}

::grpc::Status P4Service::Capabilities(::grpc::ServerContext* context,
                                       const ::p4::v1::CapabilitiesRequest* req,
                                       ::p4::v1::CapabilitiesResponse* resp) {
  resp->set_p4runtime_api_version(STRINGIFY(P4RUNTIME_VER));
  return ::grpc::Status::OK;
}

::util::Status P4Service::CheckAndIncrementConnectionCount() {
  absl::WriterMutexLock l(&controller_lock_);

  if (num_controller_connections_ >= FLAGS_max_num_controller_connections) {
    return MAKE_ERROR(ERR_NO_RESOURCE)
           << "Can have max " << FLAGS_max_num_controller_connections
           << " active/inactive streams for all the nodes.";
  }
  ++num_controller_connections_;

  return ::util::OkStatus();
}

::util::Status P4Service::AddOrModifyController(
    uint64 node_id, const ::p4::v1::MasterArbitrationUpdate& update,
    p4runtime::SdnConnection* controller) {
  // To be called by all the threads handling controller connections.
  absl::WriterMutexLock l(&controller_lock_);
  auto it = node_id_to_controller_manager_.find(node_id);
  if (it == node_id_to_controller_manager_.end()) {
    absl::WriterMutexLock lg(&stream_response_thread_lock_);
    // This is the first time we are hearing about this node. Lets try to add
    // an RX response writer for it. If the node_id is invalid, registration
    // will fail.
    std::shared_ptr<Channel<::p4::v1::StreamMessageResponse>> channel =
        Channel<::p4::v1::StreamMessageResponse>::Create(128);
    // Create the writer and register with the SwitchInterface.
    auto writer =
        std::make_shared<ChannelWriterWrapper<::p4::v1::StreamMessageResponse>>(
            ChannelWriter<::p4::v1::StreamMessageResponse>::Create(channel));
    RETURN_IF_ERROR(switch_interface_->RegisterStreamMessageResponseWriter(
        node_id, writer));
    // Create the reader and pass it to a new thread.
    auto reader =
        ChannelReader<::p4::v1::StreamMessageResponse>::Create(channel);
    pthread_t tid = 0;
    int ret = pthread_create(&tid, nullptr, StreamResponseReceiveThreadFunc,
                             new ReaderArgs<::p4::v1::StreamMessageResponse>{
                                 this, std::move(reader), node_id});
    if (ret) {
      // Clean up state and return error.
      RETURN_IF_ERROR(
          switch_interface_->UnregisterStreamMessageResponseWriter(node_id));
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to create packet-in receiver thread for node "
             << node_id << " with error " << ret << ".";
    }
    // Store Channel and tid for Teardown().
    stream_response_reader_tids_.push_back(tid);
    stream_response_channels_[node_id] = channel;
    // SdnControllerManager must be constructed in-place, as it's not moveable.
    node_id_to_controller_manager_.emplace(node_id, node_id);
    it = node_id_to_controller_manager_.find(node_id);
  }

  // Need to check we do not go beyond the max number of connections per node.
  if (it->second.ActiveConnections() >= FLAGS_max_num_controllers_per_node) {
    return MAKE_ERROR(ERR_NO_RESOURCE)
           << "Cannot have more than " << FLAGS_max_num_controllers_per_node
           << " controllers for node (aka device) with ID " << node_id << ".";
  }

  ::grpc::Status status =
      it->second.HandleArbitrationUpdate(update, controller);
  if (!status.ok()) {
    return ::util::Status(static_cast<::util::error::Code>(status.error_code()),
                          status.error_message());
  }

  return ::util::OkStatus();
}

void P4Service::RemoveController(uint64 node_id,
                                 p4runtime::SdnConnection* connection) {
  absl::WriterMutexLock l(&controller_lock_);
  --num_controller_connections_;
  auto it = node_id_to_controller_manager_.find(node_id);
  if (it == node_id_to_controller_manager_.end()) return;
  it->second.Disconnect(connection);
}

::grpc::Status P4Service::IsWritePermitted(
    uint64 node_id, const ::p4::v1::WriteRequest& req) const {
  absl::ReaderMutexLock l(&controller_lock_);
  auto it = node_id_to_controller_manager_.find(node_id);
  if (it == node_id_to_controller_manager_.end())
    return ::grpc::Status(
        grpc::StatusCode::PERMISSION_DENIED,
        absl::StrCat("Write from non-master is not permitted for node ",
                     node_id, "."));
  return it->second.AllowRequest(req);
}

::grpc::Status P4Service::IsWritePermitted(
    uint64 node_id,
    const ::p4::v1::SetForwardingPipelineConfigRequest& req) const {
  absl::ReaderMutexLock l(&controller_lock_);
  auto it = node_id_to_controller_manager_.find(node_id);
  if (it == node_id_to_controller_manager_.end())
    return ::grpc::Status(
        grpc::StatusCode::PERMISSION_DENIED,
        absl::StrCat("Write from non-master is not permitted for node ",
                     node_id, "."));
  return it->second.AllowRequest(req);
}

::grpc::Status P4Service::IsReadPermitted(
    uint64 node_id, const p4::v1::ReadRequest& req) const {
  absl::ReaderMutexLock l(&controller_lock_);
  auto it = node_id_to_controller_manager_.find(node_id);
  if (it == node_id_to_controller_manager_.end()) return ::grpc::Status::OK;
  return it->second.AllowRequest(req);
}

bool P4Service::IsMasterController(
    uint64 node_id, const absl::optional<std::string>& role_name,
    const absl::optional<absl::uint128>& election_id) const {
  absl::ReaderMutexLock l(&controller_lock_);
  auto it = node_id_to_controller_manager_.find(node_id);
  if (it == node_id_to_controller_manager_.end()) return false;
  return it->second.AllowRequest(role_name, election_id).ok();
}

::util::StatusOr<::p4::v1::ForwardingPipelineConfig>
P4Service::DoGetForwardingPipelineConfig(uint64 node_id) const {
  absl::ReaderMutexLock l(&config_lock_);
  if (forwarding_pipeline_configs_ == nullptr ||
      forwarding_pipeline_configs_->node_id_to_config_size() == 0) {
    return MAKE_ERROR(ERR_FAILED_PRECONDITION)
           << "No valid forwarding pipeline config has been pushed for any "
           << "node so far.";
  }
  auto it = forwarding_pipeline_configs_->node_id_to_config().find(node_id);
  if (it == forwarding_pipeline_configs_->node_id_to_config().end()) {
    return MAKE_ERROR(ERR_FAILED_PRECONDITION)
           << "Invalid node id or no valid forwarding pipeline config has been "
           << "pushed for node " << node_id << " yet.";
  }

  return it->second;
}

p4::v1::ReadRequest P4Service::ExpandWildcardsInReadRequest(
    const p4::v1::ReadRequest& req,
    const p4::config::v1::P4Info& p4info) const {
  absl::ReaderMutexLock l(&controller_lock_);

  auto it = node_id_to_controller_manager_.find(req.device_id());
  if (it == node_id_to_controller_manager_.end()) return req;
  return it->second.ExpandWildcardsInReadRequest(req, p4info);
}

void* P4Service::StreamResponseReceiveThreadFunc(void* arg) {
  auto* args =
      reinterpret_cast<ReaderArgs<::p4::v1::StreamMessageResponse>*>(arg);
  auto* p4_service = args->p4_service;
  auto node_id = args->node_id;
  auto reader = std::move(args->reader);
  delete args;
  return p4_service->ReceiveStreamRespones(node_id, std::move(reader));
}

void* P4Service::ReceiveStreamRespones(
    uint64 node_id,
    std::unique_ptr<ChannelReader<::p4::v1::StreamMessageResponse>> reader) {
  do {
    ::p4::v1::StreamMessageResponse resp;
    // Block on next stream response RX from Channel.
    int code = reader->Read(&resp, absl::InfiniteDuration()).error_code();
    // Exit if the Channel is closed.
    if (code == ERR_CANCELLED) break;
    // Read should never timeout.
    if (code == ERR_ENTRY_NOT_FOUND) {
      LOG(ERROR) << "Read with infinite timeout failed with ENTRY_NOT_FOUND.";
      continue;
    }
    // Handle StreamMessageResponse.
    StreamResponseReceiveHandler(node_id, resp);
  } while (true);
  return nullptr;
}

void P4Service::StreamResponseReceiveHandler(
    uint64 node_id, const ::p4::v1::StreamMessageResponse& resp) {
  // We don't expect arbitration updates from the switch.
  if (resp.has_arbitration()) {
    LOG(FATAL) << "Received MasterArbitrationUpdate from switch. This should "
                  "never happen!";
  }
  // We send the responses only to the master controller stream for this node.
  absl::ReaderMutexLock l(&controller_lock_);
  auto it = node_id_to_controller_manager_.find(node_id);
  if (it == node_id_to_controller_manager_.end()) return;
  absl::Status status = it->second.SendStreamMessageToPrimary(resp);
  LOG_IF_EVERY_N(ERROR, !status.ok(), 500)
      << "Can't send StreamMessageResponse " << resp.ShortDebugString()
      << " to primary controller: " << status.ToString();
}

}  // namespace hal
}  // namespace stratum
