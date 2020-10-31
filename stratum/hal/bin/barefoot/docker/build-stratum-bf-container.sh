#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e

DOCKERFILE_DIR=$( cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd )
STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$DOCKERFILE_DIR/../../../../.." >/dev/null 2>&1 && pwd )"}
STRATUM_BF_DIR=$( cd "$DOCKERFILE_DIR/.." >/dev/null 2>&1 && pwd )
STRATUM_TARGET=${STRATUM_TARGET:-stratum_bf}
JOBS=${JOBS:-4}
WITH_ONLP=${WITH_ONLP:-true}
DOCKER_IMG=${DOCKER_IMG:-stratumproject/build:build}

print_help() {
echo "
The script builds containerized version of Stratum for Barefoot Tofino based device.
It builds SDE using Dockerfile.builder and saves artifacts to an intermediate builder image.
It also builds the kernel module if kernel header tarball is given.
Then it runs Bazel build for Stratum code base and copies libraries from builder to runtime image using Dockerfile.runtime.
Usage: $0 [SDE_TAR [KERNEL_HEADERS_TAR]...]

Example:
    $0 ~/bf-sde-9.2.0.tgz
    $0 ~/bf-sde-9.2.0.tgz ~/linux-4.14.49-ONL.tar.xz
    SDE_INSTALL_TAR=~/bf-sde-9.2.0-install.tgz $0

Additional environment variables:
    SDE_INSTALL_TAR: Tar archive of BF SDE install (set to skip SDE build)
    SDE_INSTALL: Path to BF SDE install directory (set to skip SDE build)
    STRATUM_TARGET: stratum_bf or stratum_bfrt (Default: stratum_bf)
    STRATUM_ROOT: The root directory of Stratum.
    JOBS: The number of jobs to run simultaneously while building the base container. (Default: 4)
    WITH_ONLP: Includes ONLP support. (Default: true)
    DOCKER_IMG: Docker image to use for building (Default: stratumproject/build:build)
    RELEASE_BUILD: Optimized build with stripped symbols (Default: false)
"
}

# Build BF SDE for Stratum (if BF SDE tar is present)
if [ -n "$1" ]; then
  SDE_TAR=$1
  DOCKER_OPTS=""
  CMD_OPTS=""
  SDE_TAR_DIR=$( cd $(dirname "$SDE_TAR") >/dev/null 2>&1 && pwd )
  SDE_TAR_NAME=$( basename $SDE_TAR )
  DOCKER_OPTS+="-v $SDE_TAR_DIR:/bf-tar "
  if [ -t 0 ]; then
    # Running in a TTY
    DOCKER_OPTS+="-it "
  fi
  CMD_OPTS+="-t /bf-tar/$SDE_TAR_NAME "
  shift
  i=1
  for KERNEL_HEADERS_TAR in "$@"
  do
      KERNEL_HEADERS_TAR_DIR=$( cd $(dirname "$KERNEL_HEADERS_TAR") >/dev/null 2>&1 && pwd )
      KERNEL_HEADERS_TAR_NAME=$( basename $KERNEL_HEADERS_TAR )
      DOCKER_OPTS+="-v $KERNEL_HEADERS_TAR_DIR:/kernel-tar$i "
      CMD_OPTS+="-k /kernel-tar$i/$KERNEL_HEADERS_TAR_NAME "
      ((i+=1))
  done
  echo "Building BF SDE"
  set -x
  docker run --rm \
    $DOCKER_OPTS \
    -v $STRATUM_BF_DIR:/stratum-bf \
    -w /stratum-bf \
    --entrypoint bash \
    $DOCKER_IMG -c "./build-bf-sde.sh $CMD_OPTS"
  SDE_INSTALL_TAR="${SDE_TAR%.tgz}-install.tgz"
  set +x
fi

