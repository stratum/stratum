<!--
Copyright 2019 Dell, Inc.
Copyright 2019-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->
# Stratum on a Broadcom SDKLT based switch

The following guide details how to compile the Stratum binary to run on a Broadcom based switch (i.e. like Tomahawk) using the Broadcom SDKLT.

## ONLPv2 operating system on the switch
Stratum requires an ONLPv2 operating system on the switch. ONF maintains a [fork](https://github.com/opennetworkinglab/OpenNetworkLinux) with additional platforms. Follow the [ONL](https://opennetlinux.org/doc-building.html) instructions to setup your device. Here is what your switch should look like:

```bash
# uname -a
Linux as7712 4.14.49-OpenNetworkLinux #4 SMP Tue May 14 20:43:21 UTC 2019 x86_64 GNU/Linux
```

```bash
# cat /etc/os-release
PRETTY_NAME="Debian GNU/Linux 9 (stretch)"
NAME="Debian GNU/Linux"
VERSION_ID="9"
VERSION="9 (stretch)"
VERSION_CODENAME=stretch
ID=debian
HOME_URL="https://www.debian.org/"
SUPPORT_URL="https://www.debian.org/support"
BUG_REPORT_URL="https://bugs.debian.org/"
```

```bash
# cat /etc/onl/SWI
images:ONL-onf-ONLPv2_ONL-OS_<some-date>_AMD64.swi
```
Note the **ONLPv2**!

```bash
# cat /etc/onl/platform
x86-64-<vendor-name>-<box-name>-32x-r0
```

## Pre-built Docker image

Stratum for Broadcom switches can be run inside Docker on the switch itself.
As part of CI, we publish Stratum with a pre-compiled binary and a set of default configuration files as a [Docker container](https://hub.docker.com/repository/docker/stratumproject/stratum-bcm).

```bash
docker pull stratumproject/stratum-bcm:latest  # to update the image
cd stratum/hal/bin/bcm/standalone/docker
./start-stratum-container.sh
```

## From source

Sometimes you have to build Stratum from source, e.g. because you develop some private feature or want to try a fix not yet pushed to GitHub.

### Build dependencies

Stratum comes with a [development Docker container](https://github.com/stratum/stratum#development-environment) for build purposes. This is the preferred and supported way of building Stratum, as it has all dependencies installed.

If you for some reason want to build natively, here are some pointers to an enviroment that worked for us:

- clang-6.0 or newer

- Linux 4.4.0-161-generic

- Ubuntu 16.04.6 LTS

### Building the `stratum_bcm` package

You can build the same package that we publish manually with the following steps:

```
git clone https://github.com/stratum/stratum.git
cd stratum
./setup_dev_env.sh  # You're now inside the docker container
bazel build //stratum/hal/bin/bcm/standalone:stratum_bcm_package
scp ./bazel-bin/stratum/hal/bin/bcm/standalone/stratum_bcm_package.tar.gz root@<your_switch_ip>:stratum_bcm_package.tar.gz
```

If you're not building inside the docker container, skip the `./setup_dev_env.sh` step.

### SDKLT

**ONLY needed when not using Docker!**

SDKLT requires two Kernel modules to be installed for Packet IO and interfacing with the ASIC. We provide prebuilt binaries for Kernel 4.14.49 in the `stratum_bcm_package.tar.gz` package and the SDKLT [tarball](https://github.com/opennetworkinglab/SDKLT/releases). Install them before running stratum:

```bash
tar xf stratum_bcm_package.tar.gz
# or
wget https://github.com/opennetworkinglab/SDKLT/releases/...
tar xf sdklt-4.14.49.tgz
insmod linux_ngbde.ko && insmod linux_ngknet.ko
```

Check for correct install:

```bash
# lsmod
Module                  Size  Used by
linux_ngknet          352256  0
linux_ngbde            32768  1 linux_ngknet
# dmesg -H
[Jan10 10:53] linux-kernel-bde (6960): MSI not used
[  +2.611898] Broadcom NGBDE loaded successfully
```

### Running the `stratum_bcm` binary

Running `stratum_bcm` requires some configuration files, passed as CLI flags:

- base_bcm_chassis_map_file: Protobuf defining chip capabilities and all possible port configurations of a chassis.
    Example found under: `/stratum/hal/config/**platform name**/base_bcm_chassis_map.pb.txt`
- chassis_config_file: Protobuf setting the config of a specific node.
    Selects a subset of the available port configurations from the chassis map. Determines
    which ports will be available.
    Example found under: `/stratum/hal/config/**platform name**/chassis_config.pb.txt`
- bcm_sdk_config_file: Yaml config passed to the SDKLT. Must match the chassis map.
    Example found under: `/stratum/hal/config/**platform name**/SDKLT.yml`
- bcm_hardware_specs_file: ACL and UDF properties of chips. Found under: `/stratum/hal/config/bcm_hardware_specs.pb.txt`
- bcm_serdes_db_proto_file: Contains SerDes configuration. Not implemented yet, can be an empty file.

We provide defaults for most platforms under `stratum/hal/config`. If you followed the build instructions, these should be on the switch under `/etc/stratum/$PLATFORM`.
Depending on your actual cabling, you'll have to adjust the config files. Panel ports 31 & 32 are in loopback mode and should work without cables.

To start Stratum, you can use the convenience script we package in:

```bash
cd <extracted package>
./start-stratum.sh
```

You should see the ports coming up and have a SDKLT shell prompt:
```
I0628 18:29:10.806623  7930 bcm_chassis_manager.cc:1738] State of SingletonPort (node_id: 1, port_id: 34, slot: 1, port: 3, unit: 0, logical_port: 34, speed: 40G): UP
BCMLT.0>
```


## Troubleshooting

### `insmod: ERROR: could not insert module linux_ngbde.ko: Invalid module format`

You are trying to insert Kernel modules build for a different Kernel version. Make sure your switch looks exactly like described under Runtime dependencies.


### [OpenNSA] No traffic on ports with partial ChassisConfig

When using a Chassis config that does not contain all possible ports of the
switch, traffic might not be received or sent on a subset or all of the ports.
This is because of a quirk in the OpenNSA SDK and its config file, which we
generate from the chassis config. It seems that only initializing some ports
of a port quad (i.e. 3 and 4, but not 1 and 2, using logical port numbers)
leads to this behaviour. Easiest fix is to use the full file and set unwanted
ports administratively to down state.
