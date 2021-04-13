// Copyright 2018-2019 Barefoot Networks, Inc.
// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/barefoot/bf_interface.h"

/*
INFO: From Compiling stratum/lib/barefoot/bf_interface.cc:
In file included from stratum/lib/barefoot/bf_interface.cc:10:0:
./stratum/glue/init_google.h:13:6: warning: "GOOGLE_BASE_HAS_INITGOOGLE" is not defined [-Wundef]
 #if !GOOGLE_BASE_HAS_INITGOOGLE
      ^~~~~~~~~~~~~~~~~~~~~~~~~~
*/

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
// TODO(bocon): absl Mutex
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/barefoot/bf_init.h"
#include "stratum/hal/lib/barefoot/bf_sde_interface.h"
#include "stratum/hal/lib/barefoot/bf_sde_wrapper.h"
#include "stratum/hal/lib/barefoot/bfrt_action_profile_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/bfrt_counter_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_node.h"
#include "stratum/hal/lib/barefoot/bfrt_packetio_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_pre_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"

namespace stratum {
namespace barefoot {

using namespace ::stratum::hal;
using namespace ::stratum::hal::barefoot;

class BfInterfaceImpl : public BfInterface {
 public:
  //   explicit TableKey(std::unique_ptr<bfrt::BfRtTableKey> table_key)
  //       : table_key_(std::move(table_key)) {}

  ::absl::Status SetForwardingPipelineConfig(
      const ::p4::v1::SetForwardingPipelineConfigRequest& req,
      ::p4::v1::SetForwardingPipelineConfigResponse* resp) override;
  ::absl::Status GetForwardingPipelineConfig(
      const ::p4::v1::GetForwardingPipelineConfigRequest& req,
      ::p4::v1::GetForwardingPipelineConfigResponse* resp) override;
  ::absl::Status Write(const ::p4::v1::WriteRequest& req,
                       ::p4::v1::WriteResponse* resp) override;
  ::absl::Status Read(const ::p4::v1::ReadRequest& req,
                      ::p4::v1::ReadResponse* resp) override;

  ::absl::Status InitSde(absl::string_view bf_sde_install,
                         absl::string_view bf_switchd_cfg,
                         bool bf_switchd_background) override
      LOCKS_EXCLUDED(init_lock_);

 private:
  // Private constructor, use CreateSingleton and GetSingleton().
  // BfInterfaceImpl();

  // BfRt Managers.
  std::unique_ptr<BfrtTableManager> bfrt_table_manager_;
  std::unique_ptr<BfrtActionProfileManager> bfrt_action_profile_manager_;
  std::unique_ptr<BfrtPacketioManager> bfrt_packetio_manager_;
  std::unique_ptr<BfrtPreManager> bfrt_pre_manager_;
  std::unique_ptr<BfrtCounterManager> bfrt_counter_manager_;
  std::unique_ptr<BfrtNode> bfrt_node_;
};

namespace {

::absl::Status ConvertStatusToAbsl(const ::util::Status& status) {
  if (status.ok()) return ::absl::OkStatus();
  // TODO(bocon): ensure code conversion matches
  return ::absl::Status(static_cast<::absl::StatusCode>(status.error_code()),
                        status.error_message());
}

}  // namespace

::absl::Status BfInterfaceImpl::SetForwardingPipelineConfig(
    const ::p4::v1::SetForwardingPipelineConfigRequest& req,
    ::p4::v1::SetForwardingPipelineConfigResponse* resp) {
  // TODO(bocon): assert the device id matches?
  ::util::Status status =
      bfrt_node_->PushForwardingPipelineConfig(req.config());
  // Nothing to do for the response; it's an empty proto
  return ConvertStatusToAbsl(status);
}

::absl::Status BfInterfaceImpl::GetForwardingPipelineConfig(
    const ::p4::v1::GetForwardingPipelineConfigRequest& req,
    ::p4::v1::GetForwardingPipelineConfigResponse* resp) {
  // TODO(bocon): implement this method; note: we don't store the config yet
  return absl::UnimplementedError("unimplemented");
}

::absl::Status BfInterfaceImpl::Write(const ::p4::v1::WriteRequest& req,
                                      ::p4::v1::WriteResponse* resp) {
  std::vector<::util::Status> results;
  ::util::Status status = bfrt_node_->WriteForwardingEntries(req, &results);
  // Nothing to do for the response; it's an empty proto
  if (!status.ok()) {
    for (const auto& result : results) {
      LOG(ERROR) << result;
      // result.CanonicalCode()
      // result.error_code()
      // result.error_message()
    }
  }
  return ConvertStatusToAbsl(status);
}

::absl::Status BfInterfaceImpl::Read(const ::p4::v1::ReadRequest& req,
                                     ::p4::v1::ReadResponse* resp) {
  // TODO(bocon): implement this method
  // bfrt_node_->ReadForwardingEntries(req);
  return absl::UnimplementedError("unimplemented");
}

::absl::Status BfInterfaceImpl::InitSde(absl::string_view bf_sde_install,
                                        absl::string_view bf_switchd_cfg,
                                        bool bf_switchd_background) {
  // Initialize bf_switchd library.
  {
    // TODO(bocon): See if we can pass the char* without a copy
    int status = InitBfSwitchd(std::string(bf_sde_install).c_str(),
                               std::string(bf_switchd_cfg).c_str(),
                               bf_switchd_background);
    if (status != 0)
      return absl::InternalError(
          absl::StrFormat("Error when starting switchd, status: %d", status));
  }

  // TODO(antonin): The SDE expects 0-based device ids, so we instantiate
  // components with "device_id" instead of "node_id".
  int device_id = 0;

  auto bf_sde_wrapper = BfSdeWrapper::CreateSingleton();

  auto result = bf_sde_wrapper->IsSoftwareModel(device_id);
  bool is_sw_model;
  if (result.ok())
    is_sw_model = result.ValueOrDie();
  else
    return ConvertStatusToAbsl(result.status());
  const OperationMode mode =
      is_sw_model ? OPERATION_MODE_SIM : OPERATION_MODE_STANDALONE;
  LOG(INFO) << "Detected is_sw_model: " << is_sw_model;
  LOG(INFO) << "SDE version: " << bf_sde_wrapper->GetSdeVersion();

  bfrt_table_manager_ =
      BfrtTableManager::CreateInstance(mode, bf_sde_wrapper, device_id);
  bfrt_action_profile_manager_ =
      BfrtActionProfileManager::CreateInstance(bf_sde_wrapper, device_id);
  bfrt_packetio_manager_ =
      BfrtPacketioManager::CreateInstance(bf_sde_wrapper, device_id);
  bfrt_pre_manager_ = BfrtPreManager::CreateInstance(bf_sde_wrapper, device_id);
  bfrt_counter_manager_ =
      BfrtCounterManager::CreateInstance(bf_sde_wrapper, device_id);
  bfrt_node_ = BfrtNode::CreateInstance(
      bfrt_table_manager_.get(), bfrt_action_profile_manager_.get(),
      bfrt_packetio_manager_.get(), bfrt_pre_manager_.get(),
      bfrt_counter_manager_.get(), bf_sde_wrapper, device_id);
  return absl::OkStatus();
}

BfInterface* BfInterface::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex BfInterface::init_lock_(absl::kConstInit);

BfInterface* BfInterface::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new BfInterfaceImpl();
  }