echo "
Build variables:
  BF SDE tar: ${SDE_TAR:-none}
  Kernel headers directories: ${@:-none}
  SDE install tar: ${SDE_INSTALL_TAR:-none}
  SDE install directory: ${SDE_INSTALL:-none}
  Stratum directory: $STRATUM_ROOT
  Stratum target: $STRATUM_TARGET
  Build jobs: $JOBS
  Enable ONLP: $WITH_ONLP
  Docker image for building: $DOCKER_IMG
  Release build enabled: ${RELEASE_BUILD:-false}
"

# Set build options for Stratum build
DOCKER_OPTS=""
if [ -n "$SDE_INSTALL_TAR" ]; then
  SDE_VERSION=$(tar xf $SDE_INSTALL_TAR -O ./share/VERSION)
  SDE_INSTALL_TAR_DIR=$( cd $(dirname "$SDE_INSTALL_TAR") >/dev/null 2>&1 && pwd )
  SDE_INSTALL_TAR_NAME=$( basename $SDE_INSTALL_TAR )
  DOCKER_OPTS+="-v $SDE_INSTALL_TAR_DIR:/bf-tar "
  DOCKER_OPTS+="-e SDE_INSTALL_TAR=/bf-tar/$SDE_INSTALL_TAR_NAME "
elif [ -n "$SDE_INSTALL" ]; then
  SDE_VERSION=$(cat $SDE_INSTALL/share/VERSION)
  DOCKER_OPTS+="-v $SDE_INSTALL:/sde-install "
  DOCKER_OPTS+="-e SDE_INSTALL=/sde-install "
else
  echo "Error: SDE_INSTALL_TAR or SDE_INSTALL is not set";
  print_help
  exit 1
fi
if [ -t 0 ]; then
  # Running in a TTY
  DOCKER_OPTS+="-it "
fi

# Build Stratum BF in Docker (optimized and stripped)
EXTRA_BUILD_OPTS=""
if [ -n "$RELEASE_BUILD" ]; then
  # Build with optimization enabled (-O2) and with assert() calls disabled (-DNDEBUG)
  EXTRA_BUILD_OPTS+="--compilation_mode=opt "
  # Strip all symbols
  EXTRA_BUILD_OPTS+="--linkopt=-Wl,--strip-all "
fi
set -x
docker run --rm \
  $DOCKER_OPTS \
  -v $STRATUM_ROOT:/stratum \
  -v $(pwd):/output \
  -w /stratum \
  --entrypoint bash \
  $DOCKER_IMG -c \
    "bazel build //stratum/hal/bin/barefoot:${STRATUM_TARGET}_deb \
       $EXTRA_BUILD_OPTS \
       --define sde_ver=$SDE_VERSION \
       --define phal_with_onlp=$WITH_ONLP \
       --jobs $JOBS && \
     cp -f /stratum/bazel-bin/stratum/hal/bin/barefoot/${STRATUM_TARGET}_deb.deb /output/ && \
     cp -f \$(readlink -f /stratum/bazel-bin/stratum/hal/bin/barefoot/${STRATUM_TARGET}_deb.deb) /output/"
set +x


DOCKER_OPTS=""
if [ "$(docker version | grep Experimental | grep true | wc -l)" -eq "2" ]; then
  DOCKER_OPTS+="--squash "
fi

# Build Stratum BF runtime Docker image
STRATUM_NAME=$(echo $STRATUM_TARGET | sed 's/_/-/')
RUNTIME_IMAGE=stratumproject/$STRATUM_NAME:$SDE_VERSION
echo "Building Stratum runtime image: $RUNTIME_IMAGE"
set -x
docker build \
  $DOCKER_OPTS \
  -t "$RUNTIME_IMAGE" \
  --build-arg STRATUM_TARGET="$STRATUM_TARGET" \
  -f "$DOCKERFILE_DIR/Dockerfile" \
  "$(pwd)"

docker save $RUNTIME_IMAGE -o ${STRATUM_NAME}-${SDE_VERSION}-docker.tar
gzip ${STRATUM_NAME}-${SDE_VERSION}-docker.tar

set +x
echo "
Build complete!
  Stratum Docker image: $RUNTIME_IMAGE
  Output directory: $(pwd)
"
