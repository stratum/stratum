// Copyright 2018-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/pi/pi_node.h"

#include "PI/frontends/proto/device_mgr.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "google/rpc/code.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace pi {

using ::pi::fe::proto::DeviceMgr;
using Code = ::google::rpc::Code;

namespace {

// Utility functions to convert between grpc::Status and Stratum
// util::Status. This is a bit silly because util::Status will be converted back
// to grpc::Status in the P4Service.

::util::Status toUtilStatus(const DeviceMgr::Status& from,
                            std::vector<::util::Status>* results,
                            int updates_size = 0) {
  if (from.code() == Code::OK) {
    if (results->size() != 0)
      return MAKE_ERROR(ERR_INTERNAL) << "Expected empty results vector.";
    results->resize(updates_size);
    return ::util::OkStatus();
  }
  ::util::Status status(::util::Status::canonical_space(), from.code(),
                        from.message());
  for (const auto& detail : from.details()) {
    ::p4::v1::Error error;
    detail.UnpackTo(&error);
    results->emplace_back(::util::Status::canonical_space(),
                          error.canonical_code(), error.message());
  }
  return status;
}

::util::Status toUtilStatus(const DeviceMgr::Status& from) {
  if (from.code() == Code::OK) {
    return ::util::OkStatus();
  } else {
    return ::util::Status(::util::Status::canonical_space(), from.code(),
                          from.message());
  }
}

}  // namespace

void StreamMessageCb(uint64_t node_id, ::p4::v1::StreamMessageResponse* msg,
                     void* cookie) {
  auto* pi_node = static_cast<PINode*>(cookie);
  pi_node->SendStreamMessageResponse(*msg);
}

PINode::PINode(::pi::fe::proto::DeviceMgr* device_mgr, int unit)
    : device_mgr_(device_mgr),
      unit_(unit),
      pipeline_initialized_(false),
      node_id_(0) {}

PINode::~PINode() = default;

std::unique_ptr<PINode> PINode::CreateInstance(
    ::pi::fe::proto::DeviceMgr* device_mgr, int unit) {
  return absl::WrapUnique(new PINode(device_mgr, unit));
}

::util::Status PINode::PushChassisConfig(const ChassisConfig& config,
                                         uint64 node_id) {
  (void)config;
  absl::WriterMutexLock l(&lock_);
  node_id_ = node_id;
  return ::util::OkStatus();
}

::util::Status PINode::VerifyChassisConfig(const ChassisConfig& config,
                                           uint64 node_id) {
  (void)config;
  (void)node_id;
  return ::util::OkStatus();
}

::util::Status PINode::PushForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&lock_);
  auto status = device_mgr_->pipeline_config_set(
      ::p4::v1::SetForwardingPipelineConfigRequest_Action_VERIFY_AND_COMMIT,
      config);
  // This is required by DeviceMgr in case the device is re-assigned internally
  device_mgr_->stream_message_response_register_cb(StreamMessageCb,
                                                   static_cast<void*>(this));
  pipeline_initialized_ = (status.code() == Code::OK);
  return toUtilStatus(status);
}

::util::Status PINode::SaveForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&lock_);
  auto status = device_mgr_->pipeline_config_set(
      ::p4::v1::SetForwardingPipelineConfigRequest_Action_VERIFY_AND_SAVE,
      config);
  // This is required by DeviceMgr in case the device is re-assigned internally
  device_mgr_->stream_message_response_register_cb(StreamMessageCb,
                                                   static_cast<void*>(this));
  return toUtilStatus(status);
}

::util::Status PINode::CommitForwardingPipelineConfig() {
  absl::WriterMutexLock l(&lock_);
  auto status = device_mgr_->pipeline_config_set(
      ::p4::v1::SetForwardingPipelineConfigRequest_Action_COMMIT,
      ::p4::v1::ForwardingPipelineConfig());
  pipeline_initialized_ = (status.code() == Code::OK);
  return toUtilStatus(status);
}

::util::Status PINode::VerifyForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  auto status = device_mgr_->pipeline_config_set(
      ::p4::v1::SetForwardingPipelineConfigRequest_Action_VERIFY, config);
  return toUtilStatus(status);
}

::util::Status PINode::Shutdown() {
  absl::WriterMutexLock l(&lock_);
  pipeline_initialized_ = false;
  return ::util::OkStatus();
}

::util::Status PINode::Freeze() { return ::util::OkStatus(); }

::util::Status PINode::Unfreeze() { return ::util::OkStatus(); }

::util::Status PINode::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_initialized_) {
    return MAKE_ERROR(ERR_INTERNAL) << "Pipeline not initialized";
  }
  if (!req.updates_size()) return ::util::OkStatus();  // nothing to do.
  RET_CHECK(results != nullptr)
      << "Need to provide non-null results pointer for non-empty updates.";

  auto status = device_mgr_->write(req);
  return toUtilStatus(status, results, req.updates_size());
}

::util::Status PINode::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_initialized_) {
    return MAKE_ERROR(ERR_INTERNAL) << "Pipeline not initialized";
  }
  RET_CHECK(writer) << "Channel writer must be non-null.";
  RET_CHECK(details) << "Details pointer must be non-null.";

  ::p4::v1::ReadResponse response;
  auto status = device_mgr_->read(req, &response);
  RETURN_IF_ERROR(toUtilStatus(status, details, req.entities_size()));
  if (!writer->Write(response))
    return MAKE_ERROR(ERR_INTERNAL) << "Write to stream channel failed.";
  return ::util::OkStatus();
}

::util::Status PINode::RegisterStreamMessageResponseWriter(
    std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>> writer) {
  absl::MutexLock l(&rx_writer_lock_);
  rx_writer_ = writer;
  // The StreamMessageCb callback is registered with the DeviceMgr instance when
  // the P4 forwarding pipeline is assigned.
  return ::util::OkStatus();
}

::util::Status PINode::UnregisterStreamMessageResponseWriter() {
  absl::MutexLock l(&rx_writer_lock_);
  rx_writer_ = nullptr;
  return ::util::OkStatus();
}

::util::Status PINode::HandleStreamMessageRequest(
    const ::p4::v1::StreamMessageRequest& request) {
  absl::ReaderMutexLock l(&lock_);
  if (!pipeline_initialized_) {
    return MAKE_ERROR(ERR_INTERNAL) << "Pipeline not initialized";
  }
  return toUtilStatus(device_mgr_->stream_message_request_handle(request));
}

void PINode::SendStreamMessageResponse(
    const ::p4::v1::StreamMessageResponse& response) {
  absl::MutexLock l(&rx_writer_lock_);
  if (rx_writer_ == nullptr) return;
  rx_writer_->Write(response);
}

}  // namespace pi
}  // namespace hal
}  // namespace stratum
