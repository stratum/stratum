// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// Unit test for bfrt_server.

#include <string>

#include "gflags/gflags.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "stratum/glue/net_util/ports.h"
#include "stratum/hal/lib/barefoot/bfrt_grpc_server.h"

#ifdef WITH_BFRT_GRPC_SERVER
// TODO(bocon): try to fix the include path for bf_rt
#include "proto/bfruntime.grpc.pb.h"
#include "proto/bf_rt_server_impl.hpp"
#endif  // WITH_BFRT_GRPC_SERVER

DECLARE_bool(incompatible_enable_bfrt_grpc_server);
DECLARE_string(incompatible_bfrt_grpc_server_addr);

namespace stratum {
namespace hal {
namespace barefoot {

TEST(BfrtServerTest, BfrtServerStart) {
  ::gflags::FlagSaver flag_saver_;
  FLAGS_incompatible_enable_bfrt_grpc_server = true;
  std::string url = "127.0.0.1:" +
      std::to_string(stratum::PickUnusedPortOrDie());
  FLAGS_incompatible_bfrt_grpc_server_addr = url;

  StartBfRtServerIfEnabled();

#ifdef WITH_BFRT_GRPC_SERVER
  auto stub = ::bfrt_proto::BfRuntime::NewStub(
      ::grpc::CreateChannel(url, ::grpc::InsecureChannelCredentials()));
  ASSERT_NE(stub, nullptr);

  ::grpc::ClientContext ctx;
  ::grpc::Status status;
  ::bfrt_proto::GetForwardingPipelineConfigRequest req;
  ::bfrt_proto::GetForwardingPipelineConfigResponse resp;
  status = stub->GetForwardingPipelineConfig(&ctx, req, &resp);

  EXPECT_EQ(status.error_code(),
            ::bfrt::server::to_grpc_status(BF_NOT_READY, "").error_code());
#else
  ASSERT_TRUE(false) << "Recompile with: --define with_bfrt_grpc_server=true";
#endif  // WITH_BFRT_GRPC_SERVER
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
