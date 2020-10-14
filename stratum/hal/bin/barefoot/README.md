<!--
Copyright 2018 Barefoot Networks, Inc.
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# Running Stratum on a Barefoot Tofino based switch

## Quick start

There are two ways to deploy Stratum on a Barefoot Tofino based switch.

### 1. Deploy with Docker

```bash
./stratum/hal/bin/barefoot/docker/start-stratum-container.sh
```

The Docker image contains pre-built Stratum binary, latest Barefoot Software
Development Environment (SDE) libraries, and default configuration files for all
supported platforms.

Building a Docker image yourself and more is covered in the [Docker README](./docker/README.md).

### 2. Build and deploy Stratum Debian package

```bash
apt-get update
apt-get install -y --reinstall ./stratum_bf_deb.deb
start-stratum.sh
```

This package installs all dependencies, configuration files and the `stratum_bf`
[systemd service](#managing-stratum-with-systemd).

In the future we might provide pre-built Debian packages, but for now, you have
to build them yourself as lined out in this document.

## Installing the SDE

Before you can build Stratum, the Barefoot SDE needs to be installed.

Note: To do this, you will need to have a SELA with Intel (Barefoot) to access P4
Studio SDE. Contact Intel for more details.

```bash
tar -xzvf bf-sde-<SDE_VERSION>.tgz
export SDE=`pwd`/bf-sde-<SDE_VERSION>
export SDE_INSTALL=$SDE/install
cd $SDE/p4studio_build
sed -i.bak '/package_dependencies/d; /thrift/d' profiles/stratum_profile.yaml  # For SDE version <= 8.9.x
./p4studio_build.py -up profiles/stratum_profile.yaml [-kdir <path/to/linux/sources>] [--bsp-path $BSP_PATH]
```

Barefoot's P4Studio Build tool comes with a default Stratum profile
(`stratum_profile.yaml`), which takes care of installing all the necessary
dependencies and builds the SDE with the appropriate flags. Feel free to
customize the profile if needed; please refer to the P4Studio Build documentation.
If you are using the
reference BSP provided by Barefoot, you may also use P4Studio Build to
install the BSP (see [below](#board-support-package-bsp-or-onlpv2)).
Also, we drop Thrift support in Stratum, the Stratum profile will
be updated in next version. Now you need to remove the Thrift dependency if you
are using SDE version 8.9.x.

Remember to download and pass the correct Kernel sources (`-kdir`) if you
are building modules for a specific version other than the host's.

As there are some issues with building the SDE on ONL switches, it's better to
do that on a separate server.

### Supported SDE versions

 - 8.9.2
 - 9.0.0
 - 9.1.0
 - 9.2.0

### Board support package (BSP) or ONLPv2?

Stratum can be run on Tofino-based platforms in 2 different modes:

**ONLPv2**

If your platform comes with ONLPv2 and a JSON "port mapping" file is provided
by the platform vendor (see this
[example](../../config/x86-64-accton-wedge100bf-32x-r0/port_map.json) for the
Wedge 100bf-32x), you can use Stratum in "BSP-less mode". Refer to this
[section](#running-the-binary-in-bsp-less-mode) for more information. This is
the recommended mode. No changes to the SDE needed.

**BSP**

Otherwise, you need to build & install the BSP. You will not be able to use
the Stratum ONLP support. The exact instructions vary by the BSP vendor, here is
how it works for the Wedge reference switch. Pass the BSP sources to the p4studio_build
script with the `--bsp-path` flag.

```bash
tar -xzvf bf-reference-bsp-<SDE_VERSION>.tgz
export BSP_PATH=`pwd`/bf-reference-bsp-<SDE_VERSION>
./p4studio_build.py -up profiles/stratum_profile.yaml --bsp-path $BSP_PATH [-kdir <path/to/linux/sources>]
```

## Building Stratum

The [SDE](#installing-the-sde) needs to be installed and set up for this step.

```bash
bazel build //stratum/hal/bin/barefoot:stratum_bf_deb [--define phal_with_onlp=false] [--define sde_ver=9.2.0]
```

We provide a Bazel target that builds the Stratum binary and packages all
necessary configurations into a single Debian package(.deb file). The Debian
package also includes systemd service definition so users can use systemd to
start the Stratum as a system service.

The resulting Debian package can be found here:
`bazel-bin/stratum/hal/bin/barefoot/stratum_bf_deb.deb`

Copy this file over to the switch and follow the [running Stratum](#running-stratum)
instructions.

### Building for a different SDE version

Stratum is designed for the latest Barefoot SDE. You can specify a version by
using the `--define sde_ver=<SDE version>` flag if you need to build Stratum
against an older version (e.g. 8.9.2) and set up the SDE [accordingly](#installing-the-sde).

### Disabling ONLPv2 support

If you're using a vendor-provided BSP or running Stratum with the Tofino
software model, ONLP needs to be disabled. The `--define phal_with_onlp=false`
flag tells Bazel not to build with the ONLP Phal implementation.

## Running Stratum

Install the package built in the previous step and start Stratum:

```bash
apt-get update
apt-get install -y --reinstall ./stratum_bf_deb.deb
start-stratum.sh
```

You can safely ignore warnings like this:
`N: Download is performed unsandboxed as root as file '/root/stratum_bf_deb.deb' couldn't be accessed by user '_apt'. - pkgAcquire::Run (13: Permission denied)`

Stratum picks sane defaults for most platforms, but should you need to change some
of the configs, you can do so by passing additional arguments to the start script.
Try `--help` for a list of all available options.

### Running with BSP or on Tofino model

```bash
start-stratum.sh --bf_sim
```

The `--bf_sim` flag tells Stratum not to use the Phal ONLP implementation, but
`PhalSim`, a "fake" Phal implementation, instead. Use this flag when you are
using a vendor-provided BSP or running Stratum with the Tofino software model.

### Running the binary in BSP-less mode

```bash
start-stratum.sh --bf_switchd_cfg=/usr/share/stratum/tofino_skip_p4_no_bsp.conf
```

If ONLP support is available for your platform, you do not need to use a
BSP. Instead the platform vendor can provide a JSON "port mapping" file (see
this [example](platforms/x86-64-accton-wedge100bf-32x-r0.json) for the Wedge
100bf-32x) and Stratum takes care of making the information exposed by ONLP
available to the SDE as needed.

To start Stratum in BSP-less mode, copy the JSON port mapping file for your
platform to `/etc/stratum/<platform>/port_map.json` and run `start-stratum.sh` with
`--bf_switchd_cfg=stratum/hal/bin/barefoot/tofino_skip_p4_no_bsp.conf`.

Platforms with repeaters (such as the Wedge 100bf-65x) are not currently
supported in BSP-less mode.

We only support DAC cables at the moment, and autoneg must be forced "on" for
every port. See [sample_config.pb.txt](sample_config.pb.txt) for an example
(look for `autoneg: TRI_STATE_TRUE`). We are working on adding support for
optical cables.

By default FEC is turned off for every port. You can turn on FEC for a given
port in the chassis config file by adding `fec_mode: FEC_MODE_ON` to the
`config_params` message field for the appropriate singleton port entry. FEC will
then be configured automatically based on the port speed: Firecode for 10G and
40G, Reed-Solomon for all other speeds (25G, 50G, 100G and other supported port
speeds). For example:
```
singleton_ports {
  id: 132
  port: 132
  speed_bps: 100000000000
  config_params {
    admin_state: ADMIN_STATE_ENABLED
    autoneg: TRI_STATE_TRUE
    fec_mode: FEC_MODE_ON
  }
  node: 1
  name: "132"
  slot: 1
}
```
will configure device port 132 in 100G mode with Reed-Solomon (RS) FEC.

FEC can also be configured when adding a port through gNMI.

### Managing Stratum with systemd

Systemd provides service management and Stratum has been integrated into it.

Start/stop Stratum service manually:
```bash
systemctl start stratum_bf.service  # stop
```

Enable/disable auto-start of Stratum on boot:
```bash
systemctl enable stratum_bf.service  # disable
```

View logs:
```bash
journalctl -u stratum_bf.service
```

## Testing gNMI

See [gNMI CLI](/stratum/tools/gnmi/README.md)

## Stratum BfPipelineConfig format and the BfPipelineBuilder

Stratum supports a few different device configuration formats for pushing the P4
pipeline over P4Runtime for Barefoot devices, including
the older binary packing used by [PI](https://github.com/p4lang/PI)
and a newer more flexible protobuf based format ([bf.proto](stratum/hal/lib/barefoot/bf.proto)).
You can use the device config builder to generate the protobuf based format:

```bash
bazel run //stratum/hal/bin/barefoot:bf_pipeline_builder -- \
    -p4c_conf_file=/path/to/bf-p4c/compiler/output.conf \
    -bf_pipeline_config_binary_file=$PWD/device_config.pb.bin
```

The tool is also available as a [Docker image](https://hub.docker.com/repository/docker/stratumproject/stratum-bf-pipeline-builder):

```bash
docker run --rm -v $PWD:$PWD stratumproject/stratum-bf-pipeline-builder:latest \
    -p4c_conf_file=./output.conf \
    -bf_pipeline_config_binary_file=./device_config.pb.bin
```

The output goes into the `p4_device_config` field of the P4Runtime
`ForwardingPipelineConfig` message as usual.

To inspect a `BfPipelineConfig` message, the tool provides an unpacking mode,
which recreates the original compiler output files:

```bash
bazel run //stratum/hal/bin/barefoot:bf_pipeline_builder -- \
    -bf_pipeline_config_binary_file=$PWD/device_config.pb.bin \
    -unpack_dir=$PWD
```

### bf-p4c Archive Format

You can enable a special archive format by passing the
`-incompatible_enable_p4_device_config_tar` flag when starting Stratum.

*Note: Support for this format may disappear at any time.*

The format accepts a tar archive of the bf-p4c compiler output:

```bash
mkdir -p /tmp/p4out
bf-p4c <options...> \
  -o /tmp/p4out -I ${P4_SRC_DIR} \
  --p4runtime-files /tmp/p4out/p4info.txt \
  --p4runtime-force-std-externs \
  ${P4_SRC_DIR}/my_prog.p4
tar -czf "pipeline.tgz" -C "/tmp/p4out" .
```

The structure of the tar archive should look something like this:

```bash
$ tar -tf pipeline.tgz
./
./bfrt.json
./my_prog.conf
./pipe/
./pipe/context.json
./pipe/tofino.bin
```

To use this format, enable it with the aforementioned flag when starting
Stratum, and then use the contents of the tar archive as the `p4_device_config`
field of the P4Runtime `ForwardingPipelineConfig` message.

## Using p4runtime-shell

[p4runtime-shell](https://github.com/p4lang/p4runtime-shell) is an interactive
Python shell for P4Runtime. While it can also be used to set the P4 forwarding
pipeline and issue P4Runtime `Write` RPCs, it especially comes in handy when you
want to read the forwarding state of the switch.

To start a shell session, you can use (requires Docker):
```
./p4runtime-sh-docker --grpc-addr <Stratum IP>:9559 --device-id 1 --election-id 0,1
```

Refer to the [p4runtime-shell](https://github.com/p4lang/p4runtime-shell)
documentation for more information.

## Troubleshooting

### Huge pages / DMA allocation error

`ERROR: bf_sys_dma_buffer_alloc for dev_id 0 failed(-1)`

This error means that the Tofino driver could not allocate DMA memory from the
huge pages pool. Ensure that at least 128 huge pages are mounted and available:

```bash
> grep HugePages_ /proc/meminfo
HugePages_Total:     128
HugePages_Free:      128
HugePages_Rsvd:        0
HugePages_Surp:        0
```

To enable them or allocate more, follow the steps from the [post install script](deb/postinst).

### ONLP / BMC errors on Wedge100BF

`07-24 23:10:16.072010 [x86_64_accton_wedge100bf_32x] ERROR: bmc_send_command(cat /sys/bus/i2c/drivers/com_e_driver/4-0033/temp2_input
) timed out`

`07-25 08:30:59.834213 [x86_64_accton_wedge100bf_32x] Unable to read status from file (/sys/bus/i2c/drivers/lm75/3-0048/temp1_input)`

This error occurs when ONLP can not reach the BMC CPU managing the platform.
Either the ONL image is not correctly set up or ONLP support is simply broken on
this particular switch model. As a workaround, [BSP mode](#Running-with-BSP-or-on-Tofino-model),
which bypasses ONLP, is available.
