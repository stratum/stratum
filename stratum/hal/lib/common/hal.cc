// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/common/hal.h"

#include <limits.h>

#include <utility>

#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/synchronization/mutex.h"
#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

// TODO(unknown): Use FLAG_DEFINE for all flags.
DEFINE_string(external_stratum_urls, stratum::kExternalStratumUrls,
              "Comma-separated list of URLs for server to listen to for "
              "external calls from SDN controller, etc.");
DEFINE_string(local_stratum_url, stratum::kLocalStratumUrl,
              "URL for listening to local calls from stratum stub.");
DEFINE_bool(warmboot, false, "Determines whether HAL is in warmboot stage.");
DEFINE_string(persistent_config_dir, "/etc/stratum/",
              "The persistent dir where all the config files will be stored.");
DEFINE_int32(grpc_keepalive_time_ms, 600000, "grpc keep alive time");
DEFINE_int32(grpc_keepalive_timeout_ms, 20000,
             "grpc keep alive timeout period");
DEFINE_int32(grpc_keepalive_min_ping_interval, 10000,
             "grpc keep alive minimum ping interval");
DEFINE_int32(grpc_keepalive_permit, 1, "grpc keep alive permit");
DEFINE_uint32(grpc_max_recv_msg_size, 256 * 1024 * 1024,
              "grpc server max receive message size (0 = gRPC default).");
DEFINE_uint32(grpc_max_send_msg_size, 0,
              "grpc server max send message size (0 = gRPC default).");

