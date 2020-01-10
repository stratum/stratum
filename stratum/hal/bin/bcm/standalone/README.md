# Stratum on a Broadcom SDKLT based switch

The following guide details how to compile the Stratum binary to run on a Broadcom based switch (i.e. like Tomahawk) using the Broadcom SDKLT.

## Build dependencies

Stratum comes with a [development Docker container](https://github.com/stratum/stratum#development-environment) for build purposes. This is the preferred and supported way of building Stratum, as it has all dependencies installed.

If you for some reason want to build natively, here are some pointers to an enviroment that worked for us:

- clang-6.0 or newer

- Linux 4.4.0-161-generic

- Ubuntu 16.04.6 LTS

## Building the `stratum_bcm` binary

The `stratum_bcm` binary is a standalone, static executable and is build by the following steps:

```
git clone https://github.com/stratum/stratum.git
cd stratum
./setup_dev_env.sh  # You're now inside the docker container
bazel build //stratum/hal/bin/bcm/standalone:stratum_bcm
scp ./bazel-bin/stratum/hal/bin/bcm/standalone/stratum_bcm root@<your_switch_ip>:stratum_bcm
scp stratum/hal/config root@<your_switch_ip>:stratum_configs
```

If you're not building inside the docker container, skip the `./setup_dev_env.sh` step.

## Runtime dependencies

Currently only a non-virtualized, bare-metal setup is supported. But we're working on a Docker-based solution.

### ONLPv2
Stratum requires an ONLPv2 operating system on the switch. ONF maintains a [fork](https://github.com/opennetworkinglab/OpenNetworkLinux) with additional platforms. Follow the [ONL](https://opennetlinux.org/doc-building.html) instructions to setup your device. Here is what your switch should look like:

```bash
# uname -a
Linux as7712 4.14.49-OpenNetworkLinux #4 SMP Tue May 14 20:43:21 UTC 2019 x86_64 GNU/Linux
```

```bash
# cat /etc/os-release
PRETTY_NAME="Debian GNU/Linux 8 (jessie)"
NAME="Debian GNU/Linux"
VERSION_ID="8"
VERSION="8 (jessie)"
ID=debian
HOME_URL="http://www.debian.org/"
SUPPORT_URL="http://www.debian.org/support"
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

### SDKLT
SDKLT requires two Kernel modules to be installed for Packet IO and interfacing with the ASIC. We provide prebuilt binaries for Kernel 4.14.49 in the release [tarball](https://github.com/opennetworkinglab/SDKLT/releases). Install them before running stratum:

```bash
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

## Running the `stratum_bcm` binary

Running `stratum_bcm` requires five configuration files, passed as CLI flags:

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

We provide defaults for most platforms under `stratum/hal/config`. If you followed the build instructions, these should be on the switch under `stratum_configs`.
Depending on your actual cabling, you'll have to adjust the config files. Panel ports 31 & 32 are in loopback mode and should work without cables.

The config flags are best stored in a flag file `stratum.flags`:

```bash
-external_stratum_urls=0.0.0.0:28000
-persistent_config_dir=/etc/stratum
-base_bcm_chassis_map_file=/etc/stratum/chassis_map.pb.txt
-chassis_config_file=/etc/stratum/chassis_config.pb.txt
-bcm_sdk_config_file=/etc/stratum/sdk_config.yml
-bcm_hardware_specs_file=/etc/stratum/bcm_hardware_specs.pb.txt
-forwarding_pipeline_configs_file=/tmp/stratum/pipeline_cfg.pb.txt
-write_req_log_file=/tmp/stratum/p4_writes.pb.txt
-bcm_serdes_db_proto_file=/etc/stratum/dummy_serdes_db.pb.txt
-bcm_sdk_checkpoint_dir=/tmp/stratum/bcm_chkpt
-colorlogtostderr
-alsologtostderr
-logtosyslog=false
-v=0
```

(You can also use a bash script, passing the flags individually. Prevents [Issue 61](https://github.com/gflags/gflags/issues/61)).

Start stratum:
```bash
./stratum_bcm -flagfile=stratum.flags
```

You should see the ports coming up and have a SDKLT shell prompt:
```
I0628 18:29:10.806623  7930 bcm_chassis_manager.cc:1738] State of SingletonPort (node_id: 1, port_id: 34, slot: 1, port: 3, unit: 0, logical_port: 34, speed: 40G): UP
BCMLT.0>
```


## Troubleshooting

### `insmod: ERROR: could not insert module linux_ngbde.ko: Invalid module format`

You are trying to insert Kernel modules build for a different Kernel version. Make sure your switch looks exactly like described under Runtime dependencies.
