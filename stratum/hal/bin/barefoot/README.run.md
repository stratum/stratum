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
[Dockerhub](https://hub.docker.com/r/stratumproject/stratum-bfrt/tags)

```bash
$ docker pull stratumproject/stratum-bfrt:latest-[SDE version]
```

For example, the container with BF SDE 9.7.2: <br/>
`stratumproject/stratum-bfrt:latest-9.7.2`

These containers include kernel modules for OpenNetworkLinux.

-----

## Running Stratum in a Docker container

**This is the recommended way to run Stratum.**

Before deploing the container to the device, make sure you install Docker on the
switch; we've tested with: **Docker 18.09.8 community edition**

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
docker pull stratumproject/stratum-bfrt:latest-9.7.2
docker save stratumproject/stratum-bfrt:latest-9.7.2 -o stratum-bfrt-9.7.2-docker.tar
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
docker load -i stratum-bfrt-9.7.2-docker.tar
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

If you re-image your switch (reload SONiC via ONIE), you will need to run these commands again.

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
LOG_DIR           # The directory for logging, default: `/var/log/`.
SDE_VERSION       # The SDE version
DOCKER_IMAGE      # The container image name, default: stratumproject/stratum-bfrt
DOCKER_IMAGE_TAG  # The container image tag, default: latest-$SDE_VERSION
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
[sudo] apt-get install -y --reinstall ./stratum_bfrt_deb.deb
```

You can safely ignore warnings like this:
`N: Download is performed unsandboxed as root as file '/root/stratum_bfrt_deb.deb' couldn't be accessed by user '_apt'. - pkgAcquire::Run (13: Permission denied)`

### Running Stratum
```bash
[sudo] start-stratum.sh
```

For more details on additional options, see [below](#stratum-runtime-options).
Otherwise, continue with the [pipeline README](/stratum/hal/bin/barefoot/README.pipeline.md)
on how to load a P4 pipeline into Stratum.

### Managing Stratum with systemd

Systemd provides service management and Stratum has been integrated into it.

Start/stop Stratum service manually:
```bash
systemctl start stratum_bfrt.service  # stop
```

Enable/disable auto-start of Stratum on boot:
```bash
systemctl enable stratum_bfrt.service  # disable
```

View logs:
```bash
journalctl -u stratum_bfrt.service
```

-----

## Running Stratum on `tofino-model`

Stratum can be run on a Tofino simulator. In these instructions, Stratum and
`tofino-model` are both running in their own containers sharing the host's
network stack. Other configurations are possible.

### Building the `tofino-model` container

Before running `tofino-model`, we can build a container that contains the
required binaries. *This only needs to be done once for each BF SDE version.*

```bash
export SDE_TAR=<path to tar>/bf-sde-<SDE_VERSION>.tgz
./stratum/hal/bin/barefoot/docker/build-tofino-model-container.sh $SDE_TAR
```

### Running Stratum and `tofino-model`

#### Start `tofino-model`

In one terminal window, run `tofino-model` in one container:

```bash
docker run --rm -it --privileged \
  --network=host \
  stratumproject/tofino-model:9.7.2  # <SDE_VERSION>
```

In another terminal window, run Stratum in its own container:

```bash
PLATFORM=barefoot-tofino-model \
stratum/hal/bin/barefoot/docker/start-stratum-container.sh \
  -bf_switchd_background=false
```

### Cleaning up `tofino-model` interfaces

To remove the interfaces created when the `tofino-model` container starts,
you can run the following (with `sudo` if not running as root):

```bash
ip link show | egrep -o '(veth[[:digit:]]+)' | sort -u | \
   xargs -n1 -I{} [sudo] ip link del {} 2> /dev/null
```

### Other deployment options

You can run both Stratum and `tofino-model` natively on the host (i.e. not
in Docker containers). They communicate over localhost TCP ports.

If you wish to run multiple instances of Stratum and `tofino-model` on the
same machine, you can use Docker's container network to link the containers'
networking stacks together. For example, you can pass
`--network container:stratum` when starting the `tofino-model` container.

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
  platform: PLT_GENERIC_BAREFOOT_TOFINO
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
it must be positive and unique across all ports in the config. For consistency we
use the following schema to derive the default port IDs in our configs:

- Non-channelized ports: ID = front panel port number
- Channelized ports: ID = front panel port * 100 + channel

The BF device port ID (SDK port ID) can be read using gNMI:
`/interfaces/interface[name=<name>]/state/ifindex`

In both cases, the `name` used in the gNMI paths should match the name provided
in the `singleton_ports` config.

*Note: You should use the BF device port ID (SDK port ID) when reading and
writing P4Runtime entities and packets. In the future, we may support P4Runtime
port translation which would allow you to use the user-provide SDN port ID.*

#### Tofino specific configuration (experimental)

Some parts of the ChassisConfig do not apply to all platforms. These are
organized in the `VendorConfig` part of the configuration file. For Tofino, we
support the following extensions:

##### Port shaping

Port shaping can be configured on a port-by-port basis with limits in either
bits per second (bps) or packets per second (pps), by adding the relevant
entries in the `node_id_to_port_shaping_config` map of the `TofinoConfig`
message. The following snippet shows singleton port 1 being configured with a
byte (bps) shaping rate of 1 Gbit/s and a burst size of 16 KB:

```
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
  speed_bps: 40000000000
  config_params {
    admin_state: ADMIN_STATE_ENABLED
  }
  node: 1
}
vendor_config {
  tofino_config {
    node_id_to_port_shaping_config {
      key: 1  # node id reference
      value {
        per_port_shaping_configs {
          key: 1  # singleton port id reference
          value {
            byte_shaping {
              rate_bps: 1000000000 # 1G
              burst_bytes: 16384 # 2x MTU
            }
          }
        }
      }
    }
  }
}
```

##### Quality of Service (QoS)

Due to legal reasons we can't give a full description of the QoS model inside
the Tofino traffic manager here. Refer to the Intel docs "10k-AS1-002EA" and
"10k-UG8-002EA-FF-TM" available on the customer portal.

Example configuration:

```protobuf
vendor_config {
  tofino_config {
    node_id_to_qos_config {
      key: 1
      value {
        pool_configs {
          pool: INGRESS_APP_POOL_0
          pool_size: 30000
          enable_color_drop: true
          color_drop_limit_green: 30000
          color_drop_limit_yellow: 10000
          color_drop_limit_red: 5000
        }
        pool_configs {
          pool: INGRESS_APP_POOL_1
          pool_size: 30000
          enable_color_drop: true
          color_drop_limit_green: 30000
          color_drop_limit_yellow: 10000
          color_drop_limit_red: 5000
        }
        pool_configs {
          pool: EGRESS_APP_POOL_0
          pool_size: 30000
          enable_color_drop: true
          color_drop_limit_green: 30000
          color_drop_limit_yellow: 10000
          color_drop_limit_red: 5000
        }
        pool_configs {
          pool: EGRESS_APP_POOL_1
          pool_size: 30000
          enable_color_drop: true
          color_drop_limit_green: 30000
          color_drop_limit_yellow: 10000
          color_drop_limit_red: 5000
        }
        pool_color_drop_hysteresis_green: 20000
        pool_color_drop_hysteresis_yellow: 8000
        pool_color_drop_hysteresis_red: 4000
        ppg_configs {
          sdk_port: 260
          # Or SingletonPort ID
          # port: 1
          is_default_ppg: true
          minimum_guaranteed_cells: 200
          pool: INGRESS_APP_POOL_0
          base_use_limit: 400
          baf: BAF_80_PERCENT
          hysteresis: 50
          ingress_drop_limit: 4000
          icos_bitmap: 0xfd
        }
        ppg_configs {
          sdk_port: 260
          is_default_ppg: false
          minimum_guaranteed_cells: 200
          pool: INGRESS_APP_POOL_1
          base_use_limit: 400
          baf: BAF_80_PERCENT
          hysteresis: 50
          ingress_drop_limit: 4000
          icos_bitmap: 0x02
        }
        ppg_configs {
          sdk_port: 268
          is_default_ppg: true
          minimum_guaranteed_cells: 200
          pool: INGRESS_APP_POOL_0
          base_use_limit: 400
          baf: BAF_80_PERCENT
          hysteresis: 50
          ingress_drop_limit: 4000
          icos_bitmap: 0xfd
        }
        ppg_configs {
          sdk_port: 268
          is_default_ppg: false
          minimum_guaranteed_cells: 200
          pool: INGRESS_APP_POOL_1
          base_use_limit: 400
          baf: BAF_80_PERCENT
          hysteresis: 50
          ingress_drop_limit: 4000
          icos_bitmap: 0x02
        }
        queue_configs {
          sdk_port: 260
          queue_mapping {
            queue_id: 0
            priority: PRIO_0
            weight: 1
            minimum_guaranteed_cells: 100
            pool: EGRESS_APP_POOL_0
            base_use_limit: 200
            baf: BAF_80_PERCENT
            hysteresis: 50
            max_rate_bytes {
              rate_bps: 100000000
              burst_bytes: 9000
            }
            min_rate_bytes {
              rate_bps: 1000000
              burst_bytes: 4500
            }
          }
          queue_mapping {
            queue_id: 1
            priority: PRIO_1
            weight: 1
            minimum_guaranteed_cells: 100
            pool: EGRESS_APP_POOL_1
            base_use_limit: 200
            baf: BAF_80_PERCENT
            hysteresis: 50
            max_rate_bytes {
              rate_bps: 100000000
              burst_bytes: 9000
            }
            min_rate_bytes {
              rate_bps: 1000000
              burst_bytes: 4500
            }
          }
        }
        queue_configs {
          sdk_port: 268
          # Or SingletonPort ID
          # port: 1
          queue_mapping {
            queue_id: 0
            priority: PRIO_0
            weight: 1
            minimum_guaranteed_cells: 100
            pool: EGRESS_APP_POOL_0
            base_use_limit: 200
            baf: BAF_80_PERCENT
            hysteresis: 50
            max_rate_bytes {
              rate_bps: 100000000
              burst_bytes: 9000
            }
            min_rate_bytes {
              rate_bps: 1000000
              burst_bytes: 4500
            }
          }
          queue_mapping {
            queue_id: 1
            priority: PRIO_1
            weight: 1
            minimum_guaranteed_cells: 100
            pool: EGRESS_APP_POOL_1
            base_use_limit: 200
            baf: BAF_80_PERCENT
            hysteresis: 50
            max_rate_bytes {
              rate_bps: 100000000
              burst_bytes: 9000
            }
            min_rate_bytes {
              rate_bps: 1000000
              burst_bytes: 4500
            }
          }
        }
      }
    }
  }
}
```

### Running with BSP or on Tofino model

On some supported platforms the BSP-based implementation is chosen by default.
This selection can be overwritten with the `-bf_switchd_cfg` flag:

```bash
start-stratum.sh -bf_switchd_cfg=/usr/share/stratum/tofino_skip_p4.conf
```

### Running the binary in BSP-less mode

```bash
start-stratum.sh --bf_switchd_cfg=/usr/share/stratum/tofino_skip_p4_no_bsp.conf
```

The platform vendor can provide a JSON "port mapping" file (see
this [example](platforms/x86-64-accton-wedge100bf-32x-r0.json) for the Wedge
100bf-32x) and Stratum takes care of making the information to the SDE as needed.

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

### Running with BF CLI

You can enable the BF CLI by passing `-bf_switchd_background=false` as an
argument when starting Stratum. The CLI is disabled by default.

The CLI will start after Stratum and the SDE finish initialization. You may
need to press "Enter" a few times to see the prompt. Please refer to BF SDE
documentation for details on how to use the CLI.

### Access BF Shell via telnet (experimental)

When Stratum started, the BF Shell will be enabled by default. To access the BF
Shell, run `attach-bf-shell.sh` in the container:

```bash
docker exec -it [stratum container name or ID] attach-bf-shell.sh
```

This script will start a telnet session that connects to the BF Shell. Once you
have entered the BF Shell, type `ucli` to access the BF CLI.

> Since we do not support all features from the BF Shell, using commands other
  than `ucli` may cause Stratum crash.

To exit the BF CLI or the BF Shell, use `exit`. Note that using `Ctrl+C` will
end the BF Shell without closing the telnet session. To exit the telnet session,
press `Ctrl` and `]` to escape from the session and type `quit` to exit telnet.

### Experimental P4Runtime translation support

The `stratum_bfrt` target supports P4Runtime translation which helps to translate
between SDN port and the SDK port.

To enable this, you need to create a new port type in you P4 code and use this type
for match field and action parameter, for example:

```p4
@p4runtime_translation("tna/PortId_t", 32)
type bit<9> FabricPortId_t;
```

To enable it on Stratum, add `--experimental_enable_p4runtime_translation` flag
when starting Stratum.

Note that `stratum_bfrt` also follows the PSA port spec, below are reserved ports
when using `stratum_bfrt`:

- `0x00000000`: Unspecified port.
- `0xFFFFFFFD`: CPU port.
- `0xFFFFFF00` - `0xFFFFFF03`: Recirculation ports for pipeline 0 - 3.

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

### RESOURCE_EXHAUSTED when pushing pipeline

Stratum rejects a SetForwardingPipelineConfig request with a RESOURCE_EXHAUSTED
gRPC error, like this:

```
INFO:PTF runner:Sending P4 config
ERROR:PTF runner:Error during SetForwardingPipelineConfig
ERROR:PTF runner:<_InactiveRpcError of RPC that terminated with:
        status = StatusCode.RESOURCE_EXHAUSTED
        details = "to-be-sent initial metadata size exceeds peer limit"
        debug_error_string = "{"created":"@1607159813.061940445","description":"Error received from peer ipv4:127.0.0.1:28000","file":"src/core/lib/surface/call.cc","file_line":1056,"grpc_message":"to-be-sent initial metadata size exceeds peer limit","grpc_status":8}"
>
```

This error originates from the gRPC layer and can occur when the pipeline is
particularly large and does not fit in the [maximum receive message size](https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_builder.html#ab5c8a420f2acfc6fcea2f2210e9d426e).
Although we set a reasonable default, the value can be adjusted with Stratum's
`-grpc_max_recv_msg_size` flag.

### Checking the Switch or ASIC revision number

Some switch models and ASIC chips are updated over time, but the old devices
remain in circulation.
The following commands allow you to check the revision of your device.

For use with the `ucli` in a running Stratum instance:
```bash
# Check for part_revision_number and the codes A0 or B0.
efuse 0
pm sku -d
```

In a bash shell on the switch:
```bash
lspci -d 1d1c:
# 05:00.0 Unassigned class [ff00]: Device 1d1c:0010 (rev 10)
```

### SDE config mismatch and connection error

`connect failed. Error: Connection refused`

This error means that Stratum was started with the expectation of a BSP
(`-bf_switchd_cfg=...`), but no BSP was actually found. Check that you are
passing in the right config, depending on how you compiled the SDE:
`tofino_skip_p4.conf` vs. `tofino_skip_p4_no_bsp.conf `. Normally the
`start-stratum.sh` script will pick the right value.
