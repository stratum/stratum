#!/bin/bash
#
# Copyright 2018-present Open Networking Foundation
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
set -xe
DOCKERFILE_DIR=$(dirname "${BASH_SOURCE[0]}")
STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$DOCKERFILE_DIR/../../../../.." >/dev/null 2>&1 && pwd )"}
JOBS=${JOBS:-4}

print_help() {
cat << EOF

The script builds containerized version of Stratum for Barefoot Tofino based device.
It builds SDE using Dockerfile.builder and saves artifacts to an intermediate builder image.
It also builds the kernel module if kernel header tarball is given.
Then it runs Bazel build for Stratum code base and copies libraries from builder to runtime image using Dockerfile.runtime.

Usage: $0 SDE_TAR [KERNEL_HEADERS_TAR]

Example:
    $0 ~/bf-sde-9.0.0.tgz
    $0 ~/bf-sde-9.0.0.tgz ~/linux-4.14.49-ONL.tar.xz

EOF
}

BUILD_ARGS="--build-arg JOBS=$JOBS"
SDE_TAR=""
KERNEL_HEADERS_TAR=""

if [ "$#" -eq 0 ]; then
    print_help
    exit 1
fi

# Copy tarballs to Stratum root
echo """Copying SDE and header tarballs to $DOCKERFILE_DIR/
NOTE: Copied tarballs will be DELETED after the build"""

if [ -n "$1" ]; then
    SDE_TAR=$(basename $1)
    BUILD_ARGS="$BUILD_ARGS --build-arg SDE_TAR=$SDE_TAR"
    IMG_TAG=${SDE_TAR%.tgz}
    cp -f $1 $DOCKERFILE_DIR
fi
if [ -n "$2" ]; then
    KERNEL_HEADERS_TAR=$(basename $2)
    BUILD_ARGS="$BUILD_ARGS --build-arg KERNEL_HEADERS_TAR=$KERNEL_HEADERS_TAR"
    IMG_TAG=$IMG_TAG-${KERNEL_HEADERS_TAR%.tar.xz}
    cp -f $2 $DOCKERFILE_DIR
fi

BUILDER_IMAGE=stratumproject/stratum-bf-builder:$IMG_TAG
RUNTIME_IMAGE=stratumproject/stratum-bf:$IMG_TAG

# Build base builder image
echo "Building $BUILDER_IMAGE"
docker build -t $BUILDER_IMAGE $BUILD_ARGS \
	 -f $DOCKERFILE_DIR/Dockerfile.builder $DOCKERFILE_DIR

# Remove copied tarballs (and ignore the error)
rm -f $DOCKERFILE_DIR/$SDE_TAR
rm -f $DOCKERFILE_DIR/$KERNEL_HEADERS_TAR

# Build runtime image
echo "Building $RUNTIME_IMAGE"
docker build -t $RUNTIME_IMAGE \
             --build-arg BUILDER_IMAGE=$BUILDER_IMAGE \
             -f $DOCKERFILE_DIR/Dockerfile.runtime $STRATUM_ROOT
