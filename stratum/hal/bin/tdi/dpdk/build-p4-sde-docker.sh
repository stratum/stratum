#!/bin/bash
# Copyright 2022-present Intel Corporation
# SPDX-License-Identifier: Apache-2.0
set -ex

THIS_DIR=$( cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd )
STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$THIS_DIR/../../../../.." >/dev/null 2>&1 && pwd )"}

DOCKER_IMG=${DOCKER_IMG:-stratumproject/build:build}
JOBS=${JOBS:-4}

print_help() {
echo "
The script starts the build of the P4 DPDK backend inside a dockerized
environment, using the regular Stratum build image by default.
Usage: $0

Example:
    $0
    JOBS=16 $0
    DOCKER_IMG=foo/my-base-image $0

Additional environment variables:
    JOBS: The number of jobs to run simultaneously while building the base container. (Default: 4)
    DOCKER_IMG: Docker image to use for building (Default: stratumproject/build:build)
    STRATUM_ROOT: The root directory of Stratum.
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
  $DOCKER_IMG -c "stratum/hal/bin/tdi/dpdk/build-p4-sde.sh"

exit 0

