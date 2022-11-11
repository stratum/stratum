#!/usr/bin/env bash
# Copyright 2022-present Intel Corporation
# SPDX-License-Identifier: Apache-2.0

# Check number of hugepages available (and set up if not configured).
nr_hugepages=$(cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages)
free_hugepages=$(cat /sys/kernel/mm/hugepages/hugepages-2048kB/free_hugepages)
# stratum_dpdk uses around 6 pages in testing, but 256 total is a safe bound
# when running multiple instances in parallel.
required_hugepages=16

if [[ $nr_hugepages -lt 256 ]] || [[ $free_hugepages -lt $required_hugepages ]]; then
    if [[ $nr_hugepages -eq 0 ]]; then
        echo "Setting up hugepages..."
        if ! echo 256 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages; then
            echo "Failed to set up hugepages."
            exit 255
        fi
    else
        echo "ERROR: There are $free_hugepages free hugepages, and $required_hugepages are required."
        echo "       The system has $nr_hugepages hugepages allocated in total."
        echo "       This could mean there is another Stratum instance running already,"
        echo "       or some other application is using hugepages, too."
        exit 255
    fi
fi

# Mount hugepages fs if necessary
if ! grep hugetlbfs /etc/mtab | grep -q /mnt/huge; then
    echo "Mounting hugepages..."
    if ! ( mkdir -p /mnt/huge && mount -t hugetlbfs -o pagesize=2M nodev /mnt/huge ); then
        echo "Failed to mount hugepages."
        exit 255
    fi
fi

PLATFORM="p4-dpdk-software-switch"
DPDK_SWITCHD_CFG=/usr/share/stratum/dpdk_skip_p4.conf

mkdir -p /var/run/stratum /var/log/stratum

exec /usr/bin/stratum_dpdk \
    -chassis_config_file=/etc/stratum/$PLATFORM/chassis_config.pb.txt \
    -log_dir=/var/log/stratum \
    -dpdk_switchd_cfg=$DPDK_SWITCHD_CFG \
    $@
