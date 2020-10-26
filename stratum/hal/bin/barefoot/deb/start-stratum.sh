#!/usr/bin/env bash
# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

FLAG_FILE=${FLAG_FILE:-/etc/stratum/stratum.flags}

# Find kernel module if KDRV_PATH is not set
if [ -z "$KDRV_PATH" ]; then
    # First, look for kernel-specific module
    KERNEL_VERSION=$(uname -r)
    if [ -f "/usr/lib/modules/${KERNEL_VERSION}/bf_kdrv.ko" ]; then
        KDRV_PATH="/usr/lib/modules/${KERNEL_VERSION}/bf_kdrv.ko"
    # Next, look for general module
    elif [ -f "/usr/lib/modules/bf_kdrv.ko" ]; then
        KDRV_PATH="/usr/lib/modules/bf_kdrv.ko"
    fi
fi

# Try to load the platform string if not already set.
if [[ -z "$PLATFORM" ]] && [[ -f "/etc/onl/platform" ]]; then
    PLATFORM="$(cat /etc/onl/platform)"
elif [[ -z "$PLATFORM" ]]; then
    echo "PLATFORM variable must be set manually on non-ONL switches."
    exit 255
fi

if [ ! -f "$FLAG_FILE" ]; then
    echo "Cannot find flag file $FLAG_FILE"
    exit 255
fi

# Set up port map for device
PORT_MAP="/etc/stratum/$PLATFORM/port_map.json"
if [ ! -f "$PORT_MAP" ]; then
    echo "Cannot find port map file $PORT_MAP"
    exit 255
fi
ln -f -s "$PORT_MAP" /usr/share/port_map.json

if [ -f "$KDRV_PATH" ]; then
    lsmod | grep 'kdrv' &> /dev/null
    if [[ $? == 0 ]]
    then
        echo "bf_kdrv_mod found! Unloading first..."
        rmmod bf_kdrv
    fi
    echo "loading bf_kdrv_mod..."
    insmod $KDRV_PATH intr_mode="msi" || true
    if [[ $? != 0 ]];then
        echo "Cannot load kernel module, wrong kernel version?"
        exit 255
    fi
else
    echo "Cannot find $KDRV_PATH, skip installing the Kernel module."
fi

mkdir -p /var/run/stratum /var/log/stratum

exec /usr/bin/stratum_bf \
    -chassis_config_file=/etc/stratum/$PLATFORM/chassis_config.pb.txt \
    -log_dir=/var/log/stratum \
    -flagfile=$FLAG_FILE \
    $@
