#!/bin/bash
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e

echo "Building BF SDE kernel modules"
: ${SDE:?"Barefoot SDE variable is not set"}
echo "SDE: $SDE"
: ${SDE_INSTALL:?"Barefoot SDE_INSTALL is not set"}
echo "SDE_INSTALL: $SDE_INSTALL"

BF_KDRV_DIR=$SDE/pkgsrc/bf-drivers/kdrv/bf_kdrv

function build_kernel_module () {
  echo "Building kernel module for $KERNEL_VERSION"
  DEST_DIR="$SDE_INSTALL/lib/modules/$KERNEL_VERSION/"
  mkdir -p $DEST_DIR
  make -C $KERNEL_HEADERS_PATH M=$BF_KDRV_DIR src=$BF_KDRV_DIR modules
  mv $BF_KDRV_DIR/bf_kdrv.ko $DEST_DIR
}

# No args are supplied, build against local kernel headers
if [ -z "$1" ]; then
  echo "Building using local kernel"
  KERNEL_VERSION=$(uname -r)
  KERNEL_HEADERS_PATH=/usr/src/linux-headers-$KERNEL_VERSION
  if [ -d "$KERNEL_HEADERS_PATH" ]; then
    build_kernel_module
  else
    echo "Local kernel headers could not be found for $KERNEL_VERSION"
    exit 1
  fi
fi

# Build kernel headers from command args
for KERNEL_HEADERS_TAR in "$@"
do
  echo "Building using $KERNEL_HEADERS_TAR"
  KERNEL_HEADERS_PATH="$(mktemp -d /tmp/linux_headers.XXXXXX)"
  tar xf $KERNEL_HEADERS_TAR -C $KERNEL_HEADERS_PATH --strip-components 1
  KERNEL_VERSION="$(awk 'match($0, /CONFIG_BUILD_SALT="([^"]+)"/, a) {print a[1]}' $KERNEL_HEADERS_PATH/.config)"
  if [[ -z $KERNEL_VERSION ]]; then
    KERNEL_VERSION="$(cat $KERNEL_HEADERS_PATH/include/config/kernel.release)"
  fi
  build_kernel_module
  rm -rf $KERNEL_HEADERS_PATH
done
