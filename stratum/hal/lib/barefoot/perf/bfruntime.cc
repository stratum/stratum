
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "bfruntime.h"


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


void BfRuntimeImpl::sendStreamMessage(
    const bfrt_proto::StreamMessageResponse &response) const {
  if (stream_ == nullptr) {
    // Indicates that a bi-directional stream was not opened by the client
    // to receive any callbacks from the server
    return;
  }

  // Next we need to check if the ServerReaderWriter stream pointer managed by
  // grpc and cached in the connection_data object is actually still valid
  // and only send out the response when the stream is actually valid
  // This is important to check because we are caching a pointer to the stream
  // that is owned by the grpc infra. Thus when the client disconnects, the
  // thread which was held up in the StreamChannel RPC will break from the
  // while loop and return. This will cause grpc infra to invalidate it. Now,
  // it might so happen that another thread (learn cb) might be in the middle
  // of sending learn data to the client on this stream (which is no longer
  // valid).
  {
    std::lock_guard<std::mutex> stream_lock(getStreamChannelRWMutex());
    if (getStreamChannelRWValidFlag()) {
      stream_->Write(response);
    }
  }
}

static const char* ptr = nullptr;
void BfRuntimeImpl::WriteResponse() {
    // ::bfrt_proto::StreamMessageResponse resp;
    // auto notification = resp.mutable_idle_timeout_notification();
    // auto target_device = notification->mutable_target();
    // target_device->set_device_id(1);
    // target_device->set_pipe_id(2);
    // target_device->set_direction(3);
    // target_device->set_prsr_id(4);
    // auto table_entry = notification->mutable_table_entry();
    // table_entry->set_table_id(5);
    // auto table_key = table_entry->mutable_key();
    // auto key_field = table_key->add_fields();
    // key_field->mutable_exact()->set_value("6");
    // auto table_data = table_entry->mutable_data();
    // table_data->set_action_id(7);
    // auto data_field = table_data->add_fields();
    // data_field->set_field_id(8);
    // //TODO(bocon) other fields...


      bfrt_proto::StreamMessageResponse response;
  auto notification = response.mutable_idle_timeout_notification();
  auto table_entry = notification->mutable_table_entry();
  auto dev_tgt = notification->mutable_target();
  dev_tgt->set_device_id(0);
  dev_tgt->set_pipe_id(1);

  table_entry->set_table_id(rand());
  auto table_key = table_entry->mutable_key();

  // for (const auto &field_id : field_ids) {
  for (int i = 0; i < rand() % 5; i++) {
    auto field = table_key->add_fields();
    field->set_field_id(rand());
    // KeyFieldType type;
    // bf_status = table->keyFieldTypeGet(field_id, &type);
    // size_t size;
    // table->keyFieldSizeGet(field_id, &size);
    // size = (size + 7) / 8;
    int size = 2;
    // switch (type) {
    //   case KeyFieldType::EXACT: {
    switch(rand() % 2){
      case 0: {
        std::vector<uint8_t> value(size);
        value[0] = rand();
        value[1] = rand();
        // bf_status = key->getValue(field_id, size, &value[0]);
        auto exact = field->mutable_exact();
        exact->set_value(&value[0], size);
      } break;
      // } break;
      // case KeyFieldType::TERNARY: {
      //   std::vector<uint8_t> value(size);
      //   std::vector<uint8_t> mask(size);
      //   bf_status = key->getValueandMask(field_id, size, &value[0], &mask[0]);
      //   auto ternary = field->mutable_ternary();
      //   ternary->set_value(&value[0], size);
      //   ternary->set_mask(&mask[0], size);
      // } break;
      // case KeyFieldType::LPM: {
      case 1: {
        std::vector<uint8_t> value(size);
        value[0] = rand();
        value[1] = rand();
        uint16_t prefix_len = rand();
        // bf_status = key->getValueLpm(field_id, size, &value[0], &prefix_len);
        auto lpm = field->mutable_lpm();
        lpm->set_value(&value[0], size);
        lpm->set_prefix_len(prefix_len);
      } break;
      // case KeyFieldType::RANGE: {
      //   std::vector<uint8_t> start(size);
      //   std::vector<uint8_t> end(size);
      //   bf_status = key->getValueRange(field_id, size, &start[0], &end[0]);
      //   auto range = field->mutable_range();
      //   range->set_low(&start[0], size);
      //   range->set_high(&end[0], size);
      // } break;
    }
  }

    // ptr = notification->DebugString().c_str();

    sendStreamMessage(response);

    {
      table_entry->set_table_id(rand());
      auto table_key = table_entry->mutable_key();
      auto field = table_key->add_fields();
      field->set_field_id(rand());
    }
    sendStreamMessage(response);

}


::grpc::Status BfRuntimeImpl::StreamChannel(
      ::grpc::ServerContext* context,
      StreamChannelReaderWriter* stream) {
  ::bfrt_proto::StreamMessageRequest req;
  while (stream->Read(&req));  // wait for client to close side
  
  stream_ = stream;
  //::absl::Duration p1;
  //::absl::Duration p2;
  //::absl::Duration p3;
  // ::absl::Duration min_write_time = ::absl::Seconds(100);
  // ::absl::Duration max_write_time;
  // ::absl::Duration total_write_time;
  //int write_count = 0;
  while (!context->IsCancelled()) {
    // ::absl::Time t1 = ::absl::Now();
    // ::absl::Time t2 = ::absl::Now();

    // uint8_t data[10000000];
    // ::google::protobuf::io::ZeroCopyOutputStream* out =
    //     new ::google::protobuf::io::ArrayOutputStream(data, sizeof(data));
    // if(!resp.SerializeToZeroCopyStream(out)) break;

    // ::absl::Time t3 = ::absl::Now();
    // if(!stream->Write(resp)) break;
    // ::absl::Time t4 = ::absl::Now();
    // auto delta = t4 - t3;
    // if (delta > max_write_time) max_write_time = delta;
    // if (delta < min_write_time) min_write_time = delta;
    // total_write_time += delta;
    // write_count++;

    WriteResponse();
    // p1 += t2 - t1;
    // p2 += t3 - t2;
    // p3 += t4 - t3;
  }
  //std::cout << "p1: " << p1
  //         << " p2: " << p2
  //         << " p3: " << p3
  //         << std::endl;
  // std::cout << "max write: " << max_write_time
  //           << " min write: " << min_write_time
  //           << " avg write: " << total_write_time / write_count
  //           << std::endl;

  if (context->IsCancelled()) {
    std::cout << "cancel" << std::endl; 
    return ::grpc::Status(
        ::grpc::StatusCode::CANCELLED,
        "Stream was cancelled by client.");
  }
  std::cout << "ok" << std::endl; 
  return ::grpc::Status::OK;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
