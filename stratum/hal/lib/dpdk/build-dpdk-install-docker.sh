#!/bin/bash
# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -eu

DOCKERFILE_DIR=$( cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd )

DPDK_TAR="dpdk-install.tar.xz"
docker build -t dpdk-base $DOCKERFILE_DIR/.
docker run --rm --entrypoint cat dpdk-base "/$DPDK_TAR" > "$DPDK_TAR"

echo "Build DPDK tar: $PWD/$DPDK_TAR"
