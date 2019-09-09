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

KDRV_PATH=$BF_SDE_INSTALL/lib/modules/bf_kdrv.ko
PORT_MAP=$BF_SDE_INSTALL/share/stratum/$(cat /etc/onl/platform).json
FLAG_FILE=/stratum_configs/stratum.flags

if [ ! -f "$FLAG_FILE" ]; then
    echo "Use default flag file"
    FLAG_FILE=/usr/local/share/stratum/stratum.flags
fi

if [ ! -f "$PORT_MAP" ]; then
    echo "Cannot find $PORT_MAP"
    exit -1
fi

ln -s $PORT_MAP $BF_SDE_INSTALL/share/port_map.json

lsmod | grep 'kdrv' &> /dev/null
if [[ $? == 0 ]]
then
    echo "bf_kdrv_mod found! Unloading first..."
    rmmod bf_kdrv
fi

if [ ! -f "$KDRV_PATH" ]; then
    echo "Cannot find $KDRV_PATH"
    exit -1
fi

echo "loading bf_kdrv_mod..."
insmod $KDRV_PATH intr_mode="msi"

/usr/local/bin/stratum_bf -flagfile=$FLAG_FILE

