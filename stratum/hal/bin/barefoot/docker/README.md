Containerize Stratum for Barefoot Tofino based device
====

This directory provides most material to build the containerize version of the
Stratum for Barefoot Tofino based device.

You can pull prebuilt version of this container image from the Dockerhub

```bash
$ docker pull stratumproject/stratum-bf:[SDE version]-[Linux kernel version]
``` 

### Dependency

 - Docker: 18.06.0-ce
 - Barefoot SDE
 - Linux headers of your NOS (e.g., OpenNetworkLinux)

To build the Docker image for runtime or development, first you need to place
the Barefoot SDE and Linux headers tarball to the Stratum root directory.

You can find the Linux headers package by using apt-get.
Or you can download Linux headers [here][onl-linux-headers] if you are using the OpenNetworkLinux.

### Building the image

To build the Docker image, run `build-stratum-bf-container.sh` script with SDE and Linux header tarball, for example:

```bash
$ ./build-stratum-bf-container.sh
```

The expected format of container image name will be `stratumproject/stratum-bf:[SDE Version]-[Linux Kernel Version]`  

### Deploy the container to the device

Before deploy the container to the device, make sure you install Docker 18 on the 
device. No other dependency required for the device.

If you are building the container on your local PC or server, save the container image
as a tarball with following command:

```bash
$ docker save [Image Name] -o [Tarball Name]
```

And deploy the tarball to the device via scp, rsync, http, etc.

To load the tarball to the Docker use the following command:

```bash
$ docker load -i [Tarball Name]
# And you should be able to find your image by
$ docker images
```

### Run the docker container on the device

Before start the container, make sure you have setup the huge page for DMA purposes.

__Note:__ This step only needs to be done once

```bash
$ [sudo] echo "vm.nr_hugepages = 128" >> /etc/sysctl.conf
$ [sudo] sysctl -p /etc/sysctl.conf
$ [sudo] mkdir /mnt/huge
$ [sudo] mount -t hugetlbfs nodev /mnt/huge
```

After everything setup, run `start-stratum-container.sh` on the device.

### Environment variables for `start-stratum-container.sh`

```bash
CONFIG_DIR  # The directory for configuration, default: `/root`
LOG_DIR     # The directory for logging, default: `/var/log/`.
SDE_VERSION # The SDE version, default: `8.9.1`
KERNEL_VERSION    # The Linux kernel version, default: `uname -r`. 
DOCKER_IMAGE      # The container image name, default: stratumproject/stratum-bf
DOCKER_IMAGE_TAG  # The container image tag, default: $SDE_VERSION-$KERNEL_VERSION
```

### Custom configurations

You can place config files such as chassis config, flag file, phal config to the `CONFIG_DIR` directory (default is `/root`) and start the Stratum container.

[onl-linux-headers]: https://github.com/opennetworkinglab/OpenNetworkLinux/releases/tag/onlpv2-dev-1.0.1 
