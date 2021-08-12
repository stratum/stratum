// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/dummy/dummy_box.h"

#include <pthread.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/public/proto/error.pb.h"

constexpr char kDefaultDummyBoxUrl[] = "localhost:28010";
const ::absl::Duration kDefaultEventWriteTimeout = absl::Seconds(10);

DEFINE_string(
    dummy_box_url, kDefaultDummyBoxUrl,
    "External URL for dummmy box server to listen to external calls.");
DEFINE_int32(dummy_test_grpc_keepalive_time_ms, 600000, "grpc keep alive time");
DEFINE_int32(dummy_test_grpc_keepalive_timeout_ms, 20000,
             "grpc keep alive timeout period");
DEFINE_int32(dummy_test_grpc_keepalive_min_ping_interval, 10000,
             "grpc keep alive minimum ping interval");
DEFINE_int32(dummy_test_grpc_keepalive_permit, 1, "grpc keep alive permit");

namespace stratum {
namespace hal {
namespace dummy_switch {

std::unique_ptr<::grpc::Server> external_server_;

void* ExternalServerWaitingFunc(void* arg) {
  if (external_server_ == nullptr) {
    LOG(ERROR) << "gRPC server does not initialized";
    return nullptr;
  }
  LOG(INFO) << "Listen test service on " << FLAGS_dummy_box_url << ".";
  external_server_->Wait();  // block
  return nullptr;
}

::grpc::Status DummyBox::DeviceStatusUpdate(
    ::grpc::ServerContext* context, const DeviceStatusUpdateRequest* request,
    DeviceStatusUpdateResponse* response) {
  switch (request->source().source_case()) {
    case DeviceStatusUpdateRequest::Source::kPort:
      return HandlePortStatusUpdate(request->source().port().node_id(),
                                    request->source().port().port_id(),
                                    request->state_update());
    case DeviceStatusUpdateRequest::Source::kNode:
    case DeviceStatusUpdateRequest::Source::kPortQueue:
    case DeviceStatusUpdateRequest::Source::kChassis:
    default:
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED,
                            "Not implement yet!");
  }
}

::grpc::Status DummyBox::TransceiverEventUpdate(
    ::grpc::ServerContext* context, const TransceiverEventRequest* request,
    TransceiverEventResponse* response) {
  absl::ReaderMutexLock l(&sdk_lock_);
  for (auto& writer_elem : xcvr_event_writers_) {
    PhalInterface::TransceiverEvent event;
    event.slot = request->slot();
    event.port = request->port();
    event.state = request->state();
    writer_elem.writer->Write(event, kDefaultEventWriteTimeout);
  }
  return ::grpc::Status();
}

::grpc::Status DummyBox::HandlePortStatusUpdate(
    uint64 node_id, uint64 port_id, ::stratum::hal::DataResponse state_update) {
  absl::ReaderMutexLock l(&sdk_lock_);
  auto event_writer_elem = node_event_notify_writers_.find(node_id);
  if (event_writer_elem == node_event_notify_writers_.end()) {
    // No event writer for this device can handle the event.
    LOG(WARNING) << "Receives device status update event, however"
                 << " there is no event writer for device id " << node_id
                 << " found, drop event.";
    return ::grpc::Status(::grpc::StatusCode::NOT_FOUND,
                          "Event writer not found");
  }
  auto node_event_notify_writer_ = event_writer_elem->second;

  DummyNodeEvent* event = new DummyNodeEvent();
  event->node_id = node_id;
  event->port_id = port_id;
  event->state_update = state_update;
  node_event_notify_writer_->Write(DummyNodeEventPtr(event));
  return ::grpc::Status();
}

::util::StatusOr<int> DummyBox::RegisterTransceiverEventWriter(
    std::unique_ptr<ChannelWriter<PhalInterface::TransceiverEvent>> writer,
    int priority) {
  absl::WriterMutexLock l(&sdk_lock_);
  // Generate new transceiver writer ID
  ++xcvr_writer_id_;
  PhalInterface::TransceiverEventWriter xcvr_event_writer;
  xcvr_event_writer.writer = std::move(writer);
  xcvr_event_writer.priority = priority;
  xcvr_event_writer.id = xcvr_writer_id_;
  xcvr_event_writers_.push_back(std::move(xcvr_event_writer));

  std::sort(xcvr_event_writers_.begin(), xcvr_event_writers_.end(),
            TransceiverEventWriterComp());
  return ::util::StatusOr<int>(xcvr_writer_id_);
}

::util::Status DummyBox::UnregisterTransceiverEventWriter(int id) {
  absl::WriterMutexLock l(&sdk_lock_);
  std::remove_if(xcvr_event_writers_.begin(), xcvr_event_writers_.end(),
                 FindXcvrById(id));
  return ::util::OkStatus();
}

::util::Status DummyBox::RegisterNodeEventNotifyWriter(
    uint64 node_id,
    std::shared_ptr<WriterInterface<DummyNodeEventPtr>> writer) {
  absl::WriterMutexLock l(&sdk_lock_);
  if (node_event_notify_writers_.find(node_id) !=
      node_event_notify_writers_.end()) {
    return ::util::Status(::util::error::ALREADY_EXISTS,
                          "Writer already exists");
  }

  node_event_notify_writers_.emplace(node_id, writer);
  return ::util::OkStatus();
}

::util::Status DummyBox::UnregisterNodeEventNotifyWriter(uint64 node_id) {
  absl::WriterMutexLock l(&sdk_lock_);
  if (node_event_notify_writers_.find(node_id) ==
      node_event_notify_writers_.end()) {
    return ::util::Status(::util::error::NOT_FOUND, "Writer not found");
  }
  node_event_notify_writers_.erase(node_id);
  return ::util::OkStatus();
}

::util::Status DummyBox::RegisterChassisEventNotifyWriter(
    std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  absl::WriterMutexLock l(&sdk_lock_);
  if (chassis_event_notify_writer_) {
    return MAKE_ERROR(ERR_INTERNAL) << "Chassis event writer already exists";
  }
  chassis_event_notify_writer_ = writer;
  return ::util::OkStatus();
}

::util::Status DummyBox::UnregisterChassisEventNotifyWriter() {
  absl::WriterMutexLock l(&sdk_lock_);
  chassis_event_notify_writer_.reset();
  return ::util::OkStatus();
}

DummyBox* DummyBox::GetSingleton() {
  static DummyBox* dummy_box_singleton_ = new DummyBox();
  return dummy_box_singleton_;
}

::util::Status DummyBox::Start() {
  absl::WriterMutexLock l(&sdk_lock_);
  if (initialized_) {
    return MAKE_ERROR(ERR_ABORTED) << "SDK already initialized";
  }

  // Initialize the gRPC server with test service.
  ::grpc::ServerBuilder builder;
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS,
                             FLAGS_dummy_test_grpc_keepalive_time_ms);
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
                             FLAGS_dummy_test_grpc_keepalive_timeout_ms);
  builder.AddChannelArgument(
      GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS,
      FLAGS_dummy_test_grpc_keepalive_min_ping_interval);
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS,
                             FLAGS_dummy_test_grpc_keepalive_permit);
  builder.AddListeningPort(FLAGS_dummy_box_url,
                           ::grpc::InsecureServerCredentials());
  builder.RegisterService(this);

  external_server_ = builder.BuildAndStart();
  if (external_server_ == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Failed to start DummyBox test service to listen "
           << "to " << FLAGS_dummy_box_url << ".";
  }

  // Create another thread to run "external_server_->Wait()" since we can not
  // block the main thread here
  int ret = pthread_create(&external_server_tid_, nullptr,
                           ExternalServerWaitingFunc, nullptr);

  if (ret != 0) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Failed to create server listen thread. Err: " << ret << ".";
  }

  initialized_ = true;
  return ::util::OkStatus();
}

::util::Status DummyBox::Shutdown() {
  absl::WriterMutexLock l(&sdk_lock_);
  LOG(INFO) << "Shutting down the DummyBox.";
  external_server_->Shutdown(std::chrono::system_clock::now());
  if (external_server_tid_) pthread_join(external_server_tid_, nullptr);
  initialized_ = false;
  return ::util::OkStatus();
}

DummyBox::~DummyBox() {}
DummyBox::DummyBox()
    : initialized_(false), xcvr_writer_id_(0), external_server_tid_(0) {}

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum
