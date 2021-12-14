#!/bin/bash
# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -ex

# Check number of hugepages available (and set up if not configured).
nr_hugepages=$(cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages)
free_hugepages=$(cat /sys/kernel/mm/hugepages/hugepages-2048kB/free_hugepages)
# 512 per NUMA node.
if [[ $nr_hugepages -lt 512 ]] || [[ $free_hugepages -lt 512 ]]; then
    if [[ $nr_hugepages -eq 0 ]]; then
        echo "Setting up hugepages..."
        if ! echo 512 > /proc/sys/vm/nr_hugepages; then
            echo "Failed to set up hugepages."
            exit 255
        fi
    else
        echo "ERROR: There are $free_hugepages free hugepages, and 512 are required."
        echo "       The system has $nr_hugepages hugepages allocated in total."
        echo "       This could mean there is another Stratum instance running already,"
        echo "       or some other application is using hugepages, too."
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


mkdir -p /var/run/stratum /var/log/stratum

PLATFORM="x86-64-dpdk-swx-r0"

exec dpdk_pipeline -l 0-4 --socket-mem 512,512 --huge-unlink -- \
    -s /etc/stratum/l2fwd.cli \
    $@
