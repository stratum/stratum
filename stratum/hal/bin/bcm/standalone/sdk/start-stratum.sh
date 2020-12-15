#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -ex

# Remove other SDK kernel modules, if present
rmmod linux_ngknet || true
rmmod linux_ngbde || true

# Reinsert OpenNSA kernel modules
rmmod linux_bcm_knet || true
rmmod linux_user_bde || true
rmmod linux_kernel_bde || true
pushd /usr/lib/stratum/
insmod linux-kernel-bde.ko && insmod linux-user-bde.ko && insmod linux-bcm-knet.ko default_mtu=3000  # kDefaultKnetIntfMtu
popd

# Setup devices and symlinks
mknod /dev/linux-bcm-knet c 122 0 || true
mknod /dev/linux-bcm-net c 123 0 || true
mknod /dev/linux-bcm-diag c 124 0 || true
ln -sf /dev/linux-bcm-diag /dev/linux-bcm-diag-full || true
mknod /dev/linux-uk-proxy c 125 0 || true
mknod /dev/linux-bcm-core c 126 0 || true
mknod /dev/linux-kernel-bde c 127 0 || true
ln -sf  /dev/linux-bcm-core  /dev/linux-user-bde || true
sleep 1

mkdir -p /var/run/stratum /var/log/stratum

PLATFORM=$(cat /etc/onl/platform)

exec stratum_bcm_opennsa \
    -base_bcm_chassis_map_file=/etc/stratum/${PLATFORM}/base_bcm_chassis_map.pb.txt \
    -chassis_config_file=/etc/stratum/${PLATFORM}/chassis_config.pb.txt \
    -bcm_sdk_config_file=/etc/stratum/${PLATFORM}/SDKLT.yml \
    -phal_config_file=/etc/stratum/${PLATFORM}/phal_config.pb.txt \
    -bcm_sdk_checkpoint_dir=/tmp/bcm_chkpt \
    -log_dir=/var/log/stratum \
    -v=0 \
    $@
