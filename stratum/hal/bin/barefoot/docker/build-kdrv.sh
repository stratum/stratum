#!/bin/bash
# Copyright 2019-present Open Networking Foundation
#
# SPDX-License-Identifier: Apache-2.0

KERNEL_HEADERS_PATH=${KERNEL_HEADERS_PATH:-/usr/src/kernel-headers}
KDRV_DIR=${KDRV_DIR:-/bf-sde-install/src/kdrv/bf_kdrv}

if [ -n "$KERNEL_HEADERS_TAR" ]; then
  mkdir -p "$KERNEL_HEADERS_PATH"
  tar xf /stratum/$KERNEL_HEADERS_TAR -C $KERNEL_HEADERS_PATH --strip-components 1
  mkdir -p "$SDE_INSTALL/lib/modules"
  KERNEL_VERSION="$(cat $KERNEL_HEADERS_PATH/include/config/kernel.release)"
  echo "Building kernel module for $KERNEL_VERSION"
  make -C $KERNEL_HEADERS_PATH M=$KDRV_DIR src=$KDRV_DIR modules
  mv $KDRV_DIR/bf_kdrv.ko $SDE_INSTALL/lib/modules/
else
  echo "No Kernel headers found, skip"
  exit 0
fi
