// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "stratum/hal/lib/common/hal.h"

#include <chrono>  // NOLINT
#include <utility>

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/sandcastle/procmon_service.grpc.pb.h"
#include "stratum/lib/utils.h"
#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

DEFINE_string(url, google::hercules::kLocalHerculesUrl,
              "External URL for server to listen to external calls.");
DEFINE_string(local_hercules_url, google::hercules::kLocalHerculesUrl,
              "URL for listening to local calls from hercules stub.");
DEFINE_bool(warmboot, false, "Determines whether HAL is in warmboot stage.");
DEFINE_string(procmon_service_addr, ::google::hercules::kProcmonServiceUrl,
              "URL of the procmon service to connect to.");
DEFINE_string(persistent_config_dir, "",
              "The persistent dir where all the config files will be stored.");
DEFINE_int32(grpc_keepalive_time_ms, 600000, "grpc keep alive time");
DEFINE_int32(grpc_keepalive_timeout_ms, 20000,
             "grpc keep alive timeout period");
DEFINE_int32(grpc_keepalive_min_ping_interval, 10000,
             "grpc keep alive minimum ping interval");
DEFINE_int32(grpc_keepalive_permit, 1, "grpc keep alive permit");

namespace stratum {
namespace hal {

namespace {

// Signal received callback which is registered as the handler for SIGINT and
// SIGTERM signals using signal() system call.
void SignalRcvCallback(int value) {
  Hal* hal = Hal::GetSingleton();
  if (hal == nullptr) return;
  hal->HandleSignal(value);
}

// Set the channel arguments to match the defualt keep-alive parameters set by
// the google3 side net/grpc clients.
void SetGrpcServerKeepAliveArgs(::grpc::ServerBuilder* builder) {
  builder->AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS,
                              FLAGS_grpc_keepalive_time_ms);
  builder->AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
                              FLAGS_grpc_keepalive_timeout_ms);
  builder->AddChannelArgument(
      GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS,
      FLAGS_grpc_keepalive_min_ping_interval);
  builder->AddChannelArgument(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS,
                              FLAGS_grpc_keepalive_permit);
}

}  // namespace

Hal* Hal::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex Hal::init_lock_(absl::kConstInit);

Hal::Hal(OperationMode mode, SwitchInterface* switch_interface,
         AuthPolicyChecker* auth_policy_checker,
         CredentialsManager* credentials_manager)
    : mode_(mode),
      switch_interface_(CHECK_NOTNULL(switch_interface)),
      auth_policy_checker_(CHECK_NOTNULL(auth_policy_checker)),
      credentials_manager_(CHECK_NOTNULL(credentials_manager)),
      error_buffer_(CHECK_NOTNULL(new ErrorBuffer())),
      config_monitoring_service_(),
      p4_service_(nullptr),
      external_server_(nullptr),
      old_signal_handlers_() {}

Hal::~Hal() {
  // TODO: Handle this error?
  UnregisterSignalHandlers().IgnoreError();
}

::util::Status Hal::Setup() {
  LOG(INFO) << "Setting up HAL in "
            << (FLAGS_warmboot ? "WARMBOOT" : "COLDBOOT") << " mode...";

  CHECK_RETURN_IF_FALSE(!FLAGS_persistent_config_dir.empty())
      << "persistent_config_dir flag needs to be explicitly given.";
  RETURN_IF_ERROR(RecursivelyCreateDir(FLAGS_persistent_config_dir));

  // Setup all the services. In case of coldboot setup, we push the saved
  // configs to the switch as part of setup. In case of warmboot, we only
  // recover the internal state of the class.
  RETURN_IF_ERROR(config_monitoring_service_->Setup(FLAGS_warmboot));
  RETURN_IF_ERROR(p4_service_->Setup(FLAGS_warmboot));
  if (FLAGS_warmboot) {
    // In case of warmboot, we also call unfreeze the switch interface after
    // services are setup. Note that finding the saved configs in case of
    // warmboot is critical. We will not perform unfreeze if we dont find those
    // files.
    LOG(INFO) << "Unfreezing HAL...";
    ::util::Status status = switch_interface_->Unfreeze();
    if (!status.ok()) {
      error_buffer_->AddError(status, "Failed to unfreeze HAL: ", GTL_LOC);
      return status;
    }
  }

  // Successful warmboot or coldboot will clear out the blocking errors.
  error_buffer_->ClearErrors();

  return ::util::OkStatus();
}

::util::Status Hal::Teardown() {
  // Teardown is called as part of both warmboot and coldboot shutdown. In case
  // of warmboot shutdown, the stack is first freezed by calling an RPC in
  // AdminService, which itself calls Freeze() methid in SwitchInterface class.
  LOG(INFO) << "Shutting down HAL...";
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, config_monitoring_service_->Teardown());
  APPEND_STATUS_IF_ERROR(status, p4_service_->Teardown());
  APPEND_STATUS_IF_ERROR(status, switch_interface_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, auth_policy_checker_->Shutdown());
  if (!status.ok()) {
    error_buffer_->AddError(status, "Failed to shutdown HAL: ", GTL_LOC);
    return status;
  }

  return ::util::OkStatus();
}

