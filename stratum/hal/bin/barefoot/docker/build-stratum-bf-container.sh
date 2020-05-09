#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -xe
DOCKERFILE_DIR=$(dirname "${BASH_SOURCE[0]}")
STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$DOCKERFILE_DIR/../../../../.." >/dev/null 2>&1 && pwd )"}
JOBS=${JOBS:-4}
WITH_ONLP=${WITH_ONLP:-true}

print_help() {
echo "
The script builds containerized version of Stratum for Barefoot Tofino based device.
It builds SDE using Dockerfile.builder and saves artifacts to an intermediate builder image.
It also builds the kernel module if kernel header tarball is given.
Then it runs Bazel build for Stratum code base and copies libraries from builder to runtime image using Dockerfile.runtime.

Usage: $0 SDE_TAR [KERNEL_HEADERS_TAR]

Example:
    $0 ~/bf-sde-9.0.0.tgz
    $0 ~/bf-sde-9.0.0.tgz ~/linux-4.14.49-ONL.tar.xz

Additional environment variables:

STRATUM_ROOT: The root directory of Stratum.
JOBS: The number of jobs to run simultaneously while building the base container. (Default: 4)
WITH_ONLP: Includes ONLP support. (Default: true)
"
}

SDE_TAR=""
KERNEL_HEADERS_TAR=""
RUNTIME_IMG_TAG=""

if [ "$#" -eq 0 ]; then
    print_help
    exit 1
fi

# Copy tarballs to Stratum root
cat << EOF
Copying SDE and Kernel header tarballs to $DOCKERFILE_DIR/
NOTE: Copied tarballs will be DELETED after the build
EOF

if [ -n "$1" ]; then
    SDE_TAR=$(basename $1)
    IMG_TAG=${SDE_TAR%.tgz}
    RUNTIME_IMG_TAG="$IMG_TAG"
    cp -f "$1" "$DOCKERFILE_DIR"
fi
if [ -n "$2" ]; then
    KERNEL_HEADERS_TAR=$(basename $2)
    RUNTIME_IMG_TAG="$IMG_TAG-${KERNEL_HEADERS_TAR%.tar.xz}"
    cp -f "$2" "$STRATUM_ROOT"
fi

BUILDER_IMAGE=stratumproject/stratum-bf-builder:$IMG_TAG
RUNTIME_IMAGE=stratumproject/stratum-bf:$RUNTIME_IMG_TAG

# Build base builder image
echo "Building $BUILDER_IMAGE"
docker build -t "$BUILDER_IMAGE" \
     --build-arg JOBS="$JOBS" \
     --build-arg SDE_TAR="$SDE_TAR" \
     -f "$DOCKERFILE_DIR/Dockerfile.builder" "$DOCKERFILE_DIR"

# Build runtime image
echo "Building $RUNTIME_IMAGE"
docker build -t "$RUNTIME_IMAGE" \
    --build-arg BUILDER_IMAGE="$BUILDER_IMAGE" \
    --build-arg KERNEL_HEADERS_TAR="$KERNEL_HEADERS_TAR" \
    --build-arg WITH_ONLP="$WITH_ONLP" \
    -f "$DOCKERFILE_DIR/Dockerfile.runtime" "$STRATUM_ROOT"

# Remove copied tarballs
if [ -f "$DOCKERFILE_DIR/$SDE_TAR" ]; then
    rm -f "$DOCKERFILE_DIR/$SDE_TAR"
fi
if [ -f "$DOCKERFILE_DIR/$KERNEL_HEADERS_TAR" ]; then
    rm -f "$DOCKERFILE_DIR/$KERNEL_HEADERS_TAR"
fi
