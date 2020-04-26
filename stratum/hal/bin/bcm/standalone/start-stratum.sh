#!/bin/bash
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

set -ex

load-kernel-modules.sh

mkdir -p /var/run/stratum /var/log/stratum

PLATFORM=$(cat /etc/onl/platform)

exec stratum_bcm \
    -external_stratum_urls=0.0.0.0:28000 \
    -persistent_config_dir=/etc/stratum \
    -base_bcm_chassis_map_file=/etc/stratum/stratum_configs/${PLATFORM}/base_bcm_chassis_map.pb.txt \
    -chassis_config_file=/etc/stratum/stratum_configs/${PLATFORM}/chassis_config.pb.txt \
    -bcm_sdk_config_file=/etc/stratum/stratum_configs/${PLATFORM}/SDKLT.yml \
    -phal_config_path=/etc/stratum/stratum_configs/${PLATFORM}/phal_config.pb.txt \
    -bcm_serdes_db_proto_file=/etc/stratum/stratum_configs/dummy_serdes_db.pb.txt \
    -bcm_hardware_specs_file=/etc/stratum/stratum_configs/bcm_hardware_specs.pb.txt \
    -forwarding_pipeline_configs_file=/var/run/stratum/pipeline_cfg.pb.txt \
    -write_req_log_file=/var/log/stratum/p4_writes.pb.txt \
    -bcm_sdk_checkpoint_dir=/tmp/bcm_chkpt \
    -log_dir=/var/log/stratum \
    -colorlogtostderr \
    -logtosyslog=false \
    -stderrthreshold=0 \
    -v=0 \
    $@
