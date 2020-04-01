#!/bin/bash

if [ -n "$KERNEL_HEADERS_TAR" ]; then
  mkdir -p /usr/src/kernel-headers && \
  tar xf /stratum/$KERNEL_HEADERS_TAR -C $KERNEL_HEADERS_PATH --strip-components 1 && \
  mkdir -p $SDE_INSTALL/lib/modules && \
  make -C $KERNEL_HEADERS_PATH M=$KDRV_DIR src=$KDRV_DIR modules && \
  mkdir -p /output/usr/local/modules
  mv $KDRV_DIR/bf_kdrv.ko /output/usr/local/modules/
else
  echo "No Kernel headers found, skip"
  exit 0
fi
