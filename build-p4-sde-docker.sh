#!/bin/bash
# Copyright 2022-present Intel Corporation
# SPDX-License-Identifier: Apache-2.0
set -ex

DOCKERFILE_DIR=$( cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd )
STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$DOCKERFILE_DIR/" >/dev/null 2>&1 && pwd )"}
DOCKER_IMG=${DOCKER_IMG:-stratumproject/build:build}
# STRATUM_BF_DIR=$( cd "$DOCKERFILE_DIR/.." >/dev/null 2>&1 && pwd )
# STRATUM_TARGET=${STRATUM_TARGET:-stratum_bfrt}
JOBS=${JOBS:-4}

print_help() {
echo "
The script builds containerized version of Stratum for Barefoot Tofino based device.
It also builds the kernel module if kernel header tarball is given.
Usage: $0 [SDE_TAR [KERNEL_HEADERS_TAR]...]

Example:
    $0 ~/bf-sde-9.7.0.tgz
    $0 ~/bf-sde-9.7.0.tgz ~/linux-4.14.49-ONL.tar.xz
    SDE_INSTALL_TAR=~/bf-sde-9.7.0-install.tgz $0

Additional environment variables:
    SDE_INSTALL_TAR: Tar archive of BF SDE install (set to skip SDE build)
    SDE_INSTALL: Path to BF SDE install directory (set to skip SDE build)
    STRATUM_TARGET: stratum_bfrt (Default: stratum_bfrt)
    STRATUM_ROOT: The root directory of Stratum.
    JOBS: The number of jobs to run simultaneously while building the base container. (Default: 4)
    DOCKER_IMG: Docker image to use for building (Default: stratumproject/build:build)
    RELEASE_BUILD: Optimized build with stripped symbols (Default: false)
    BAZEL_CACHE: Path to Bazel cache (Default: <empty>)
    BSP: Path to optional BSP package directory (Default: <empty>)
"
}

DOCKER_EXTRA_RUN_OPTS=""
if [ -t 0 ]; then
  # Running in a TTY, so run interactively (i.e. make Ctrl-C work)
  DOCKER_EXTRA_RUN_OPTS+="-it "
fi

DOCKER_OPTS=""
DOCKER_OPTS+="-v $STRATUM_ROOT:/stratum "
DOCKER_OPTS+="-w /stratum "
DOCKER_OPTS+="--env JOBS=$JOBS "

echo "Building P4 TDI DPDK SDE"
docker run --rm \
  $DOCKER_OPTS \
  $DOCKER_EXTRA_RUN_OPTS \
  --entrypoint bash \
  $DOCKER_IMG -c "./build-p4-sde.sh"

exit 0

