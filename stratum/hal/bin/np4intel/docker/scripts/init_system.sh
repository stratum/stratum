#! /usr/bin/bash -

# Copyright 2019-present Dell EMC
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# Example system init script that needs to be run the first time
# after boot to initialise the DPDK modules and ports

# Init the system

# Insert the vfio-pci module
modprobe vfio-pci

# bind N3000 card DPDK ports
/usr/local/sbin/dpdk-devbind -b vfio-pci 0000:88:00.0
#/usr/local/sbin/dpdk-devbind -b vfio-pci 0000:88:00.1
#/usr/local/sbin/dpdk-devbind -b vfio-pci 0000:88:00.0
#/usr/local/sbin/dpdk-devbind -b vfio-pci 0000:88:00.1
#/usr/local/sbin/dpdk-devbind -b vfio-pci 0000:8c:00.0
#/usr/local/sbin/dpdk-devbind -b vfio-pci 0000:8c:00.1
#/usr/local/sbin/dpdk-devbind -b vfio-pci 0000:8c:00.0
#/usr/local/sbin/dpdk-devbind -b vfio-pci 0000:8c:00.1
