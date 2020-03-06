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

DOCKERFILE_DIR=$(dirname "${BASH_SOURCE[0]}")
STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$DOCKERFILE_DIR/../../../../.." >/dev/null 2>&1 && pwd )"}
JOBS=${JOBS:-4}

print_help() {
cat << EOF

The script builds containerized version of Stratum for Barefoot Tofino based device. It builds SDE and kernel modules using Dockerfile.builder and saves artifacts to an intermediate builder image. Then it runs bazel build for Stratum code base and copies libraries from builder to runtime image using Dockerfile.runtime.

Usage: $0 SDE_TAR KERNEL_HEADERS_TAR

Example:
    $0 ~/bf-sde-9.0.0.tgz ~/linux-4.14.49-ONL.tar.gz

EOF
}

if [ "$#" -ne 2 ]; then
    print_help
    exit 1
fi

# Copy tarballs to Stratum root
echo """Copying SDE and header tarballs to $DOCKERFILE_DIR/
NOTE: Copied tarballs will be DELETED after the build"""
cp -i $1 $DOCKERFILE_DIR
cp -i $2 $DOCKERFILE_DIR
SDE_TAR=$(basename $1)
KERNEL_HEADERS_TAR=$(basename $2)

# Build SDE and kernel modules
BUILDER_IMAGE=stratumproject/stratum-bf-builder:${SDE_TAR%.tgz}-${KERNEL_HEADERS_TAR%.tar.xz}
echo "Building $BUILDER_IMAGE"
docker build -t $BUILDER_IMAGE \
	 --build-arg JOBS=$JOBS \
	 --build-arg SDE_TAR=$SDE_TAR \
	 --build-arg KERNEL_HEADERS_TAR=$KERNEL_HEADERS_TAR \
	 -f $DOCKERFILE_DIR/Dockerfile.builder $DOCKERFILE_DIR

# Remove copied tarballs
rm $DOCKERFILE_DIR/$SDE_TAR $DOCKERFILE_DIR/$KERNEL_HEADERS_TAR

# Run Bazel build and generate runtime image
RUNTIME_IMAGE=stratumproject/stratum-bf:${SDE_TAR%.tgz}-${KERNEL_HEADERS_TAR%.tar.xz}
echo "Building $RUNTIME_IMAGE"
docker build -t $RUNTIME_IMAGE \
             --build-arg BUILDER_IMAGE=$BUILDER_IMAGE \
             -f $DOCKERFILE_DIR/Dockerfile.runtime $STRATUM_ROOT
