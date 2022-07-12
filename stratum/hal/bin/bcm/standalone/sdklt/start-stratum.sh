#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -ex

# Remove other SDK kernel modules, if present
rmmod linux_bcm_knet || true
rmmod linux_user_bde || true
rmmod linux_kernel_bde || true

# Reinsert SDKLT kernel modules
rmmod linux_ngknet || true
rmmod linux_ngbde || true
pushd /usr/lib/stratum/
insmod linux_ngbde.ko && insmod linux_ngknet.ko default_mtu=3000  # kDefaultKnetIntfMtu
popd
sleep 1

mkdir -p /var/run/stratum /var/log/stratum

# Try to load the platform string if not already set.
if [[ -z "$PLATFORM" ]] && [[ -f "/etc/onl/platform" ]]; then
    PLATFORM="$(cat /etc/onl/platform)"
elif [[ -z "$PLATFORM" ]] && [[ -f "/etc/sonic/sonic-environment" ]]; then
    PLATFORM=$(source /etc/sonic/sonic-environment; echo "$PLATFORM" | sed 's/_/-/g')
    echo "Stopping SONiC services..."
    systemctl stop sonic.target &> /dev/null || true
elif [[ -z "$PLATFORM" ]]; then
    echo "PLATFORM variable must be set manually on non-ONL switches."
    exit 255
fi

exec stratum_bcm_sdklt \
    -base_bcm_chassis_map_file=/etc/stratum/${PLATFORM}/base_bcm_chassis_map.pb.txt \
    -chassis_config_file=/etc/stratum/${PLATFORM}/chassis_config.pb.txt \
    -bcm_sdk_config_file=/etc/stratum/${PLATFORM}/SDKLT.yml \
    -phal_config_file=/etc/stratum/${PLATFORM}/phal_config.pb.txt \
    -bcm_sdk_checkpoint_dir=/tmp/bcm_chkpt \
    -log_dir=/var/log/stratum \
    -v=0 \
    $@
