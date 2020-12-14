#!/usr/bin/env bash
# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

KDRV_PATH=${KDRV_PATH:-/usr/lib/modules/bf_kdrv.ko}
FLAG_FILE=${FLAG_FILE:-/etc/stratum/stratum.flags}

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
# stratum_bf uses around 70 pages in testing, but 128 is a safe bound.
if [[ $nr_hugepages -lt 128 ]] || [[ $free_hugepages -lt 128 ]]; then
    if [[ $nr_hugepages -eq 0 ]]; then
        echo "Setting up hugepages..."
        sysctl -w vm.nr_hugepages=128 || \
        (echo "Failed to set up hugepages."; exit 255)
    else
        echo "ERROR: There are $free_hugepages free hugepages, and 128 are required."
        echo "       The system has $nr_hugepages hugepages allocated in total."
        exit 255
    fi
fi

# Mount hugepages fs if necessary
if ! grep hugetlbfs /etc/mtab | grep -q /mnt/huge; then
    echo "Mounting hugepages..."
    mkdir -p /mnt/huge && \
    mount -t hugetlbfs nodev /mnt/huge || \
    (echo "Failed to mount hugepages."; exit 255)
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

exec /usr/bin/stratum_bf \
    -chassis_config_file=/etc/stratum/$PLATFORM/chassis_config.pb.txt \
    -flagfile=$FLAG_FILE \
    $@
