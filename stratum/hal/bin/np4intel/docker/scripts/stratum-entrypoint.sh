#!/usr/bin/env bash

# Copyright 2019-present Dell EMC
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# See if we've got argument
if [ $# -gt 0 ]; then
  key="$1"
  case $key in
    --debug)
      DEBUG=YES
      ;;
  *) # unknown option
    echo "Unknown option"
    exit 1
    ;;
  esac
fi

# Environment vars
STRATUM_CONFIGS=${STRATUM_CONFIGS:-/stratum_configs}
STRATUM_LOGS=${STRATUM_LOGS:-/stratum_logs}
FLAG_FILE=$STRATUM_CONFIGS/stratum.flags
DEVICE_CONFIGS=$STRATUM_CONFIGS/device_configs
PLATFORM=${PLATFORM:-np4-intel-n3000}

# mount the hugepages
mount -t hugetlbfs nodes /mnt/huge

# create forwarding pipeline config
export PYTHONPATH=/py_out
/build_pipeline_configs.py \
     --device-config-dir $DEVICE_CONFIGS \
     --p4info $STRATUM_CONFIGS/p4info_np4.txt \
     --pipeline-config $STRATUM_CONFIGS/pipeline_config.pb.txt
ERR=$?
if [ $ERR -ne 0 ]; then
    >&2 echo "ERROR: Error while building pipeline configs"
    exit $ERR
fi

# Start stratum
if [ "$DEBUG" == YES ]; then
    echo "Running the debug Stratum binary"
    /usr/local/bin/stratum_np4intel.debug -flagfile=$FLAG_FILE
else
    /usr/local/bin/stratum_np4intel -flagfile=$FLAG_FILE
fi