  return singleton_;
}

BfInterface* BfInterface::GetSingleton() {
  absl::ReaderMutexLock l(&init_lock_);
  return singleton_;
}

}  // namespace barefoot
}  // namespace stratum

using ::stratum::barefoot::BfInterface;

// A macro for simplify checking of arguments in the C functions.
#define CHECK_RETURN_CODE_IF_FALSE(cond) \
  if (ABSL_PREDICT_TRUE(cond)) {         \
  } else /* NOLINT */                    \
    return static_cast<int>(::absl::StatusCode::kInvalidArgument)

// A macro that converts an absl::Status to an int and returns it.
#define RETURN_STATUS(status) return static_cast<int>(status.code())

// TODO(bocon): consider free if response not null
// A macro that converts between binary and C++ representations of
// a protobuf. The C++ objects are used to call the C++ function.
#define RETURN_CPP_API(RequestProto, ResponseProto, Function)    \
  CHECK_RETURN_CODE_IF_FALSE(*packed_response == NULL);          \
  RequestProto request;                                          \
  CHECK_RETURN_CODE_IF_FALSE(                                    \
      request.ParseFromArray(packed_request, request_size));     \
  ResponseProto response;                                        \
  ::absl::Status status =                                        \
      BfInterface::GetSingleton()->Function(request, &response); \
  *response_size = response.ByteSizeLong();                      \
  *packed_response = malloc(*response_size);                     \
  response.SerializeToArray(*packed_response, *response_size);   \
  RETURN_STATUS(status)

int bf_p4_init(const char* bf_sde_install, const char* bf_switchd_cfg,
            bool bf_switchd_background) {
  // Check if the SDE has already been initialized; presumably if the singleton
  // has been created.
  if (BfInterface::GetSingleton() != nullptr) return -1;
  RETURN_STATUS(BfInterface::CreateSingleton()->InitSde(
      bf_sde_install, bf_switchd_cfg, bf_switchd_background));
  return 0;
}

int bf_p4_destroy() {
  // TODO(bocon): Free bf_interface_ and teardown SDE
  return 0;
}

int bf_p4_set_pipeline_config(const PackedProtobuf packed_request,
                              size_t request_size,
                              PackedProtobuf* packed_response,
                              size_t* response_size) {
  RETURN_CPP_API(::p4::v1::SetForwardingPipelineConfigRequest,
                 ::p4::v1::SetForwardingPipelineConfigResponse,
                 SetForwardingPipelineConfig);
}

int bf_p4_get_pipeline_config(const PackedProtobuf packed_request,
                              size_t request_size,
                              PackedProtobuf* packed_response,
                              size_t* response_size) {
  RETURN_CPP_API(::p4::v1::GetForwardingPipelineConfigRequest,
                 ::p4::v1::GetForwardingPipelineConfigResponse,
                 GetForwardingPipelineConfig);
}

int bf_p4_write(const PackedProtobuf packed_request, size_t request_size,
                PackedProtobuf* packed_response, size_t* response_size) {
  RETURN_CPP_API(::p4::v1::WriteRequest, ::p4::v1::WriteResponse, Write);
}

int bf_p4_read(const PackedProtobuf packed_request, size_t request_size,
               PackedProtobuf* packed_response, size_t* response_size) {
  RETURN_CPP_API(::p4::v1::ReadRequest, ::p4::v1::ReadResponse, Read);
}
