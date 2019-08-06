#!/usr/bin/env bash

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

