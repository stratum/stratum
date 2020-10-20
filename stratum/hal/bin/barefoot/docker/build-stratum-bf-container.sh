#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -xe
DOCKERFILE_DIR=$( cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd )
STRATUM_ROOT=${STRATUM_ROOT:-"$( cd "$DOCKERFILE_DIR/../../../../.." >/dev/null 2>&1 && pwd )"}
STRATUM_TARGET=${STRATUM_TARGET:-stratum_bf}
JOBS=${JOBS:-4}
WITH_ONLP=${WITH_ONLP:-true}

print_help() {
echo "
The script builds containerized version of Stratum for Barefoot Tofino based device.
It builds SDE using Dockerfile.builder and saves artifacts to an intermediate builder image.
It also builds the kernel module if kernel header tarball is given.
Then it runs Bazel build for Stratum code base and copies libraries from builder to runtime image using Dockerfile.runtime.

Usage: $0 SDE_TAR [KERNEL_HEADERS_TAR]

FIXME(bocon): update usage to include named parameters

Example:
    $0 ~/bf-sde-9.0.0.tgz
    $0 ~/bf-sde-9.0.0.tgz ~/linux-4.14.49-ONL.tar.xz

Additional environment variables:

STRATUM_ROOT: The root directory of Stratum.
STRATUM_TARGET: stratum_bf or stratum_bfrt
JOBS: The number of jobs to run simultaneously while building the base container. (Default: 4)
WITH_ONLP: Includes ONLP support. (Default: true)
"
}

PARAMS=""
while (( "$#" )); do
  case "$1" in
    -s|--bf-sde-tar)
      if [ -n "$2" ] && [ ${2:0:1} != "-" ]; then
        SDE_TAR=$2
        shift 2
      else
        echo "Error: Argument for $1 is missing" >&2
        exit 1
      fi
      ;;
    -i|--bf-sde-install-tar)
      if [ -n "$2" ] && [ ${2:0:1} != "-" ]; then
        SDE_INSTALL_TAR=$2
        shift 2
      else
        echo "Error: Argument for $1 is missing" >&2
        exit 1
      fi
      ;;
    -k|--kernel-headers)
      if [ -n "$2" ] && [ ${2:0:1} != "-" ]; then
        KERNEL_HEADERS_TAR=$2
        shift 2
      else
        echo "Error: Argument for $1 is missing" >&2
        exit 1
      fi
      ;;
    -o|--sde-build-only)
      SDE_BUILD_ONLY=1
      shift
      ;;
    -j|--jobs)
      if [ -n "$2" ] && [ ${2:0:1} != "-" ]; then
        JOBS=$2
        shift 2
      else
        echo "Error: Argument for $1 is missing" >&2
        exit 1
      fi
      ;;
    -v|--sde-version)
      if [ -n "$2" ] && [ ${2:0:1} != "-" ]; then
        SDE_VERSION=$2
        shift 2
      else
        echo "Error: Argument for $1 is missing" >&2
        exit 1
      fi
      ;;
    -h|--help)
      print_help
      exit 0
      ;;
    -*|--*=) # unsupported flags
      echo "Error: Unsupported flag $1" >&2
      print_help
      exit 1
      ;;
    *) # preserve positional arguments
      PARAMS="$PARAMS $1"
      shift
      ;;
  esac
done

# Support for legacy positional argument usage
eval set -- "$PARAMS"
if [ -n "$1" ]; then
    SDE_TAR=$1
fi
if [ -n "$2" ]; then
    KERNEL_HEADERS_TAR=$2
fi

# ---------- Validate params ----------

# Get tar file names
SDE_TAR_NAME=$(basename "$SDE_TAR")
SDE_INSTALL_TAR_NAME=$(basename "$SDE_INSTALL_TAR")
KERNEL_HEADERS_TAR_NAME=$(basename "$KERNEL_HEADERS_TAR")

# Infer version from BF SDE tar filename, if not provided
SDE_TAR_VERSION=${SDE_TAR_NAME%.tgz}
SDE_VERSION=${SDE_VERSION:-$SDE_TAR_VERSION}

# Build BF SDE install tar filename, if not provided
SDE_INSTALL_TAR_NAME=${SDE_INSTALL_TAR_NAME:-"${SDE_TAR_VERSION}-${STRATUM_TARGET}-install.tgz"}
SDE_INSTALL_TAR=${SDE_INSTALL_TAR:-$(dirname $SDE_TAR)/$SDE_INSTALL_TAR_NAME}

