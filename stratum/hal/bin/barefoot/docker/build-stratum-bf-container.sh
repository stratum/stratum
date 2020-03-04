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

STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$( dirname "${BASH_SOURCE[0]}" )/../../../../.." >/dev/null 2>&1 && pwd )"}
JOBS=${JOBS:-4}
KEEP_INTERMEDIATE_IMAGE=NO

print_help() {
cat << EOF
The script builds containerized version of Stratum for Barefoot Tofino based device. Barefoot SDE and Linux headers tarball need to be placed under Stratum root directory before running the script. By default it builds SDE and kernel modules using Dockerfile.builder and saves artifacts to an intermediate builder image unless a builder image is specified using -b option. Then it runs bazel build for Stratum code base and copies libraries from builder to runtime image using Dockerfile.runtime. By default the intermediate image gets removed.

Usage: $0
    options:
    [-s <sde-tar>]		Barefoot SDE tarball. Required when builder image is not specified
    [-k <kernel-headers-tar>]	Linux kernel headers tarball. Required when builder image is not specified
    [-b <builder-image>]	Specify a builder image which includes artifacts from building Barefoot SDE and Linux kernel modules
    [-i]			Keep intermediate builder image which could be used to speed up future builds (see -b)

Example:
    $0 -s bf-sde-9.0.0.tgz -k linux-4.14.49-ONL.tar.gz
    $0 -b stratumproject/stratum-bf-builder:bf-sde-9.0.0.tgz-linux-4.14.49-ONL.tar.gz
EOF
}

while [[ $# -gt 0 ]]
do
    key="$1"
    case $key in
        -h|--help)
        print_help
        exit 0
        ;;
    -s)
        SDE_TAR="$2"
        shift 2
        ;;
    -k)
        KERNEL_HEADERS_TAR="$2"
        shift 2
        ;;
    -b)
        BUILDER_IMAGE="$2"
        shift 2
        ;;
    -i)
        KEEP_INTERMEDIATE_IMAGE=YES
        shift
        ;;
    *)  # unknown option
        print_help
        exit 1
        ;;
    esac
done

if [[ -z $BUILDER_IMAGE ]]; then
    # No builder image specified, need to build it first
    if [[ -z $SDE_TAR || -z $KERNEL_HEADERS_TAR ]]; then
        print_help
        exit 1
    fi
    BUILDER_IMAGE=stratumproject/stratum-bf-builder:$SDE_TAR-$KERNEL_HEADERS_TAR
    echo "Building $BUILDER_IMAGE"
    docker build -t $BUILDER_IMAGE \
                 --build-arg JOBS=$JOBS \
                 --build-arg SDE_TAR=$SDE_TAR \
                 --build-arg KERNEL_HEADERS_TAR=$KERNEL_HEADERS_TAR \
                 -f $STRATUM_ROOT/stratum/hal/bin/barefoot/docker/Dockerfile.builder $STRATUM_ROOT
else
    # Don't remove builder image when it's specified by user
    KEEP_INTERMEDIATE_IMAGE=YES
fi

# Run Bazel build and generate runtime image
RUNTIME_IMAGE=stratumproject/stratum-bf:$(echo $BUILDER_IMAGE | cut -d':' -f2)
echo "Building $RUNTIME_IMAGE"
docker build -t $RUNTIME_IMAGE \
             --build-arg BUILDER_IMAGE=$BUILDER_IMAGE \
             -f $STRATUM_ROOT/stratum/hal/bin/barefoot/docker/Dockerfile.runtime $STRATUM_ROOT

# Remove builder image
if [ "$KEEP_INTERMEDIATE_IMAGE" == NO ]; then
    docker rmi $BUILDER_IMAGE
fi
