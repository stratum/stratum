#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e

DOCKERFILE_DIR=$( cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd )
STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$DOCKERFILE_DIR/../.." >/dev/null 2>&1 && pwd )"}
JOBS=${JOBS:-4}
DOCKER_IMG=${DOCKER_IMG:-stratumproject/build:build}

print_help() {
echo "
The script builds containerized version of Stratum for the BMv2 behavioral model.
Usage: $0
Additional environment variables:
    STRATUM_ROOT: The root directory of Stratum.
    JOBS: The number of jobs to run simultaneously while building the base container. (Default: 4)
    DOCKER_IMG: Docker image to use for building (Default: stratumproject/build:build)
    RELEASE_BUILD: Optimized build with stripped symbols (Default: false)
    BAZEL_CACHE: Path to Bazel cache (Default: <empty>)
"
}

DOCKER_EXTRA_RUN_OPTS=""
if [ -t 0 ]; then
  # Running in a TTY, so run interactively (i.e. make Ctrl-C work)
  DOCKER_EXTRA_RUN_OPTS+="-it "
fi

echo "
Build variables:
  Stratum directory: $STRATUM_ROOT
  Build jobs: $JOBS
  Docker image for building: $DOCKER_IMG
  Release build enabled: ${RELEASE_BUILD:-false}
  Bazel cache: ${BAZEL_CACHE:-none}
"

# Set build options for Stratum build
DOCKER_OPTS=""

# Build Stratum BMv2 in Docker (optimized and stripped)
BAZEL_OPTS=""
if [ -n "$RELEASE_BUILD" ]; then
  BAZEL_OPTS+="--config release "
fi

# Build with Bazel cache
if [ -n "$BAZEL_CACHE" ]; then
  DOCKER_OPTS+="-v $BAZEL_CACHE:/home/$USER/.cache "
  DOCKER_OPTS+="--user $USER "
fi

# Build Stratum BMv2 in Docker
set -x
docker run --rm \
  $DOCKER_OPTS \
  $DOCKER_EXTRA_RUN_OPTS \
  -v $STRATUM_ROOT:/stratum \
  -v $(pwd):/output \
  -w /stratum \
  --entrypoint bash \
  $DOCKER_IMG -c \
    "bazel build //stratum/hal/bin/bmv2:stratum_bmv2_deb.deb \
       $BAZEL_OPTS \
       --jobs $JOBS && \
     cp -f /stratum/bazel-bin/stratum/hal/bin/bmv2/stratum_bmv2_deb.deb /output/"
set +x


DOCKER_BUILD_OPTS=""
if [ "$(docker version -f '{{.Server.Experimental}}')" = "true" ]; then
  DOCKER_BUILD_OPTS+="--squash "
fi

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
STRATUM_NAME=stratum-bmv2
RUNTIME_IMAGE=opennetworking/mn-stratum
echo "Building Stratum runtime image: $RUNTIME_IMAGE"
set -x
docker build \
  $DOCKER_BUILD_OPTS \
  -t "$RUNTIME_IMAGE" \
  -f "$DOCKERFILE_DIR/Dockerfile" \
  "$(pwd)"

docker save $RUNTIME_IMAGE | gzip > ${STRATUM_NAME}-${SDE_VERSION}-docker.tar.gz

set +x
echo "
Build complete!
  Stratum Docker image: $RUNTIME_IMAGE
  Output directory: $(pwd)
"
