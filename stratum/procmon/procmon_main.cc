// Copyright 2019 Google LLC
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

#include "grpcpp/grpcpp.h"
#include <memory>

#include "gflags/gflags.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/procmon/procmon.h"
#include "stratum/procmon/procmon.pb.h"
#include "stratum/procmon/procmon_service_impl.h"
#include "stratum/glue/status/status.h"

DEFINE_string(procmon_config_file, "",
              "Path to Procmon configuration proto file.");
DEFINE_string(procmon_service_addr, stratum::kProcmonServiceUrl,
              "Url of the procmon service to listen to.");

namespace stratum {

namespace procmon {

::util::Status Main(int argc, char**argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  // Read the procmon config.
  CHECK_RETURN_IF_FALSE(!FLAGS_procmon_config_file.empty())
      << "Flag procmon_config_file must be specified.";
  ProcmonConfig config;
  RETURN_IF_ERROR(ReadProtoFromTextFile(FLAGS_procmon_config_file, &config));

  // Create and start the procmon gRPC service.
  ProcmonServiceImpl procmon_service_impl;
  ::grpc::ServerBuilder builder;
  builder.AddListeningPort(FLAGS_procmon_service_addr,
                           ::grpc::InsecureServerCredentials());
  builder.RegisterService(&procmon_service_impl);
  std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
  LOG(INFO) << "Procmon gRPC service started.";

  // Start Procmon class instance and run it.
  Procmon procmon(std::make_shared<ProcessHandler>());
  ::util::Status status = procmon.Run(config);  // blocking
  server->Wait();
  if (status.ok()) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Procmon::Run should never return with an ok status.";
  }

  return status;
}

}  // namespace procmon

}  // namespace stratum

int main(int argc, char** argv) {
  ::util::Status ret = stratum::procmon::Main(argc, argv);
  return ret.ok() ? 0 : 1;
}
