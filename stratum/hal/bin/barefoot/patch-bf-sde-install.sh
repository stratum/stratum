#!/bin/bash
# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e

echo "Patching SDE_INSTALL for Stratum"
: ${SDE:?"Barefoot SDE variable is not set"}
echo "SDE: $SDE"
: ${SDE_INSTALL:?"Barefoot SDE_INSTALL is not set"}
echo "SDE_INSTALL: $SDE_INSTALL"


set -x
# Copy required SDE sources for Stratum and make any required changes
mkdir -p $SDE_INSTALL/src/bf_rt/proto
cp -a $SDE/pkgsrc/bf-drivers/src/bf_rt/proto/* $SDE_INSTALL/src/bf_rt/proto
sed -i'' -e 's#<google/rpc/status.grpc.pb.h>#"google/rpc/status.pb.h"#' \
  $SDE_INSTALL/src/bf_rt/proto/bf_rt_server_impl.hpp
sed -i'' -e 's#<google/rpc/code.grpc.pb.h>#"google/rpc/code.pb.h"#' \
  $SDE_INSTALL/src/bf_rt/proto/bf_rt_server_impl.hpp

mkdir -p $SDE_INSTALL/src/bf_rt/bf_rt_common
cp -a $SDE/pkgsrc/bf-drivers/src/bf_rt/bf_rt_common/* $SDE_INSTALL/src/bf_rt/bf_rt_common

mkdir -p $SDE_INSTALL/include/pipe_mgr
cp -a $SDE/pkgsrc/bf-drivers/include/pipe_mgr/pktgen_intf.h $SDE_INSTALL/include/pipe_mgr
