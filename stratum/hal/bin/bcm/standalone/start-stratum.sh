#!/bin/bash
#
# Copyright 2018-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

set -ex

# Remove other SDK kernel modules, if present
rmmod linux_bcm_knet || true
rmmod linux_user_bde || true
rmmod linux_kernel_bde || true

# Reinsert SDKLT kernel modules
rmmod linux_ngknet || true
rmmod linux_ngbde || true
insmod linux_ngbde.ko && insmod linux_ngknet.ko
sleep 1

PLATFORM=$(cat /etc/onl/platform)

./stratum_bcm \
    -external_stratum_urls=0.0.0.0:28000 \
    -persistent_config_dir=/tmp/stratum \
    -base_bcm_chassis_map_file=./stratum_configs/${PLATFORM}/base_bcm_chassis_map.pb.txt \
    -chassis_config_file=./stratum_configs/${PLATFORM}/chassis_config.pb.txt \
    -bcm_sdk_config_file=./stratum_configs/${PLATFORM}/SDKLT.yml \
    -phal_config_path=./stratum_configs/${PLATFORM}/phal_config.pb.txt \
    -bcm_serdes_db_proto_file=./stratum_configs/dummy_serdes_db.pb.txt \
    -bcm_hardware_specs_file=./stratum_configs/bcm_hardware_specs.pb.txt \
    -forwarding_pipeline_configs_file=/tmp/stratum/pipeline_cfg.pb.txt \
    -write_req_log_file=/tmp/stratum/p4_writes.pb.txt \
    -bcm_sdk_checkpoint_dir=/tmp/bcm_chkpt \
    -colorlogtostderr \
    -logtosyslog=false \
    -stderrthreshold=0 \
    -v=0