namespace stratum {
namespace hal {

namespace {

// Signal received callback which is registered as the handler for SIGINT and
// SIGTERM signals using signal() system call.
void SignalRcvCallback(int value) {
  static_assert(sizeof(value) <= PIPE_BUF,
                "PIPE_BUF is smaller than the number of bytes that can be "
                "written atomically to a pipe.");
  // We must restore any changes made to errno at the end of the handler:
  // https://www.gnu.org/software/libc/manual/html_node/POSIX-Safety-Concepts.html
  int saved_errno = errno;
  // No reasonable error handling possible.
  write(Hal::pipe_write_fd_, &value, sizeof(value));
  errno = saved_errno;
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
int Hal::pipe_read_fd_ = -1;
int Hal::pipe_write_fd_ = -1;

Hal::Hal(OperationMode mode, SwitchInterface* switch_interface,
         AuthPolicyChecker* auth_policy_checker,
         CredentialsManager* credentials_manager)
    : mode_(mode),
      switch_interface_(ABSL_DIE_IF_NULL(switch_interface)),
      auth_policy_checker_(ABSL_DIE_IF_NULL(auth_policy_checker)),
      credentials_manager_(ABSL_DIE_IF_NULL(credentials_manager)),
      error_buffer_(ABSL_DIE_IF_NULL(new ErrorBuffer())),
      config_monitoring_service_(),
      p4_service_(nullptr),
      admin_service_(nullptr),
      certificate_management_service_(nullptr),
      diag_service_(nullptr),
      file_service_(nullptr),
      external_server_(nullptr),
      old_signal_handlers_(),
      signal_waiter_tid_() {}

Hal::~Hal() {
  // TODO(unknown): Handle this error?
  UnregisterSignalHandlers().IgnoreError();
}

::util::Status Hal::SanityCheck() {
  const std::vector<std::string> external_stratum_urls =
      absl::StrSplit(FLAGS_external_stratum_urls, ',', absl::SkipEmpty());
  CHECK_RETURN_IF_FALSE(!external_stratum_urls.empty())
      << "No external URL was given. This is invalid.";

  auto it =
      std::find_if(external_stratum_urls.begin(), external_stratum_urls.end(),
                   [](const std::string& url) {
                     return (url == FLAGS_local_stratum_url
                             // FIXME(boc) google only
                             /* || url == FLAGS_cmal_service_url*/);
                   });
  CHECK_RETURN_IF_FALSE(it == external_stratum_urls.end())
      << "You used one of these reserved local URLs as your external URLs: "
      << FLAGS_local_stratum_url
      << ", "
      /*FIXME(boc) google only << FLAGS_cmal_service_url */
      << ".";

  CHECK_RETURN_IF_FALSE(!FLAGS_persistent_config_dir.empty())
      << "persistent_config_dir flag needs to be explicitly given.";

  LOG(INFO) << "HAL sanity checks all passed.";

  return ::util::OkStatus();
}

::util::Status Hal::Setup() {
  LOG(INFO) << "Setting up HAL in "
            << (FLAGS_warmboot ? "WARMBOOT" : "COLDBOOT") << " mode...";

  RETURN_IF_ERROR(RecursivelyCreateDir(FLAGS_persistent_config_dir));

  // Setup all the services. In case of coldboot setup, we push the saved
  // configs to the switch as part of setup. In case of warmboot, we only
  // recover the internal state of the class.
  RETURN_IF_ERROR(config_monitoring_service_->Setup(FLAGS_warmboot));
  RETURN_IF_ERROR(p4_service_->Setup(FLAGS_warmboot));
  RETURN_IF_ERROR(admin_service_->Setup(FLAGS_warmboot));
  RETURN_IF_ERROR(certificate_management_service_->Setup(FLAGS_warmboot));
  RETURN_IF_ERROR(diag_service_->Setup(FLAGS_warmboot));
  RETURN_IF_ERROR(file_service_->Setup(FLAGS_warmboot));
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
  // AdminService, which itself calls Freeze() method in SwitchInterface class.
  LOG(INFO) << "Shutting down HAL...";
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, config_monitoring_service_->Teardown());
  APPEND_STATUS_IF_ERROR(status, p4_service_->Teardown());
  APPEND_STATUS_IF_ERROR(status, certificate_management_service_->Teardown());
  APPEND_STATUS_IF_ERROR(status, diag_service_->Teardown());
  APPEND_STATUS_IF_ERROR(status, file_service_->Teardown());
  APPEND_STATUS_IF_ERROR(status, switch_interface_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, auth_policy_checker_->Shutdown());
  APPEND_STATUS_IF_ERROR(status, admin_service_->Teardown());
  if (!status.ok()) {
    error_buffer_->AddError(status, "Failed to shutdown HAL: ", GTL_LOC);
    return status;
  }

  return ::util::OkStatus();
}

::util::Status Hal::Run() {
  // All HAL external facing services listen to a list of secure external URLs
  // given by external_stratum_urls flag, as well as a local insecure URLs for
  // given by local_stratum_url flag. The insecure URLs is used by any local
  // stratum_stub binary running on the switch, since local connections cannot
  // support auth.
  const std::vector<std::string> external_stratum_urls =
      absl::StrSplit(FLAGS_external_stratum_urls, ',');
  {
    std::shared_ptr<::grpc::ServerCredentials> server_credentials =
        credentials_manager_->GenerateExternalFacingServerCredentials();
    ::grpc::ServerBuilder builder;
    SetGrpcServerKeepAliveArgs(&builder);
    builder.AddListeningPort(FLAGS_local_stratum_url,
                             ::grpc::InsecureServerCredentials());
    for (const auto& url : external_stratum_urls) {
      builder.AddListeningPort(url, server_credentials);
    }
    if (FLAGS_grpc_max_recv_msg_size > 0) {
      builder.SetMaxReceiveMessageSize(FLAGS_grpc_max_recv_msg_size);
      builder.AddChannelArgument<int>(GRPC_ARG_MAX_METADATA_SIZE,
                                      FLAGS_grpc_max_recv_msg_size);
    }
    if (FLAGS_grpc_max_send_msg_size > 0) {
      builder.SetMaxSendMessageSize(FLAGS_grpc_max_send_msg_size);
    }
    builder.RegisterService(config_monitoring_service_.get());
    builder.RegisterService(p4_service_.get());
    builder.RegisterService(admin_service_.get());
    builder.RegisterService(certificate_management_service_.get());
    builder.RegisterService(diag_service_.get());
    builder.RegisterService(file_service_.get());
    external_server_ = builder.BuildAndStart();
    if (external_server_ == nullptr) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to start Stratum external facing services. This is an "
             << "internal error.";
    }
    LOG(ERROR) << "Stratum external facing services are listening to "
               << absl::StrJoin(external_stratum_urls, ", ") << ", "
               << FLAGS_local_stratum_url << "...";
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
  external_server_->Shutdown(absl::ToChronoTime(absl::Now()));
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

#define CHECK_IS_NULL(ptr)                                                    \
  if (ptr != nullptr) {                                                       \
    return MAKE_ERROR(ERR_INTERNAL)                                           \
           << #ptr << " is not nullptr. InitializeServer() cannot be called " \
           << "multiple times.";                                              \
  }

::util::Status Hal::InitializeServer() {
  CHECK_IS_NULL(config_monitoring_service_);
  CHECK_IS_NULL(p4_service_);
  CHECK_IS_NULL(admin_service_);
  CHECK_IS_NULL(certificate_management_service_);
  CHECK_IS_NULL(diag_service_);
  CHECK_IS_NULL(file_service_);
  CHECK_IS_NULL(external_server_);
  // FIXME(boc) google only
  // CHECK_IS_NULL(internal_server_);

  // Reset error_buffer_.
  error_buffer_->ClearErrors();

  // Build the HAL services.
  config_monitoring_service_ = absl::make_unique<ConfigMonitoringService>(
      mode_, switch_interface_, auth_policy_checker_, error_buffer_.get());
  p4_service_ = absl::make_unique<P4Service>(
      mode_, switch_interface_, auth_policy_checker_, error_buffer_.get());
  admin_service_ = absl::make_unique<AdminService>(
      mode_, switch_interface_, auth_policy_checker_, error_buffer_.get(),
      SignalRcvCallback);
  certificate_management_service_ =
      absl::make_unique<CertificateManagementService>(
          mode_, switch_interface_, auth_policy_checker_, error_buffer_.get());
  diag_service_ = absl::make_unique<DiagService>(
      mode_, switch_interface_, auth_policy_checker_, error_buffer_.get());
  file_service_ = absl::make_unique<FileService>(
      mode_, switch_interface_, auth_policy_checker_, error_buffer_.get());

  return ::util::OkStatus();
}

#undef CHECK_IS_NULL  // should not be used in any other method.

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
  // Create the pipe to transfer signals.
  RETURN_IF_ERROR(CreatePipeForSignalHandling(&pipe_read_fd_, &pipe_write_fd_));
  // Start the signal waiter thread that initiates shutdown.
  CHECK_RETURN_IF_FALSE(pthread_create(&signal_waiter_tid_, nullptr,
                                       SignalWaiterThreadFunc, nullptr) == 0)
      << "Could not start the signal waiter thread.";

  return ::util::OkStatus();
}

::util::Status Hal::UnregisterSignalHandlers() {
  // Register the old handlers for all the signals.
  for (const auto& e : old_signal_handlers_) {
    signal(e.first, e.second);
  }
  old_signal_handlers_.clear();
  // Close pipe to unblock the waiter thread.
  if (pipe_write_fd_ != -1) close(pipe_write_fd_);
  if (pipe_read_fd_ != -1) close(pipe_read_fd_);
  // Join thread.
  if (signal_waiter_tid_ && pthread_join(signal_waiter_tid_, nullptr) != 0) {
    LOG(ERROR) << "Failed to join signal waiter thread.";
  }

  return ::util::OkStatus();
}

void* Hal::SignalWaiterThreadFunc(void*) {
  int signal_value;
  int ret = read(Hal::pipe_read_fd_, &signal_value, sizeof(signal_value));
  if (ret == 0) {  // Pipe has been closed.
    return nullptr;
  } else if (ret != sizeof(signal_value)) {
    LOG(ERROR) << "Error reading complete signal from pipe: " << ret << ": "
               << strerror(errno);
    return nullptr;
  }
  Hal* hal = Hal::GetSingleton();
  if (hal == nullptr) return nullptr;
  hal->HandleSignal(signal_value);

  return nullptr;
}

}  // namespace hal
}  // namespace stratum
