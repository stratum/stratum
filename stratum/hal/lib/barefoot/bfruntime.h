
#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRUNTIME_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRUNTIME_H_

#include "bfruntime.grpc.pb.h"
#include "grpcpp/grpcpp.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfRuntimeImpl final : public ::bfrt_proto::BfRuntime::Service {
 public:
  BfRuntimeImpl();
  virtual ~BfRuntimeImpl();

  // Update one or more P4 entities on the target.
  ::grpc::Status Write(
      ::grpc::ServerContext* context,
      const ::bfrt_proto::WriteRequest* request,
      ::bfrt_proto::WriteResponse* response) override;

  // Read one or more P4 entities from the target.
  ::grpc::Status Read(
      ::grpc::ServerContext* context,
      const ::bfrt_proto::ReadRequest* request,
      ::grpc::ServerWriter< ::bfrt_proto::ReadResponse>* writer) override;

  // Sets the P4 fowarding-pipeline config.
  ::grpc::Status SetForwardingPipelineConfig(
      ::grpc::ServerContext* context, 
      const ::bfrt_proto::SetForwardingPipelineConfigRequest* request,
      ::bfrt_proto::SetForwardingPipelineConfigResponse* response) override;

  // Gets the current P4 fowarding-pipeline config.
  ::grpc::Status GetForwardingPipelineConfig(
      ::grpc::ServerContext* context,
      const ::bfrt_proto::GetForwardingPipelineConfigRequest* request,
      ::bfrt_proto::GetForwardingPipelineConfigResponse* response) override;

  // Represents the bidirectional stream between the controller and the
  // switch (initiated by the controller).
  ::grpc::Status StreamChannel(
      ::grpc::ServerContext* context,
      ::grpc::ServerReaderWriter<
          ::bfrt_proto::StreamMessageResponse,
          ::bfrt_proto::StreamMessageRequest>* stream) override;

  BfRuntimeImpl(const BfRuntimeImpl&) = delete;
  BfRuntimeImpl& operator=(const BfRuntimeImpl&) = delete;

};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRUNTIME_H_