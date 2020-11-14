#include <iostream>
#include <memory>
#include <string>
#include <signal.h>

#include "bfruntime.h"

namespace stratum {
namespace hal {
namespace barefoot {

static std::unique_ptr<::grpc::Server> server;

void RunServer() {
  const std::string server_address("0.0.0.0:50051");
  BfRuntimeImpl service;

  ::grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, ::grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  builder.SetMaxSendMessageSize(10 * 1024 * 1024);

  server = builder.BuildAndStart();
  std::cout << "Server listening on " << server_address << std::endl;


  signal(SIGINT, [](int signum) {
    std::cout << "Shutting down..." << std::endl;
    server->Shutdown();
  });
  server->Wait();
}


}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  stratum::hal::barefoot::RunServer();
  return 0;
}
