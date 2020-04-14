#!/bin/bash
#
# Copyright 2020-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

DOCKERFILE_DIR=$(dirname "${BASH_SOURCE[0]}")
STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$DOCKERFILE_DIR/../../../../.." >/dev/null 2>&1 && pwd )"}
JOBS=${JOBS:-4}

print_help() {
cat << EOF

The script builds containerized version of Stratum for NP4 Intel based device. It installs NP4 SDK libraries using Dockerfile.builder and saves artifacts to an intermediate builder image. Then it runs bazel build for Stratum code base and copies libraries from builder to runtime image using Dockerfile.runtime.

Usage: $0 [options] -- NP4_TAR
    [--local]                       Use the local stratum binaries

Example:
    $0 -- ~/np4_intel_4_7_1-1.tgz
    $0 --local -- ~/np4_intel_4_7_1-1.tgz

EOF
}


# Process the options
while [[ $# -gt 0 ]]
do
    key="$1"
    case $key in
    -h|--help)
        print_help
        exit 0
        ;;
    --local)
        LOCAL=YES
        shift
        ;;
    "--")
        shift
        break
        ;;
    *)  # unknown option
        print_help
        exit 1
        ;;
    esac
done

# We need at least 1 arg
if [ "$#" -eq 0 ]; then
    print_help
    exit 1
fi

BUILD_ARGS="--build-arg JOBS=$JOBS"

# Grab the NP4 tarball
echo """Copying NP4 tarball to $DOCKERFILE_DIR/
NOTE: Copied tarballs will be DELETED after the build"""

if [ -n "$1" ]; then
    NP4_TAR=$(basename $1)
    BUILD_ARGS="$BUILD_ARGS --build-arg NP4_TAR=$NP4_TAR"
    cp -f $1 $DOCKERFILE_DIR
fi

# Build NP4 SDK and DPDK
BUILDER_IMAGE=stratumproject/stratum-np4intel-builder
echo "Building $BUILDER_IMAGE"
docker build -t $BUILDER_IMAGE $BUILD_ARGS \
	 -f $DOCKERFILE_DIR/Dockerfile.builder $DOCKERFILE_DIR
ERR=$?
if [ $ERR -ne 0 ]; then
    >&2 echo "ERROR: Error while building $BUILDER_IMAGE"
    exit $ERR
fi

# Remove copied tarballs
if [ -f "$DOCKERFILE_DIR/$NP4_TAR" ]; then
    rm -f $DOCKERFILE_DIR/$NP4_TAR
fi

# If "local" flag set we'll use the locally generated stratum binaries
# in the current directory
RUNTIME_IMAGE=stratumproject/stratum-np4intel
echo "Building $RUNTIME_IMAGE"
if [ "$LOCAL" == YES ]; then
    docker build -t $RUNTIME_IMAGE \
	         --build-arg JOBS=$JOBS \
             --build-arg BUILDER_IMAGE=$BUILDER_IMAGE \
             -f $DOCKERFILE_DIR/Dockerfile.runtime.local $STRATUM_ROOT

# Else we'll build and generate runtime image
else
    docker build -t $RUNTIME_IMAGE \
	         --build-arg JOBS=$JOBS \
             --build-arg BUILDER_IMAGE=$BUILDER_IMAGE \
             -f $DOCKERFILE_DIR/Dockerfile.runtime $STRATUM_ROOT
fi

ERR=$?
if [ $ERR -ne 0 ]; then
    >&2 echo "ERROR: Error while building $RUNTIME_IMAGE"
    exit $ERR
fi

