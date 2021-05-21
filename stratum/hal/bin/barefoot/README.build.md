<!--
Copyright 2018 Barefoot Networks, Inc.
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# Building Stratum for Barefoot Tofino based switches

## Prerequisites

### BF SDE Package

There are multiple supported ways to build Stratum for Tofino, but all
require access to the Barefoot SDE.

*Note: To do this, you will need to have a SELA with Intel (Barefoot) to
access P4 Studio SDE. Contact Intel for more details.*

#### Supported SDE versions

 - 9.2.0
 - 9.3.0
 - 9.3.1 (Recommended; LTS release)
 - 9.4.0
 - 9.5.0 (Experimental; LTS release; Latest)

The rest of this guide depends on the BF SDE tarball, so you can export an
environment variable that points to it:

```bash
export SDE_TAR=<path to tar>/bf-sde-<SDE_VERSION>.tgz
```

*__SDE Deprecation Policy:__ We support the latest SDE released by the Intel
Barefoot team as well as the previous LTS release (also called LLR, or Long
Lived Release). Support for deprecated SDE versions will be removed after
the next Stratum release. For example as of November 2020, 9.3.0 is the
latest SDE release (and also an LTS release), so it is supported. 9.1.0 is the
previous LTS release, so it is also supported. 9.2.0, which was the previous
latest release, is now deprecated and support may be removed after the next
Stratum release -- 2020-12.*

### Linux Kernel Headers

If you plan to run Stratum on hardware, you will need to build the kernel
module for your particular switch OS.

If you are running Stratum on ONL (OpenNetworkLinux), you can download
the [Linux kernel headers here][onl-linux-headers].

You can also find the Linux headers package for your distro by using `apt-get`,
for example:

```bash
[sudo] apt-get install linux-headers-$(uname -r)
```

### Docker

If you plan to build Stratum using Docker or build the Stratum Docker container,
we've tested with: **Docker 18.06.0-ce**

You can skip this depedency if you plan to build the Stratum Debian package
locally (Method 3 and Method 4).

## Building Stratum

### Method 1: Build with Docker in one shot

This is an all-in-one script that will take the longest, but also requires
fewest steps:

```bash
cd $STRATUM_ROOT
stratum/hal/bin/barefoot/docker/build-stratum-bf-container.sh $SDE_TAR [<path to Linux kernel headers package>]
```

The Linux kernel headers package can be omitted if you do not need to build the
kernel module; for example, if you only plan to use the Tofino model simulator.

### Method 2: Pre-build the SDE, then build Stratum with Docker (Recommended)

**This method is recommended for most users as well as automated builds.**

**The SDE must be built inside the Stratum development Docker container, else
there will be linking errors!**

#### Step 1: Generate the SDE install tarball
The first step is to build the SDE:

```bash
cd $STRATUM_ROOT
stratum/hal/bin/barefoot/build-bf-sde.sh -t $SDE_TAR [-k <path to Linux kernel headers package>]
```

You can also build the SDE to support multiple kernel modules, for example:

```bash
stratum/hal/bin/barefoot/build-bf-sde.sh -t $SDE_TAR \
  -k linux-4.9.75-OpenNetworkLinux.tar.xz \
  -k linux-4.14.49-OpenNetworkLinux.tar.xz
```

Or, you can build the SDE with support for the local kernel:

```bash
stratum/hal/bin/barefoot/build-bf-sde.sh -t $SDE_TAR --build-local-kernel-mod
```

However you choose to build the SDE, the script will output the SDE install
tarball. Export the tarball path as an environment variable so that you can
use it in the next step.

```bash
export SDE_INSTALL_TAR=<path to SDE tar>/bf-sde-<SDE_VERSION>-install.tgz
```

*Note: The SDE install tarball should be used as an input for automated builds.*

#### Step 2: Build Stratum using Docker

The second step is to build the Stratum Debian package and Docker container
using our simple build script:

```bash
stratum/hal/bin/barefoot/docker/build-stratum-bf-container.sh
```

*Note: This is the same script that was used in Method 1, but without any
additional arguments. It is important that `SDE_INSTALL_TAR` is set in this
method.*

### Method 3: Pre-build the SDE, then build Stratum locally

This method is primarily useful to developers that want to make changes
to Stratum with a minimal recompile/test cycle without needing to recompile
the BF SDE or Linux kernel modules.

