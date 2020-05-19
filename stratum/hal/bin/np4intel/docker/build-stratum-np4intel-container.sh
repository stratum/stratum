#!/bin/bash
#
# Copyright 2019-present Dell EMC
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

DOCKERFILE_DIR=$(dirname "${BASH_SOURCE[0]}")
STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$DOCKERFILE_DIR/../../../../.." >/dev/null 2>&1 && pwd )"}
JOBS=${JOBS:-4}
NP4_CHKSUM="a990c39bffb078d625d7a99d7ebff21e6e012a47c2f4c0579b70dd6eeb8c0294"

print_help() {
cat << EOF

The script builds containerized version of Stratum for NP4 Intel based device. It installs NP4 SDK libraries using Dockerfile.builder and saves artifacts to an intermediate builder image. Then it runs bazel build for Stratum code base and copies libraries from builder to runtime image using Dockerfile.runtime.

Usage: $0 [options] -- NP4_BIN
    [--local]                       Use the local stratum binaries
    [--skip-chksum]                 Skip Netcope SDK checksum test

Example:
    $0 -- np4-intel-n3000-4.7.1-1-ubuntu.bin
    $0 --local -- np4-intel-n3000-4.7.1-1-ubuntu.bin

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
    --skip-chksum)
        SKIP_CHKSUM=YES
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

# Grab the NP4 binary
echo """Copying NP4 binary to $DOCKERFILE_DIR/
NOTE: Copied binary will be DELETED after the build"""

if [ -n "$1" ]; then
    NP4_BIN=$(basename $1)
    BUILD_ARGS="$BUILD_ARGS --build-arg NP4_BIN=$NP4_BIN"
    cp -f $1 $DOCKERFILE_DIR
fi

# Make sure the Netcope SDK binary matches our checksum
if [ "$SKIP_CHKSUM" != YES ]; then
    NP4_SHA_FILE=${NP4_BIN:0:-4}.sha256
    echo "$NP4_CHKSUM $NP4_BIN" >$NP4_SHA_FILE
    sha256sum -c $NP4_SHA_FILE
    ERR=$?
    if [ $ERR -ne 0 ]; then
        >&2 echo "ERROR: NP4 binary checksum failed"
        exit $ERR
    fi
    rm $NP4_SHA_FILE
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
if [ -f "$DOCKERFILE_DIR/$NP4_BIN" ]; then
    rm -f $DOCKERFILE_DIR/$NP4_BIN
fi

# If "local" flag set we'll use the locally generated stratum binaries
# in the current directory
RUNTIME_IMAGE=stratumproject/stratum-np4intel
echo "Building $RUNTIME_IMAGE"
if [ "$LOCAL" == YES ]; then
    docker build -t $RUNTIME_IMAGE \
	         --build-arg JOBS=$JOBS \
             --build-arg NP4_BIN=$NP4_BIN \
             --build-arg BUILDER_IMAGE=$BUILDER_IMAGE \
             -f $DOCKERFILE_DIR/Dockerfile.runtime.local $STRATUM_ROOT

# Else we'll build and generate runtime image
else
    docker build -t $RUNTIME_IMAGE \
	         --build-arg JOBS=$JOBS \
             --build-arg NP4_BIN=$NP4_BIN \
             --build-arg BUILDER_IMAGE=$BUILDER_IMAGE \
             -f $DOCKERFILE_DIR/Dockerfile.runtime $STRATUM_ROOT
fi

ERR=$?
if [ $ERR -ne 0 ]; then
    >&2 echo "ERROR: Error while building $RUNTIME_IMAGE"
    exit $ERR
fi

