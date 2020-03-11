Containerize Stratum for Barefoot Tofino based device
====

This directory provides most material to build the containerized version of the
Stratum for Barefoot Tofino based device.

You can pull prebuilt version of this container image from the [Dockerhub](https://hub.docker.com/repository/docker/stratumproject/stratum-bf/tags)

```bash
$ docker pull stratumproject/stratum-bf:[SDE version]-[Linux kernel version]
```

For example, the container with the latest BF-SDE and Linux kernel 4.14.49 is: <br/>
`stratumproject/stratum-bf:latest-4.14.49-OpenNetworkLinux`

### Dependency

 - Docker: 18.06.0-ce
 - Barefoot SDE
 - Linux headers of your NOS (e.g., OpenNetworkLinux)

To build the Docker image for runtime or development, first you need to have the Barefoot SDE and Linux headers tarball. You can find the Linux headers package by using apt-get. Or you can download Linux headers [here][onl-linux-headers] if you are using the OpenNetworkLinux.


### Deploy the container to the device

Before deploy the container to the device, make sure you install Docker 18 on the
device. No other dependency required for the device.

If your switch has connectivity to the Internet, you can pull the image directly from Dockerhub
using the pull command above and proceed to the next section.

If your switch does not have connectivity to the Internet or you are building the container
yourself (see below), save the container image as a tarball with following command:

```bash
$ docker save [Image Name] -o [Tarball Name]
```

And deploy the tarball to the device via scp, rsync, http, USB stick, etc.

To load the tarball to the Docker use the following command:

```bash
$ docker load -i [Tarball Name]
# And you should be able to find your image by
$ docker images
```

### Run the docker container on the device

Before start the container, make sure you have set up the huge page for DMA purposes.

__Note:__ This step only needs to be done once.

```bash
$ [sudo] echo "vm.nr_hugepages = 128" >> /etc/sysctl.conf
$ [sudo] sysctl -p /etc/sysctl.conf
$ [sudo] mkdir /mnt/huge
$ [sudo] mount -t hugetlbfs nodev /mnt/huge
```

After everything setup, run `start-stratum-container.sh` on the device.

If you re-image your switch (reload ONL via ONIE), you will need to run these commands again.

### Environment variables for `start-stratum-container.sh`

```bash
CONFIG_DIR  # The directory for configuration, default: `/root`
LOG_DIR     # The directory for logging, default: `/var/log/`.
SDE_VERSION # The SDE version
KERNEL_VERSION    # The Linux kernel version, default: `uname -r`.
DOCKER_IMAGE      # The container image name, default: stratumproject/stratum-bf
DOCKER_IMAGE_TAG  # The container image tag, default: $SDE_VERSION-$KERNEL_VERSION
```

### Custom configurations

You can place config files such as chassis config, flag file, phal config to the `CONFIG_DIR` directory (default is `/root`) and start the Stratum container.

[onl-linux-headers]: https://github.com/opennetworkinglab/OpenNetworkLinux/releases/tag/onlpv2-dev-1.0.1

### Building the image manually

If you would like to build the Docker image yourself, run `build-stratum-bf-container.sh` script with SDE and Linux header tarball, for example:

```bash
$ ./build-stratum-bf-container.sh ~/bf-sde-9.0.0.tgz ~/linux-4.14.49-ONL.tar.gz
```

__Note:__ This script saves an intermediate image named `stratumproject/stratum-bf-builder` for caching artifacts from building SDE and kernel modules which could be used to speed up future builds when the same SDE and Linux header tarballs are used as input to the script.
