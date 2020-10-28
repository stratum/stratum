// Copyright 2020-present Open Networking Foundation
// Copyright (c) 2018-2019 Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_grpc_server.h"

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"

#ifdef WITH_BFRT_GRPC_SERVER
#include <utility>
#include "absl/memory/memory.h"
// TODO(bocon): try to fix the include path for bf_rt
#include "proto/bf_rt_server_impl.hpp"
#endif  // WITH_BFRT_GRPC_SERVER

DEFINE_bool(incompatible_enable_bfrt_grpc_server, false,
            "Enables the BFRuntime gRPC server");

DEFINE_string(incompatible_bfrt_grpc_server_addr, "127.0.0.1:50052",
              "Listening address for BFRuntime gRPC server");

namespace stratum {
namespace hal {
namespace barefoot {

void StartBfRtServerIfEnabled() {
  if (FLAGS_incompatible_enable_bfrt_grpc_server) {
#ifdef WITH_BFRT_GRPC_SERVER
    auto server_data = absl::make_unique<bfrt::server::ServerData>(
        "Stratum BFRuntime gRPC Server",
        FLAGS_incompatible_bfrt_grpc_server_addr);
    bfrt::server::BfRtServer::getInstance(std::move(server_data));
    LOG(INFO) << "Started BFRuntime gRPC server on "
        << FLAGS_incompatible_bfrt_grpc_server_addr;
#else
    LOG(ERROR)
        << "Tried to enable BFRuntime gRPC server, but it was not compiled.\n"
        << "  Recompile Stratum with: --define with_bfrt_grpc_server=true";
#endif  // WITH_BFRT_GRPC_SERVER
  }
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
