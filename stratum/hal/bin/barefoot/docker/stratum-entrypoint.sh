#!/usr/bin/env bash
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

WITH_ONLP=${WITH_ONLP:-true}
KDRV_PATH=$BF_SDE_INSTALL/lib/modules/bf_kdrv.ko
FLAG_FILE=/stratum_configs/stratum.flags
PLATFORM=${PLATFORM:-x86-64-accton-wedge100bf-32x-r0}

if [ "$WITH_ONLP" = true ]; then
    PORT_MAP=$BF_SDE_INSTALL/share/stratum/$(cat /etc/onl/platform).json
else
    PORT_MAP="$BF_SDE_INSTALL/share/stratum/$PLATFORM.json"
fi

if [ ! -f "$FLAG_FILE" ]; then
    echo "Use default flag file"
    FLAG_FILE=/usr/local/share/stratum/stratum.flags
fi

if [ ! -f "$PORT_MAP" ]; then
    echo "Cannot find $PORT_MAP"
    exit -1
fi

ln -s $PORT_MAP $BF_SDE_INSTALL/share/port_map.json

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
    fi
else
    echo "Cannot find $KDRV_PATH, skip installing the Kernel module"
fi

if [ "$WITH_ONLP" = true ]; then
    /usr/local/bin/stratum_bf -flagfile=$FLAG_FILE
else
    /usr/local/bin/stratum_bf_no_onlp -flagfile=$FLAG_FILE
fi
