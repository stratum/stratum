#!/bin/bash
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -ex

load-kernel-modules.sh

mkdir -p /var/run/stratum /var/log/stratum

PLATFORM=$(cat /etc/onl/platform)

exec stratum_bcm \
    -base_bcm_chassis_map_file=/etc/stratum/${PLATFORM}/base_bcm_chassis_map.pb.txt \
    -chassis_config_file=/etc/stratum/${PLATFORM}/chassis_config.pb.txt \
    -bcm_sdk_config_file=/etc/stratum/${PLATFORM}/SDKLT.yml \
    -phal_config_file=/etc/stratum/${PLATFORM}/phal_config.pb.txt \
    -bcm_sdk_checkpoint_dir=/tmp/bcm_chkpt \
    -log_dir=/var/log/stratum \
    -colorlogtostderr \
    -stderrthreshold=0 \
    -v=0 \
    $@
