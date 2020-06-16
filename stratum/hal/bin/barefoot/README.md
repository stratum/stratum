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

Building a Docker image yourself and more us covered in the [Docker README](./docker/README.md).

### 2. Build and deploy Stratum Debian package

```bash
apt-get install --reinstall ./stratum_bf_deb.deb
stratum-entrypoint.sh
```

In the future we might provide pre-built Debian packages, but for now, you have
to build them yourself as lined out in this document.

## Installing the SDE

Before you can build Stratum, the Barefoot SDE needs to be installed.

Note: To do this, you will need to have SELA with Intel (Barefoot) to access P4
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
[example](platforms/x86-64-accton-wedge100bf-32x-r0.json) for the Wedge 100bf-32x),
you can use Stratum in "BSP-less mode". Refer to this
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
./p4studio_build.py ... --bsp-path $BSP_PATH
```

## Building Stratum

The [SDE](#installing-the-sde) needs to be installed and set up for this step.

```bash
bazel build //stratum/hal/bin/barefoot:stratum_bf_deb [--define phal_with_onlp=false] [--define sde_ver=9.2.0]
```

We provide a Bazel target that build the Stratum binary and package all
necessary configurations to a single Debian package(.deb file). The Debian
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
apt-get install --reinstall ./stratum_bf_deb.deb
stratum-entrypoint.sh
```

You can safely ignore warnings like this:
`N: Download is performed unsandboxed as root as file '/root/stratum_bf_deb.deb' couldn't be accessed by user '_apt'. - pkgAcquire::Run (13: Permission denied)`

Stratum picks sane defaults for most platforms, but should you need to change some
of the configs, you can do so by passing additional arguments to the start script.
Try `--help` for a list of all available options.

### Running with BSP or on Tofino model

```bash
stratum-entrypoint.sh --bf_sim
```

The `--bf_sim` flag tells Stratum not to use the Phal ONLP implementation, but
`PhalSim`, a "fake" Phal implementation, instead. Use this flag when you are
using a vendor-provided BSP or running Stratum with the Tofino software model.

### Running the binary in BSP-less mode

```bash
stratum-entrypoint.sh --bf_switchd_cfg=/usr/share/stratum/tofino_skip_p4_no_bsp.conf
```

If ONLP support is available for your platform, you do not need to use a
BSP. Instead the platform vendor can provide a JSON "port mapping" file (see
this [example](platforms/x86-64-accton-wedge100bf-32x-r0.json) for the Wedge
100bf-32x) and Stratum takes care of making the information exposed by ONLP
available to the SDE as needed.

To start Stratum in BSP-less mode, copy the JSON port mapping file for your
platform to `/etc/stratum/<platform>/port_map.json` and run `stratum-entrypoint.sh` with
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

## Testing gNMI

See [gNMI CLI](/tools/gnmi/README.md)

## Using p4runtime-shell

[p4runtime-shell](https://github.com/p4lang/p4runtime-shell) is an interactive
Python shell for P4Runtime. While it can also be used to set the P4 forwarding
pipeline and issue P4Runtime `Write` RPCs, it especially comes in handy when you
want to read the forwarding state of the switch.

To start a shell session, you can use (requires Docker):
```
./p4runtime-sh-docker --grpc-addr <Stratum IP>:28000 --device-id 1 --election-id 0,1
```

Refer to the [p4runtime-shell](https://github.com/p4lang/p4runtime-shell)
documentation for more information.
