#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e

DOCKERFILE_DIR=$( cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd )
STRATUM_BF_DIR=$( cd "$DOCKERFILE_DIR/.." >/dev/null 2>&1 && pwd )

print_help() {
echo "
The script builds containerized version of the Barefoot Tofino model.
Usage: $0 <SDE_TAR>

Example:
    $0 ~/bf-sde-9.3.0.tgz
"
}

if [ -z "$1" ]; then
  # Missing BF SDE tarball
  print_help
  exit 1
fi

DOCKER_EXTRA_RUN_OPTS=""
if [ -t 0 ]; then
  # Running in a TTY, so run interactively (i.e. make Ctrl-C work)
  DOCKER_EXTRA_RUN_OPTS+="-it "
fi

SDE_TAR=$1
SDE_TAR_DIR=$( cd $(dirname "$SDE_TAR") >/dev/null 2>&1 && pwd )
SDE_TAR_NAME=$( basename $SDE_TAR )
SDE_VERSION=$( tar xf $SDE_TAR -O "bf-sde-*/packages/bf-drivers-*.tgz" | \
               tar xf - -O "bf-drivers-*/VERSION" )

if [ "$SDE_TAR_DIR" != "$STRATUM_BF_DIR" ]; then
  echo "Copying SDE to Stratum tree... (will be removed later)"
  cp $SDE_TAR $STRATUM_BF_DIR/
fi

DOCKER_BUILD_OPTS=""
if [ "$(docker version -f '{{.Server.Experimental}}')" = "true" ]; then
  DOCKER_BUILD_OPTS+="--squash "
fi

MODEL_IMAGE=stratumproject/tofino-model:$SDE_VERSION
echo "Building Tofino model runtime image: $RUNTIME_IMAGE"
set -x
docker build \
  $DOCKER_BUILD_OPTS \
  -t "$MODEL_IMAGE" \
  --build-arg SDE_TAR_NAME="$SDE_TAR_NAME" \
  --build-arg SDE_VER="$SDE_VERSION" \
  -f "$DOCKERFILE_DIR/Dockerfile.model" \
  "$STRATUM_BF_DIR"

if [ "$SDE_TAR_DIR" != "$STRATUM_BF_DIR" ]; then
  rm $STRATUM_BF_DIR/$SDE_TAR_NAME
fi

docker save $MODEL_IMAGE | gzip > tofino-model-${SDE_VERSION}-docker.tar.gz
set +x
echo "
Build complete!
  Tofino Model Docker image: $MODEL_IMAGE
  Output directory: $(pwd)
"
