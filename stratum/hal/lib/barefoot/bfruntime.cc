
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "stratum/hal/lib/barefoot/bfruntime.h"


namespace stratum {
namespace hal {
namespace barefoot {

BfRuntimeImpl::BfRuntimeImpl() {};
BfRuntimeImpl::~BfRuntimeImpl() {};

::grpc::Status BfRuntimeImpl::Write(
    ::grpc::ServerContext* context,
    const ::bfrt_proto::WriteRequest* request,
    ::bfrt_proto::WriteResponse* response) {
  return ::grpc::Status(
      ::grpc::StatusCode::UNIMPLEMENTED,
      "Write not supported yet");
};

::grpc::Status BfRuntimeImpl::Read(
      ::grpc::ServerContext* context,
      const ::bfrt_proto::ReadRequest* request,
      ::grpc::ServerWriter< ::bfrt_proto::ReadResponse>* writer) {
  return ::grpc::Status(
      ::grpc::StatusCode::UNIMPLEMENTED,
      "READ not supported yet");
};

::grpc::Status BfRuntimeImpl::SetForwardingPipelineConfig(
      ::grpc::ServerContext* context, 
      const ::bfrt_proto::SetForwardingPipelineConfigRequest* request,
      ::bfrt_proto::SetForwardingPipelineConfigResponse* response) {
  return ::grpc::Status(
      ::grpc::StatusCode::UNIMPLEMENTED,
      "SetForwardingPipelineConfig not supported yet");
};

::grpc::Status BfRuntimeImpl::GetForwardingPipelineConfig(
      ::grpc::ServerContext* context,
      const ::bfrt_proto::GetForwardingPipelineConfigRequest* request,
      ::bfrt_proto::GetForwardingPipelineConfigResponse* response) {
  return ::grpc::Status(
      ::grpc::StatusCode::UNIMPLEMENTED,
      "GetForwardingPipelineConfig not supported yet");
};

::grpc::Status BfRuntimeImpl::StreamChannel(
      ::grpc::ServerContext* context,
      ::grpc::ServerReaderWriter<
          ::bfrt_proto::StreamMessageResponse,
          ::bfrt_proto::StreamMessageRequest>* stream) {
  ::bfrt_proto::StreamMessageRequest req;
  while (stream->Read(&req));  // wait for client to close side
  
  ::bfrt_proto::StreamMessageResponse resp;
  auto notification = resp.mutable_idle_timeout_notification();
  auto target_device = notification->mutable_target();
  target_device->set_device_id(1);
  target_device->set_pipe_id(2);
  target_device->set_direction(3);
  target_device->set_prsr_id(4);
  ::absl::Duration p1;
  ::absl::Duration p2;
  ::absl::Duration p3;
  // outside
  // auto table_entry = notification->mutable_table_entry();
  // table_entry->set_table_id(5);
  // auto table_key = table_entry->mutable_key();
  // auto key_field = table_key->add_fields();
  // key_field->mutable_exact()->set_value("6"); 
  // auto table_data = table_entry->mutable_data();
  // table_data->set_action_id(7);
  // auto data_field = table_data->add_fields();
  // data_field->set_field_id(8);
  while (!context->IsCancelled()) {
    // inside
    // ::absl::Time t1 = ::absl::Now();
    auto table_entry = notification->mutable_table_entry();
    table_entry->set_table_id(5);
    auto table_key = table_entry->mutable_key();
    auto key_field = table_key->add_fields();
    key_field->mutable_exact()->set_value("6");
    auto table_data = table_entry->mutable_data();
    table_data->set_action_id(7);
    auto data_field = table_data->add_fields();
    data_field->set_field_id(8);
    TODO(bocon) other fields...
    // ::absl::Time t2 = ::absl::Now();

    // uint8_t data[10000000];
    // ::google::protobuf::io::ZeroCopyOutputStream* out =
    //     new ::google::protobuf::io::ArrayOutputStream(data, sizeof(data));
    // if(!resp.SerializeToZeroCopyStream(out)) break;
    // ::absl::Time t3 = ::absl::Now();
    if(!stream->Write(resp)) break;
    // ::absl::Time t4 = ::absl::Now();
    // p1 += t2 - t1;
    // p2 += t3 - t2;
    // p3 += t4 - t3;
  }
  std::cout << "p1: " << p1
           << " p2: " << p2
           << " p3: " << p3
           << std::endl;

  if (context->IsCancelled()) {
    return ::grpc::Status(
        ::grpc::StatusCode::CANCELLED,
        "Stream was cancelled by client.");
  }
  return ::grpc::Status::OK;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
