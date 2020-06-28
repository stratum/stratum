<!--
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# Containerize Stratum for Barefoot Tofino based device

This directory provides most material to build the containerized version of the
Stratum for Barefoot Tofino based device.

You can pull a prebuilt version of this container image from the [Dockerhub](https://hub.docker.com/repository/docker/stratumproject/stratum-bf/tags)

```bash
# Docker images with specific SDE version and kernel module (for ONL-based system)
$ docker pull stratumproject/stratum-bf:[SDE version]-[Linux kernel version]
# Or images with specific SDE version but no kernel module.
$ docker pull stratumproject/stratum-bf:[SDE version]
```

For example, the container with the BF-SDE 9.1.0 and Linux kernel 4.14.49 is: <br/>
`stratumproject/stratum-bf:9.1.0-4.14.49-OpenNetworkLinux`

## Building the image manually

### Dependency

 - Docker: 18.06.0-ce
 - Barefoot SDE
 - Linux headers of your NOS (e.g., OpenNetworkLinux)

To build the Docker image for runtime or development, first, you need to have the Barefoot SDE and Linux headers tarball(optional). You can find the Linux headers package by using apt-get. Or you can download Linux headers [here][onl-linux-headers] if you are using the OpenNetworkLinux.

To build the Docker image, run `build-stratum-bf-container.sh` script with SDE and Linux header tarball, for example:

```bash
$ ./build-stratum-bf-container.sh ~/bf-sde-9.0.0.tgz ~/linux-4.14.49-ONL.tar.xz
```

You can also build the container image without Kernel headers, and the script will skip building the Kernel module.

```bash
$ ./build-stratum-bf-container.sh ~/bf-sde-9.0.0.tgz
```

Optional environment variables for this script:

 - JOBS: The number of jobs to run simultaneously while building the SDE. Default is 4
 - WITH_ONLP: Includes ONLP library linking. The default is true.

__Note:__ This script saves an intermediate image named `stratumproject/stratum-bf-builder` for caching artifacts from building SDE, which could be used to speed up future builds when the same SDE tarballs are used as input to the script.

## Deploy the container to the device

Before deploing the container to the device, make sure you install Docker 18 on the
device. No other dependency required for the device.

If your switch has connectivity to the Internet, you can pull the image directly from Dockerhub
using the pull command above and proceed to the next section.

If your switch does not have connectivity to the Internet or you are building the container
yourself (see below), save the container image as a tarball with the following command:

```bash
$ docker save [Image Name] -o [Tarball Name]
```

And deploy the tarball to the device via scp, rsync, http, USB stick, etc.

To load the tarball to the Docker, use the following command:

```bash
$ docker load -i [Tarball Name]
# And you should be able to find your image by command below
$ docker images
```

## Run the container on the device

### Set up the huge page

Before starting the container, make sure you have set up the huge page for DMA purposes.

__Note:__ This step only needs to be done once.

```bash
$ [sudo] echo "vm.nr_hugepages = 128" >> /etc/sysctl.conf
$ [sudo] sysctl -p /etc/sysctl.conf
$ [sudo] mkdir /mnt/huge
$ [sudo] mount -t hugetlbfs nodev /mnt/huge
```

If you re-image your switch (reload ONL via ONIE), you will need to run these commands again.

### Upload script to the switch

You can upload [start-stratum-container.sh][start-stratum-container-sh] to the device so you can start the container easily.

### Start the Stratum container

After setting up everything, run `start-stratum-container.sh` on the device with some environment variables(see below) if needed.

```bash
export CHASSIS_CONFIG=/path/to/chassis_config.pb.txt
start-stratum-container.sh
```

### Environment variables for `start-stratum-container.sh`

```bash
CHASSIS_CONFIG  # Override the default chassis config file.
FLAG_FILE   # Override the default flag file.
LOG_DIR     # The directory for logging, default: `/var/log/`.
SDE_VERSION # The SDE version
KERNEL_VERSION    # The Linux kernel version, default: `uname -r`.
DOCKER_IMAGE      # The container image name, default: stratumproject/stratum-bf
DOCKER_IMAGE_TAG  # The container image tag, default: $SDE_VERSION-$KERNEL_VERSION
PLATFORM          # Use specific platform port map
```

## Building Stratum container for non-OpenNetworkingLinux system (e.g., Ubuntu)

To run Stratum container on an operating system that is not OpenNetworkingLinux.
First, you need to prepare a kernel header tarball of your Linux kernel.

### Prepare a tarball file of your Linux kernel

Download the Linux header via the package manager, for example:

```bash
[sudo] apt update
[sudo] apt install -y linux-headers-`uname -r`
```

Package the kernel header as a tarball.

```bash
tar cfh linux-`uname -r`.tar.xz -C /usr/src linux-headers-`uname -r`
```

### Build Stratum container manually

To build Stratum container image manually without ONLP library included, follow **Building the image manually** section below and add an additional environment variable `WITH_ONLP=false` to disable the Open Networking Linux platform API support.

For example:

```bash
WITH_ONLP=false ./build-stratum-bf-container.sh ~/bf-sde-9.0.0.tgz ~/linux-`uname -r`.tar.xz
```

[onl-linux-headers]: https://github.com/opennetworkinglab/OpenNetworkLinux/releases/tag/onlpv2-dev-1.0.1
[start-stratum-container-sh]: https://github.com/stratum/stratum/blob/master/stratum/hal/bin/barefoot/docker/start-stratum-container.sh