if [ -z "$SDE_INSTALL_TAR_NAME" ]; then
  echo "You need to provide a BF SDE archive or a BF SDE install archive"
  exit 1
fi

# Infer version from BF SDE install tar filename, if needed
SDE_INSTALL_TAR_VERSION=${SDE_INSTALL_TAR_NAME%"-${STRATUM_TARGET}-install.tgz"}
SDE_VERSION=${SDE_VERSION:-$SDE_INSTALL_TAR_VERSION}
if [ -z "$SDE_VERSION" ]; then
  echo "BF SDE could not be inferred, so it must be specified"
  exit 1
fi
IMG_TAG=${IMG_TAG:-$SDE_VERSION}

# FIXME(bocon)
#RUNTIME_IMG_TAG="$IMG_TAG-${KERNEL_HEADERS_TAR%.tar.xz}"
#cp -f "$2" "$STRATUM_ROOT"

#FIXME(bocon)
( set -o posix ; set )

# ---------- Build BF-SDE if provided ----------
if [ -n "$SDE_TAR" ]; then
  BUILDER_IMAGE=stratumproject/$STRATUM_TARGET-builder:$IMG_TAG
  cat << EOF
Copying BF SDE tarball to $DOCKERFILE_DIR/
NOTE: Copied tarball will be DELETED after the build
EOF

  cp -f "$SDE_TAR" "$DOCKERFILE_DIR"
  # Build base builder image
  echo "Building $BUILDER_IMAGE"
  docker build -t "$BUILDER_IMAGE" \
    --build-arg JOBS="$JOBS" \
    --build-arg SDE_TAR="$SDE_TAR_NAME" \
    --build-arg SDE_INSTALL_TAR="$SDE_INSTALL_TAR_NAME" \
    --build-arg STRATUM_TARGET="$STRATUM_TARGET" \
    -f "$DOCKERFILE_DIR/Dockerfile.builder" "$DOCKERFILE_DIR"
  # Remove BF SDE tarball
  if [ -f "$DOCKERFILE_DIR/$SDE_TAR" ]; then
    rm -f "$DOCKERFILE_DIR/$SDE_TAR"
  fi

  # Extract Stratum BF SDE install package
  docker run --rm \
    -v $(dirname "$SDE_INSTALL_TAR"):/cp-mnt \
    --entrypoint cp "$BUILDER_IMAGE" \
    /output/"$SDE_INSTALL_TAR_NAME" /cp-mnt/
else
  echo "No bf-sde tar provided, skipping SDE build..."
fi

# ---------- Build Stratum ----------
if [ -z "$SDE_BUILD_ONLY" ]; then
  # Copy tarballs to Stratum root
  cat << EOF
Copying SDE install and kernel header tarballs to $STRATUM_ROOT/
NOTE: Copied tarballs will be DELETED after the build
EOF

  RUNTIME_IMG_TAG="$IMG_TAG"
  cp -f "$SDE_INSTALL_TAR" "$STRATUM_ROOT"
  if [ -f "$KERNEL_HEADERS_TAR" ]; then
    cp -f "$KERNEL_HEADERS_TAR" "$STRATUM_ROOT"
    RUNTIME_IMG_TAG="$IMG_TAG-${KERNEL_HEADERS_TAR_NAME%.tar.xz}"
  fi

  RUNTIME_IMAGE=stratumproject/$STRATUM_TARGET:$RUNTIME_IMG_TAG
  # Build runtime image
  echo "Building $RUNTIME_IMAGE"
  docker build -t "$RUNTIME_IMAGE" \
    --build-arg BUILDER_IMAGE="$BUILDER_IMAGE" \
    --build-arg SDE_INSTALL_TAR_NAME="$SDE_INSTALL_TAR_NAME" \
    --build-arg KERNEL_HEADERS_TAR="$KERNEL_HEADERS_TAR_NAME" \
    --build-arg STRATUM_TARGET="$STRATUM_TARGET" \
    --build-arg WITH_ONLP="$WITH_ONLP" \
    -f "$DOCKERFILE_DIR/Dockerfile.runtime" "$STRATUM_ROOT"

  # Remove copied tarballs
  if [ -f "$STRATUM_ROOT/$KERNEL_HEADERS_TAR_NAME" ]; then
    rm -f "$STRATUM_ROOT/$KERNEL_HEADERS_TAR_NAME"
  fi
  if [ -f "$STRATUM_ROOT/$SDE_INSTALL_TAR_NAME" ]; then
      rm -f "$STRATUM_ROOT/$SDE_INSTALL_TAR_NAME"
  fi
fi