First follow [Step 1 from Method 2](#step-1:-generate-the-sde-install-tarball)
(above) to generate the SDE install tarball.
Make sure to export the path to the install tarball as an environment variable,
then you can use Bazel to build the Stratum.

#### To build `stratum_bf`:
```bash
bazel build //stratum/hal/bin/barefoot:stratum_bf_deb
```

#### To build `stratum_bfrt`:
```bash
bazel build //stratum/hal/bin/barefoot:stratum_bfrt_deb
```

These Bazel targets build the Stratum binary and package all
necessary configurations into a single Debian package (.deb file). The Debian
package also includes systemd service definition so users can use systemd to
start the Stratum as a system service.

The resulting Debian package can be found here:
`bazel-bin/stratum/hal/bin/barefoot/stratum_bf_deb.deb` or
`bazel-bin/stratum/hal/bin/barefoot/stratum_bfrt_deb.deb`

Copy this file over to the switch and follow the
[running Stratum](./README.run.md) instructions.

The [Stratum build options](#stratum-build-options) are explained
below.

#### Optional: Create Docker image

If you want to create a Docker image from the Debian package,

```bash
export SDE_VERSION=9.3.1
export STRATUM_TARGET=stratum_bf
docker build -t stratumproject/stratum-bf:$SDE_VERSION \
  --build-arg STRATUM_TARGET="$STRATUM_TARGET" \
  -f stratum/hal/bin/barefoot/docker/Dockerfile \
  bazel-bin/stratum/hal/bin/barefoot
```

You can push the image to Docker Hub using `docker push <...>`, and
then pull the image on the switch over the Internet using `docker pull <...>`.
You will also have to use your own Docker Hub account or org in place of
`stratumproject`.

If you need to push the image to the switch directly
(e.g. your switch does not have connectivity to the Internet),
save the container image as a tarball with the following command:

```bash
docker save [Image Name] -o [Tarball Name]
```

For example,
```bash
docker save stratumproject/stratum-bf:9.3.1 -o stratum-bf-9.3.1-docker.tar
```

### Method 4: Build the SDE and Stratum locally

This method is for developers that need to make changes to the BF SDE and
Stratum with a minimal recompile/test cycle.

#### Step 1: Build the BF SDE

Extract the SDE and build as normal:

```bash
tar -xzvf bf-sde-<SDE_VERSION>.tgz
export SDE=`pwd`/bf-sde-<SDE_VERSION>
export SDE_INSTALL=$SDE/install
cd $SDE/p4studio_build
./p4studio_build.py -up profiles/stratum_profile.yaml [-kdir <path/to/linux/sources>] [--bsp-path $BSP_PATH]
```

Barefoot's P4Studio Build tool comes with a default Stratum profile
(`stratum_profile.yaml`), which takes care of installing all the necessary
dependencies and builds the SDE with the appropriate flags. Feel free to
customize the profile if needed; please refer to the P4Studio Build documentation.
If you are using the
reference BSP provided by Barefoot, you may also use P4Studio Build to
install the BSP (see [below](#board-support-package-bsp-or-onlpv2)).

Remember to download and pass the correct Kernel sources (`-kdir`) if you
are building modules for a specific version other than the host's.

As there are some issues with building the SDE on ONL switches, it's better to
do that on a separate server.

#### Step 2: Patching the BF SDE install directory

Stratum depends on a few minor modifications to the `SDE_INSTALL` directory.

With `$SDE_INSTALL` set, you can run the following to patch the directory:

```bash
cd $STRATUM_ROOT
stratum/hal/bin/barefoot/patch-bf-sde-install.sh
```

*Note: This only needs to be done one time.*

#### Step 3: Build Stratum

Make sure `$SDE_INSTALL` is set and `$SDE_INSTALL_TAR` is not set, then
follow the same instructions from
[Method 3](#method-3:-pre-build-the-sde,-then-build-stratum-locally).

Bazel will use the local `SDE_INSTALL` directory for the build.

The output of this step is a Debian package, and optionally, a Docker
container as in Method 3.

-----

## Stratum Build Options

### Building for a different SDE version

Stratum is designed for the latest Barefoot SDE. The SDE version is detected
automatically by reading the `$SDE_INSTALL_TAR/share/VERSION` file.

-----

## BF SDE Build Options

These are only available in
[Method 4](#method-4:-build-the-sde-and-stratum-locally).

### Board support package (BSP) or ONLPv2?

Stratum can be run on Tofino-based platforms in 2 different modes:

**ONLPv2**

If your platform comes with ONLPv2 and a JSON "port mapping" file is provided
by the platform vendor (see this
[example](../../config/x86-64-accton-wedge100bf-32x-r0/port_map.json) for the
Wedge 100bf-32x), you can use Stratum in "BSP-less mode". Refer to this
[section](./README.run.md#running-the-binary-in-bsp-less-mode) for more
information. **This is the recommended mode. No changes to the SDE needed.**

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

[onl-linux-headers]: https://github.com/opennetworkinglab/OpenNetworkLinux/releases/tag/onlpv2-dev-1.0.1
