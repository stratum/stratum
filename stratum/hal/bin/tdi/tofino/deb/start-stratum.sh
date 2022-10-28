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

# Check number of hugepages available (and set up if not configured)
nr_hugepages=$(cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages)
free_hugepages=$(cat /sys/kernel/mm/hugepages/hugepages-2048kB/free_hugepages)
# The number of pages needed is dynamically computed by bf_switchd.
# stratum_tofino uses around 70 pages in testing, but 128 is a safe bound.
if [[ $nr_hugepages -lt 128 ]] || [[ $free_hugepages -lt 128 ]]; then
    if [[ $nr_hugepages -eq 0 ]]; then
        echo "Setting up hugepages..."
        if ! echo 128 > /proc/sys/vm/nr_hugepages; then
            echo "Failed to set up hugepages."
            exit 255
        fi
    else
        echo "ERROR: There are $free_hugepages free hugepages, and 128 are required."
        echo "       The system has $nr_hugepages hugepages allocated in total."
        exit 255
    fi
fi

# Mount hugepages fs if necessary
if ! grep hugetlbfs /etc/mtab | grep -q /mnt/huge; then
    echo "Mounting hugepages..."
    if ! ( mkdir -p /mnt/huge && mount -t hugetlbfs nodev /mnt/huge ); then
        echo "Failed to mount hugepages."
        exit 255
    fi
fi

# Set up port map for device
PORT_MAP="/etc/stratum/$PLATFORM/port_map.json"
if [ ! -f "$PORT_MAP" ]; then
    if [[ "$PLATFORM" != 'barefoot-tofino-model' ]]; then 
        echo "Cannot find port map file $PORT_MAP for $PLATFORM"
        exit 255
    fi
else
    ln -f -s "$PORT_MAP" /usr/share/port_map.json
fi

if [ -f "$KDRV_PATH" ]; then
    lsmod | grep 'kdrv' &> /dev/null
    if [[ $? == 0 ]]
    then
        echo "bf_kdrv_mod found! Unloading first..."
        rmmod bf_kdrv
    fi
    echo "loading bf_kdrv_mod..."
    if ! insmod $KDRV_PATH intr_mode="msi"; then
        echo "Cannot load kernel module, wrong kernel version?"
        exit 255
    fi
else
    echo "Skipping kernel module installation."
fi

mkdir -p /var/run/stratum /var/log/stratum

exec /usr/bin/stratum_tofino \
    -chassis_config_file=/etc/stratum/$PLATFORM/chassis_config.pb.txt \
    -log_dir=/var/log/stratum \
    -flagfile=$FLAG_FILE \
    $@
