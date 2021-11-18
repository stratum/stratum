#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e

DOCKERFILE_DIR=$( cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd )
STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$DOCKERFILE_DIR/../../../../.." >/dev/null 2>&1 && pwd )"}
STRATUM_BF_DIR=$( cd "$DOCKERFILE_DIR/.." >/dev/null 2>&1 && pwd )
STRATUM_TARGET=${STRATUM_TARGET:-stratum_bfrt}
JOBS=${JOBS:-4}
DOCKER_IMG=${DOCKER_IMG:-stratumproject/build:build}

print_help() {
echo "
The script builds containerized version of Stratum for Barefoot Tofino based device.
It also builds the kernel module if kernel header tarball is given.
Usage: $0 [SDE_TAR [KERNEL_HEADERS_TAR]...]

Example:
    $0 ~/bf-sde-9.3.2.tgz
    $0 ~/bf-sde-9.3.2.tgz ~/linux-4.14.49-ONL.tar.xz
    SDE_INSTALL_TAR=~/bf-sde-9.3.2-install.tgz $0

Additional environment variables:
    SDE_INSTALL_TAR: Tar archive of BF SDE install (set to skip SDE build)
    SDE_INSTALL: Path to BF SDE install directory (set to skip SDE build)
    STRATUM_TARGET: stratum_bf or stratum_bfrt (Default: stratum_bfrt)
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

# Build BF SDE for Stratum (if BF SDE tar is present)
if [ -n "$1" ]; then
  SDE_TAR=$1
  DOCKER_OPTS=""
  CMD_OPTS=""
  SDE_TAR_DIR=$( cd $(dirname "$SDE_TAR") >/dev/null 2>&1 && pwd )
  SDE_TAR_NAME=$( basename $SDE_TAR )
  DOCKER_OPTS+="-v $SDE_TAR_DIR:/bf-tar "
  DOCKER_OPTS+="--env JOBS=$JOBS "
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
  # Mount BSP folder and pass it to the build script, if requested.
  if [[ -n "$BSP" && -d "$BSP" ]]; then
    DOCKER_OPTS+="-v $BSP:/bsp-directory "
    CMD_OPTS+="--bsp-path /bsp-directory "
  elif [[ $BSP =~ ^.*.tgz$ ]]; then
    DOCKER_OPTS+="--mount type=bind,source=$BSP,target=/bsp.tgz "
    CMD_OPTS+="--bsp-path /bsp.tgz "
  fi

  echo "Building BF SDE"
  set -x
  docker run --rm \
    $DOCKER_OPTS \
    $DOCKER_EXTRA_RUN_OPTS \
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
  Docker image for building: $DOCKER_IMG
  Release build enabled: ${RELEASE_BUILD:-false}
  Bazel cache: ${BAZEL_CACHE:-none}
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

# Build Stratum BF in Docker (optimized and stripped)
BAZEL_OPTS=""
if [ -n "$RELEASE_BUILD" ]; then
  BAZEL_OPTS+="--config release "
fi

# Build with Bazel cache
if [ -n "$BAZEL_CACHE" ]; then
  DOCKER_OPTS+="-v $BAZEL_CACHE:/home/$USER/.cache "
  DOCKER_OPTS+="--user $USER "
fi

# Build Stratum BF in Docker
set -x
docker run --rm \
  $DOCKER_OPTS \
  $DOCKER_EXTRA_RUN_OPTS \
  -v $STRATUM_ROOT:/stratum \
  -v $(pwd):/output \
  -w /stratum \
  --entrypoint bash \
  $DOCKER_IMG -c \
    "bazel build //stratum/hal/bin/barefoot:${STRATUM_TARGET}_deb \
       $BAZEL_OPTS \
       --jobs $JOBS && \
     cp -f /stratum/bazel-bin/stratum/hal/bin/barefoot/${STRATUM_TARGET}_deb.deb /output/ && \
     cp -f \$(readlink -f /stratum/bazel-bin/stratum/hal/bin/barefoot/${STRATUM_TARGET}_deb.deb) /output/"
set +x


DOCKER_BUILD_OPTS=""
if [ "$(docker version -f '{{.Server.Experimental}}')" = "true" ]; then
  DOCKER_BUILD_OPTS+="--squash "
fi

DOCKER_BUILD_OPTS+="--label stratum-target=$STRATUM_TARGET "
DOCKER_BUILD_OPTS+="--label bf-sde-version=$SDE_VERSION "
DOCKER_BUILD_OPTS+="--label build-timestamp=$(date +%FT%T%z) "
DOCKER_BUILD_OPTS+="--label build-machine=$(hostname) "

# Add VCS labels
pushd $STRATUM_ROOT
if [ -d .git ]; then
  GIT_URL=${GIT_URL:-$(git config --get remote.origin.url)}
  GIT_REF=$(git describe --tags --no-match --always --abbrev=40 --dirty | sed -E 's/^.*-g([0-9a-f]{40}-?.*)$/\1/')
  GIT_SHA=$(git describe --tags --match XXXXXXX --always --abbrev=40 --dirty)
  DOCKER_BUILD_OPTS+="--label org.opencontainers.image.source=$GIT_URL "
  DOCKER_BUILD_OPTS+="--label org.opencontainers.image.version=$GIT_REF "
  DOCKER_BUILD_OPTS+="--label org.opencontainers.image.revision=$GIT_SHA "
fi
popd

# Build Stratum BF runtime Docker image
STRATUM_NAME=$(echo $STRATUM_TARGET | sed 's/_/-/')
RUNTIME_IMAGE=stratumproject/$STRATUM_NAME:$SDE_VERSION
echo "Building Stratum runtime image: $RUNTIME_IMAGE"
set -x
docker build \
  $DOCKER_BUILD_OPTS \
  -t "$RUNTIME_IMAGE" \
  --build-arg STRATUM_TARGET="$STRATUM_TARGET" \
  -f "$DOCKERFILE_DIR/Dockerfile" \
  "$(pwd)"

docker save $RUNTIME_IMAGE | gzip > ${STRATUM_NAME}-${SDE_VERSION}-docker.tar.gz

set +x
echo "
Build complete!
  Stratum Docker image: $RUNTIME_IMAGE
  Output directory: $(pwd)
"