::util::Status Hal::Run() {
  // ConfigMonitoringService and P4Service start listening to FLAGS_url for
  // external connections. We also listen to a local insecure port locally for
  // hercules_stub connection on the switch.
  {
    std::shared_ptr<::grpc::ServerCredentials> server_credentials =
        credentials_manager_->GenerateExternalFacingServerCredentials();
    ::grpc::ServerBuilder builder;
    SetGrpcServerKeepAliveArgs(&builder);
    builder.AddListeningPort(FLAGS_local_hercules_url,
                             ::grpc::InsecureServerCredentials());
    if (FLAGS_url == FLAGS_local_hercules_url) {
      LOG(WARNING) << "FLAGS_url is the same as FLAGS_local_hercules_url: "
                   << FLAGS_local_hercules_url;
    } else {
      builder.AddListeningPort(FLAGS_url, server_credentials);
    }
    builder.RegisterService(config_monitoring_service_.get());
    builder.RegisterService(p4_service_.get());
    external_server_ = builder.BuildAndStart();
    if (external_server_ == nullptr) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to start ConfigMonitoringService & P4Service to listen "
             << "to " << FLAGS_url << ".";
    }
    LOG(INFO) << "ConfigMonitoringService & P4Service listening to "
              << FLAGS_url << "...";
  }

  // Try checking in with Procmon. Continue if checkin fails.
  ::util::Status status = ProcmonCheckin();
  if (!status.ok()) {
    LOG(ERROR) << "Error when checking in with procmon: "
               << status.error_message() << ".";
  }

  external_server_->Wait();  // blocking until external_server_->Shutdown()
                             // is called. We dont wait on internal_service.
  return Teardown();
}

void Hal::HandleSignal(int value) {
  LOG(INFO) << "Received signal: " << strsignal(value);
  // Calling Shutdown() so the blocking call to Wait() returns.
  // NOTE: Seems like if there is an active stream Read(), calling Shutdown()
  // with no deadline will block forever, as it waits for all the active RPCs
  // to finish. To fix this, we give a deadline set to "now" so the call returns
  // immediately.
  external_server_->Shutdown(std::chrono::system_clock::now());
}

Hal* Hal::CreateSingleton(OperationMode mode, SwitchInterface* switch_interface,
                          AuthPolicyChecker* auth_policy_checker,
                          CredentialsManager* credentials_manager) {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new Hal(mode, switch_interface, auth_policy_checker,
                         credentials_manager);
    ::util::Status status = singleton_->RegisterSignalHandlers();
    if (!status.ok()) {
      LOG(ERROR) << "RegisterSignalHandlers() failed: " << status;
      delete singleton_;
      singleton_ = nullptr;
    }
    status = singleton_->InitializeServer();
    if (!status.ok()) {
      LOG(ERROR) << "InitializeServer() failed: " << status;
      delete singleton_;
      singleton_ = nullptr;
    }
  }

  return singleton_;
}

Hal* Hal::GetSingleton() {
  absl::ReaderMutexLock l(&init_lock_);
  return singleton_;
}

::util::Status Hal::InitializeServer() {
  if (config_monitoring_service_ != nullptr ||
      p4_service_ != nullptr || external_server_ != nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Server already started? Function "
                                    << "cannot be called multiple times.";
  }

  // Reset error_buffer_.
  error_buffer_->ClearErrors();

  // Build the HAL services.
  config_monitoring_service_ = absl::make_unique<ConfigMonitoringService>(
      mode_, switch_interface_, auth_policy_checker_, error_buffer_.get());
  p4_service_ = absl::make_unique<P4Service>(
      mode_, switch_interface_, auth_policy_checker_, error_buffer_.get());

  return ::util::OkStatus();
}

::util::Status Hal::RegisterSignalHandlers() {
  // Register the signal handlers and save the old handlers as well.
  std::vector<int> sig = {SIGINT, SIGTERM, SIGUSR2};
  for (const int s : sig) {
    sighandler_t h = signal(s, SignalRcvCallback);
    if (h == SIG_ERR) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to register signal " << strsignal(s);
    }
    old_signal_handlers_[s] = h;
  }

  return ::util::OkStatus();
}

::util::Status Hal::UnregisterSignalHandlers() {
  // Register the old handlers for all the signals.
  for (const auto& e : old_signal_handlers_) {
    signal(e.first, e.second);
  }
  old_signal_handlers_.clear();

  return ::util::OkStatus();
}

::util::Status Hal::ProcmonCheckin() {
  std::unique_ptr<procmon::ProcmonService::Stub> stub =
      procmon::ProcmonService::NewStub(::grpc::CreateChannel(
          FLAGS_procmon_service_addr, ::grpc::InsecureChannelCredentials()));
  if (stub == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Could not create stub for procmon gRPC service.";
  }

  procmon::CheckinRequest req;
  procmon::CheckinResponse resp;
  ::grpc::ClientContext context;
  req.set_checkin_key(getpid());
  ::grpc::Status status = stub->Checkin(&context, req, &resp);
  if (!status.ok()) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Failed to check in with procmon: " << status.error_message();
  }

  return ::util::OkStatus();
}

}  // namespace hal
}  // namespace stratum
