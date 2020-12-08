<!--
Copyright 2018 Barefoot Networks, Inc.
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# Running Stratum on a Barefoot Tofino based switch

Stratum can be deployed using a Debian package or Docker container.

The easiest way to get started is using a pre-built packages and container:

### Latest Release version

You can find Debian packages and Docker containers on the
[Stratum Releases](https://github.com/stratum/stratum/releases) page.

### Nightly version

You can pull a nightly version of this container image from
[Dockerhub](https://hub.docker.com/repository/docker/stratumproject/stratum-bf/tags)

```bash
$ docker pull stratumproject/stratum-bf:[SDE version]
```

For example, the container with BF SDE 9.3.0: <br/>
`stratumproject/stratum-bf:9.3.0`

These containers include kernel modules for OpenNetworkLinux.

-----

## Running Stratum in a Docker container

**This is the recommended way to run Stratum.**

Before deploing the container to the device, make sure you install Docker on the
switch; we've tested with: **Docker 18.06.0-ce**

No other dependencies are required.

The next step is to load the proper container into Docker. This can
be done by pulling one of the prebuilt images above, pulling your own
image (with `docker pull <...>`), or loading an image from a tarball.

### Loading Docker image from a tarball

If your switch does not have connectivity to the internet, you can copy a
tarball of the Docker image to the switch.

Create a custom tarball yourself using the [build guide](./README.build.md) or
download one of the prebuilt images on a machine connected to the Internet and
save it using:

```bash
docker save [Image Name] -o [Tarball Name]
```

For example,
```bash
docker pull stratumproject/stratum-bf:9.3.0
docker save stratumproject/stratum-bf:9.3.0 -o stratum-bf-9.3.0-docker.tar
```

Then, deploy the tarball to the device via scp, rsync, http, USB stick, etc.

To load the tarball into Docker, use the following command on the switch:

```bash
docker load -i [Tarball Name]
# And you should be able to find your image by command below
docker images
```

For example,

```bash
docker load -i stratum-bf-9.3.0-docker.tar
```

### Set up huge pages

Before starting the container, make sure you have set up the huge page for DMA purposes.

__Note:__ This step only needs to be done once.

```bash
[sudo] echo "vm.nr_hugepages = 128" >> /etc/sysctl.conf
[sudo] sysctl -p /etc/sysctl.conf
[sudo] mkdir /mnt/huge
[sudo] mount -t hugetlbfs nodev /mnt/huge
```

If you re-image your switch (reload ONL via ONIE), you will need to run these commands again.

### Upload start script to the switch

You can upload the [start-stratum-container.sh](./docker/start-stratum-container-sh)
convenience script to the switch to more easily start the container.

### Start the Stratum container

After setting up everything, run `start-stratum-container.sh` on the device with some environment variables (see below if needed).

```bash
export CHASSIS_CONFIG=/path/to/chassis_config.pb.txt
start-stratum-container.sh
```

For more details on additional options that can be passed to
`start-stratum-container.sh`, see [below](#stratum-runtime-options).

### Environment variables for `start-stratum-container.sh`

```bash
CHASSIS_CONFIG    # Override the default chassis config file.
FLAG_FILE         # Override the default flag file.
LOG_DIR           # The directory for logging, default: `/var/log/`.
SDE_VERSION       # The SDE version
DOCKER_IMAGE      # The container image name, default: stratumproject/stratum-bf
DOCKER_IMAGE_TAG  # The container image tag, default: $SDE_VERSION
PLATFORM          # Use specific platform port map
```

-----

## Running Stratum natively on the switch

Stratum can run natively on the switch (e.g. if you are unable or unwilling
to run Docker, or if you want to manage Stratum using systemd).

### Install the Stratum Debian package on the switch

Either download one of the prebuilt Debian packages (above) or build your own
using the [build guide](./README.build.md).

Install the package using the followign command on the switch:

```bash
[sudo] apt-get update
[sudo] apt-get install -y --reinstall ./stratum_bf_deb.deb
```

You can safely ignore warnings like this:
`N: Download is performed unsandboxed as root as file '/root/stratum_bf_deb.deb' couldn't be accessed by user '_apt'. - pkgAcquire::Run (13: Permission denied)`

### Running Stratum
```bash
[sudo] start-stratum.sh
```

For more details on additional options, see [below](#stratum-runtime-options).

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

-----

## Stratum Runtime Options

Stratum picks sane defaults for most platforms, but should you need to change some
of the configs, you can do so by passing additional arguments to the start script.
Try `--help` for a list of all available options.


These options can be used when starting Stratum in Docker
(using `start-stratum-container.sh`) or natively
(usimg `start-stratum.sh`).

For brevity, we will just show examples
using `start-stratum.sh`, but replace that with `start-stratum-container.sh`
in the examples below if using Docker.

### Chassis Config

By default, Stratum tries to load a default Chassis Config file on startup for
supported platforms. This file controls which ports are configured, and by
default, we configure all ports in their non-channelized, maximum speed
configuration.

If you wish to provide your own configuration, you can do so with the
`chassis_config_file` flag when using `start-stratum.sh` or the
`CHASSIS_CONFIG` environment variable when using `start-stratum-container.sh`.

For example,
```bash
start-stratum.sh --chassis_config_file=/path/to/chassis_config.pb.txt
```
or
```bash
export CHASSIS_CONFIG=/path/to/chassis_config.pb.txt
start-stratum-container.sh
```

Here is an example Chassis Config that brings up the first two physical ports:
```proto
chassis {
  platform: PLT_BAREFOOT_TOFINO
  name: "Edgecore Wedge100BF-32x"
}
nodes {
  id: 1
  slot: 1
  index: 1
}
singleton_ports {
  id: 1
  name: "1/0"
  slot: 1
  port: 1
  speed_bps: 100000000000
  config_params {
    admin_state: ADMIN_STATE_ENABLED
  }
  node: 1
}
singleton_ports {
  id: 2
  name: "2/0"
  slot: 1
  port: 2
  speed_bps: 100000000000
  config_params {
    admin_state: ADMIN_STATE_ENABLED
  }
  node: 1
}
```

You can also provide an empty file as the Chassis Config. In this case, Stratum
will begin to start up but then wait for configuration. You can push the
configuration over gNMI to the root path ("/") using `Set` and providing either
the `ChassisConfig` protobuf message or an `openconfig::Device` protobuf message.

#### Port Numbers

Previously, the port `id` was required to match the BF device port ID for Stratum
to function. Stratum now reads BF device port ID from the SDK using the `node`,
`port`, and `channel` params provided in the `singleton_ports` config, which means
the port `id` can now be set to a user-selected value. The `id` is required, and
it must be positive and unique across all ports in the config.

The BF device port ID (SDK port ID) can be read using gNMI:
`/interfaces/interface[name=<name>]/state/ifindex`

In both cases, the `name` used in the gNMI paths should match the name provided
in the `singleton_ports` config.

*Note: You should use the BF device port ID (SDK port ID) when reading and
writing P4Runtime entities and packets. In the future, we may support P4Runtime
port translation which would allow you to use the user-provide SDN port ID.*

### Running with BSP or on Tofino model

```bash
start-stratum.sh --bf_sim -enable_onlp=false
```

The `--bf_sim` flag tells Stratum not to use the Phal ONLP implementation, but
`PhalSim`, a "fake" Phal implementation, instead. Use this flag when you are
using a vendor-provided BSP or running Stratum with the Tofino software model.
Additionally, the ONLP plugin has to be disabled with `-enable_onlp=false`.

### Running the binary in BSP-less mode

```bash
start-stratum.sh --bf_switchd_cfg=/usr/share/stratum/tofino_skip_p4_no_bsp.conf -enable_onlp=true
```

If ONLP support is available for your platform, you do not need to use a
BSP. Instead the platform vendor can provide a JSON "port mapping" file (see
this [example](platforms/x86-64-accton-wedge100bf-32x-r0.json) for the Wedge
100bf-32x) and Stratum takes care of making the information exposed by ONLP
available to the SDE as needed.

To start Stratum in BSP-less mode, copy the JSON port mapping file for your
platform to `/etc/stratum/<platform>/port_map.json` and run `start-stratum.sh` with
`--bf_switchd_cfg=stratum/hal/bin/barefoot/tofino_skip_p4_no_bsp.conf`. Make
sure to include the `-enable_onlp=true` flag to activate the ONLP plugin.

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
40G, Reed-Solomon (RS) for all other speeds (25G, 50G, 100G and other supported port
speeds). For example, the following will configure device port 132 in 100G mode
with Reed-Solomon Forward Error Correction (FEC) enabled:

```proto
singleton_ports {
  id: 132
  name: "132"
  slot: 1
  port: 2
  speed_bps: 100000000000
  config_params {
    admin_state: ADMIN_STATE_ENABLED
    autoneg: TRI_STATE_TRUE
    fec_mode: FEC_MODE_ON
  }
  node: 1
}
```

FEC can also be configured when adding a port through gNMI.

-----

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

[start-stratum-container-sh]: https://github.com/stratum/stratum/blob/master/stratum/hal/bin/barefoot/docker/start-stratum-container.sh

### TNA P4 programs on Stratum-bf / PI Node

When using Stratum with the legacy PI node backend, only limited support for P4
programs targeting TNA architecture is provided. Such programs must be compiled
with the `--p4runtime-force-std-externs` bf-p4c flag, or pushing the pipeline
will crash the switch:

```
2020-12-07 20:09:43.810989 BF_PI ERROR - handles_map_add: error when inserting into handles map
*** SIGSEGV (@0x0) received by PID 16282 (TID 0x7f5f599dc700) from PID 0; stack trace: ***
    @     0x7f5f725c60e0 (unknown)
    @           0xa7456f p4info_get_at
    @           0xa74319 pi_p4info_table_get_implementation
    @     0x7f5f744b9add (unknown)
    @     0x7f5f744b9ee3 pi_state_assign_device
    @     0x7f5f744b2f47 (unknown)
    @     0x7f5f73e29b10 bf_drv_notify_clients_dev_add
    @     0x7f5f73e26b45 bf_device_add
    @           0x9f0689 bf_switchd_device_add.part.4
    @           0x9f10b7 bf_switchd_device_add_with_p4.part.5
    @     0x7f5f744b33a5 _pi_update_device_start
    @           0xa6eef5 pi_update_device_start
    @           0x9f8403 pi::fe::proto::DeviceMgrImp::pipeline_config_set()
    @           0x9f7e31 pi::fe::proto::DeviceMgr::pipeline_config_set()
    @           0x7220d6 stratum::hal::pi::PINode::PushForwardingPipelineConfig()
    @           0x41fb99 stratum::hal::barefoot::BFSwitch::PushForwardingPipelineConfig()
    @           0x65db56 stratum::hal::P4Service::SetForwardingPipelineConfig()
```

Use Stratum-bfrt with the BfRt backend if you need advanced functionality.

### Error pushing pipeline to Stratum-bf

```
E20201207 20:44:53.611562 18416 PI-device_mgr.cpp:0] Error in first phase of device update
E20201207 20:44:53.611724 18416 bf_switch.cc:135] Return Error: pi_node->PushForwardingPipelineConfig(config) failed with generic::unknown:
E20201207 20:44:53.612004 18416 p4_service.cc:381] generic::unknown: Error without message at stratum/hal/lib/common/p4_service.cc:381
E20201207 20:44:53.612030 18416 error_buffer.cc:30] (p4_service.cc:422): Failed to set forwarding pipeline config for node 1: Error without message at stratum/hal/lib/common/p4_service.cc:381
```

This error occurs when the binary pipeline is not in the correct format.
Make sure the pipeline config binary has been packed correctly for PI node, like
so: https://github.com/stratum/stratum/blob/master/stratum/hal/bin/barefoot/update_config.py#L39-L52.
You cannot push the compiler output (e.g. `tofino.bin`) directly.

Also, consider moving to the newer [protobuf](README.pipeline.md) based pipeline
format.
