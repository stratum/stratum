
#include <iostream>
#include <string>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "stratum/hal/lib/barefoot/perf/bfruntime.grpc.pb.h"
#include <grpcpp/grpcpp.h>


namespace stratum {
namespace hal {
namespace barefoot {

using ClientStreamChannelReaderWriter =
    ::grpc::ClientReaderWriter<
        ::bfrt_proto::StreamMessageRequest,
        ::bfrt_proto::StreamMessageResponse >;


void* SleepThenEnd(void* arg) {
  ::grpc::ClientContext* context = static_cast<::grpc::ClientContext*>(arg);
  absl::SleepFor(absl::Seconds(5));
  context->TryCancel();
  return nullptr;
}

void RunClient() {
  const std::string url = "localhost:50051";
  std::unique_ptr<::bfrt_proto::BfRuntime::Stub> bfrt_stub_(
      ::bfrt_proto::BfRuntime::NewStub(
          ::grpc::CreateChannel(
              url, ::grpc::InsecureChannelCredentials())));

  ::grpc::ClientContext context;
  std::unique_ptr<ClientStreamChannelReaderWriter> stream =
      bfrt_stub_->StreamChannel(&context);

  pthread_t tid = 0; 
  if (pthread_create(&tid, nullptr, &SleepThenEnd, &context) != 0) {
    std::cout  << "Failed to create the TX thread." << std::endl;;
    stream->Finish();
    return;
  }

  if(!stream->WritesDone()) {
    std::cout << "failed to connect" << std::endl;
    return;
  };
  absl::Time start = absl::Now();

  ::bfrt_proto::StreamMessageResponse resp;
  if(!stream->Read(&resp)) return;
  absl::Time first = absl::Now();
  uint64_t count = 0;
  while(stream->Read(&resp)) {
    count++;
  }
  absl::Time end = absl::Now();

  auto delta = end - first;
  std::cout << "Read " << count << " in " << delta << std::endl;
  std::cout << "Time to first read: " << first - start << std::endl;
  std::cout << "100k in " << 100000.0/count * delta << std::endl;


  ::grpc::Status status = stream->Finish();
    if (!status.ok()) {
      std::cout << "Stream failed with the following error: "
                << status.error_message() << std::endl;
  }

  if (pthread_join(tid, nullptr) != 0) {
    std::cout << "Failed to join the sleep thread." << std::endl;
  }

}

}  // namespace stub
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  stratum::hal::barefoot::RunClient();
  return 0;
}
