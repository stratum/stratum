#!/bin/bash
# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e

if [ -z "$1" ]; then
  echo "Usage: $(basename "$0") <Stratum BF-SDE deps package>.tgz"
  exit 1
fi

: ${SDE:?"Barefoot SDE variable is not set"}
echo "SDE: $SDE"
: ${SDE_INSTALL:?"Barefoot SDE_INSTALL is not set"}
echo "SDE_INSTALL: $SDE_INSTALL"

set -x
tmpdir="$(mktemp -d /tmp/bf_sde_install.XXXXXX)"

# Copy install directory and strip shared libraries
cp -ar $SDE_INSTALL/. $tmpdir
find $tmpdir -name "*\.so*" -a -type f | xargs -n1 chmod +w
find $tmpdir -name "*\.so*" -a -type f | xargs -n1 strip --strip-all
find $tmpdir -name "*\.so*" -a -type f | xargs -n1 chmod -w

# Copy required SDE sources and make any required changes
mkdir -p $tmpdir/src/bf_rt/proto
cp -an $SDE/pkgsrc/bf-drivers/src/bf_rt/proto/* $tmpdir/src/bf_rt/proto
sed -i'' 's#<google/rpc/status.grpc.pb.h>#"google/rpc/status.pb.h"#' \
  $tmpdir/src/bf_rt/proto/bf_rt_server_impl.hpp
sed -i'' 's#<google/rpc/code.grpc.pb.h>#"google/rpc/code.pb.h"#' \
  $tmpdir/src/bf_rt/proto/bf_rt_server_impl.hpp

mkdir -p $tmpdir/src/bf_rt/bf_rt_common
cp -an $SDE/pkgsrc/bf-drivers/src/bf_rt/bf_rt_common/* $tmpdir/src/bf_rt/bf_rt_common

mkdir -p $tmpdir/include/pipe_mgr
cp -an $SDE/pkgsrc/bf-drivers/include/pipe_mgr/pktgen_intf.h $tmpdir/include/pipe_mgr

cp -anr $SDE/pkgsrc/bf-drivers/kdrv $tmpdir/src

# Build the Stratum BF SDE Install archive
tar czf $1 -C $tmpdir .
rm -rf $tmpdir
