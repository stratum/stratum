<!--
Copyright 2018 Barefoot Networks, Inc.
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# Running Stratum with the P4.org bmv2 software switch

**Note:** Stratum bmv2 is also distributed with the Stratum-enabled
Mininet Docker image. Refer to the [Mininet](/tools/mininet/README.md) README
for further details.

The `stratum_bmv2` binary is a standalone executable which includes:
1. a Stratum implementation for bmv2
2. the `v1model` datapath

You can build the Debian package yourself or you can download the most
recent version from the
[Stratum releases page](https://github.com/stratum/stratum/releases).

## Building the `stratum_bmv2` package

To build the Debian package, start the development docker container and run
Bazel:

```bash
setup_dev_env.sh  # You're inside the Docker container now
bazel build //stratum/hal/bin/bmv2:stratum_bmv2_deb
```

If you want to run `stratum_bmv2` outside of the container, you'll need to
copy it into a shared volume. For example, `/stratum` is shared by default
in the build container.

```bash
cp -f /stratum/bazel-bin/stratum/hal/bin/bmv2/stratum_bmv2_deb.deb /stratum/
```

## Running the `stratum_bmv2` binary

The `stratum_bmv2` binary can be run directly after installing the
Debian package:

```bash
[sudo] apt-get update
[sudo] apt-get install -y --reinstall ./stratum_bmv2_deb.deb
stratum_bmv2 \
    -chassis_config_file=/etc/stratum/chassis_config.pb.txt \
    -bmv2_log_level=debug
```

You can ignore the following error, we are working on fixing it:
```
E0808 17:57:36.513559 29298 utils.cc:120] StratumErrorSpace::ERR_FILE_NOT_FOUND:  not found.
E0808 17:57:36.513905 29298 utils.cc:76] Return Error: ReadFileToString(filename, &text) failed with StratumErrorSpace::ERR_FILE_NOT_FOUND:  not found.
W0808 17:57:36.513913 29298 config_monitoring_service.cc:106] No saved chassis config found in . This is normal when the switch is just installed.
```

For a sample `chassis_config.pb.txt` file, see chassis_config.pb.txt in this
directory. For each singleton port, use the Linux interface name as the `name`
and set the `admin_state` to `ADMIN_STATE_ENABLED`.

Assigning interfaces to bmv2 requires the `stratum_bmv2` binary to have the
`CAP_NET_RAW` capability. Based on your Linux distribution and the location of
the binary, you may be able to add the capability to the binary with `setcap`
and run it as an unprivileged user. You can also simply run the `stratum_bmv2`
as root.

As a basic test, you can run the following commands. It will start a P4Runtime
client in a Docker image and perform a `SetForwardingPipelineConfig` RPC (which
pushes a new P4 data plane to bmv2). You will need a bmv2 JSON file and a P4Info
Protobuf text file, which you can obtain by compiling your P4 program with the
[p4c](https://github.com/p4lang/p4c) compiler.
```
# compile P4 program (skip if you already have the bmv2 JSON file and the P4Info
# text file)
p4c -b bmv2 -a v1model -o /tmp/ --p4runtime-format text --p4runtime-file /tmp/<prog>.proto.txt <prog>.p4
# run P4Runtime client
p4_pipeline_pusher --grpc_addr=<YOUR_HOST_IP_ADDRESS>:9559 --p4_info_file=<prog>.proto.txt --p4_pipeline_config_file=<prog>.json
```

<!--
FIXME(bocon): Update to use the P4RT shell
-->

You can use the loopback program under `stratum/pipelines/loopback/p4c-out/bmv2`
if you do not have your own P4 program.

## Developer builds of `stratum_bmv2`

The following steps are for developers who want to do fast, incremental builds of
`stratum_bmv2` and then run them in the development environment.

### Setup

Start your development environment container with priveledged mode in the host
network namespace. This will allow you to create new virtual network interfaces
(veth pairs) and interact with them.

```bash
./setup_dev_env.sh -- --privileged --network=host
```

Next, create some virtual network interfaces (veth pairs) for `stratum_bmv2`.

```bash
sudo veth_setup.sh
```

When you are finished, you can remove the interfaces.
```bash
sudo veth_teardown.sh
```

If you don't have the [`veth_setup.sh`](https://github.com/p4lang/behavioral-model/blob/main/tools/veth_setup.sh) command installed, you can
add interfaces manually:
```bash
sudo ip link add name veth0 type veth peer name veth1
sudo ip link add name veth2 type veth peer name veth3
sudo ip link set dev veth0 up
sudo ip link set dev veth1 up
sudo ip link set dev veth2 up
sudo ip link set dev veth3 up
```

By default, Stratum will bind `veth0` as port 1 and `veth2` as port 2.
More generally, Stratum will bind port `X` to `veth[(X-1) * 2]`.

You can send and receive packets to/from port 1 using `veth1` and
to/from port 2 using `veth3`. More generally, you can send and receive
packets for Stratum port `X` using `veth[(X-1) * 2 + 1]`.

This port mapping is a little confusing, but Statum just follows the
convention from BMv2. If you choose to use a different naming scheme,
be sure to update the port `name` in the chassis config.

### Building and running `stratum_bmv2` for development

You can build and run the Stratum binary directly using the following
Bazel command:

```bash
export BMV2_DIR=$(bazel info workspace)/stratum/hal/bin/bmv2
bazel build //stratum/hal/bin/bmv2:stratum_bmv2
sudo bazel-bin/stratum/hal/bin/bmv2/stratum_bmv2 \
    -persistent_config_dir=/tmp/ \
    -chassis_config_file=${BMV2_DIR}/chassis_config.pb.txt \
    -initial_pipeline=${BMV2_DIR}/dummy.json \
    -forwarding_pipeline_configs_file=/tmp/bmv2_pipeline_cfg \
    -bmv2_log_level=debug
```

Note: You should build without `sudo` to preserve the right permissions
in your Bazel cache, and then run with `sudo` because you need
`CAP_NET_RAW` to bind the Linux interfaces to send and receive packets.
We may figure out how to get the Linux capabilities right in Bazel in
the future to simplify this.

<!--
FIXME(bocon): try to merge the commands by giving `stratum_bmv2`
              permission to bind interfaces
-->

### Sending a test packet

There are a lot of ways to generate test packets.

As an example, you can send a test ping packet to port 1 of BMv2, run
the following from another terminal:

```bash
docker run --rm --network host stratum-dev \
    ping -I veth1 -c 1 192.168.1.1
```

To send packets to port 2, replace `veth1` with `veth3`

### Running PTF tests

For more details on running PTF tests, see the
[Pipelines README](../../../pipelines/README.md).

Note: You will likely need to add additional veth pairs for the tests to
pass and then update the chassis config file with the additional ports.

## Troubleshooting

### Mismatched ABI Versioning

The following log means that the version of PI used in BMv2 and Stratum
differs. To fix this, update BMv2. This can usually be solved by pulling
the lastest `stratumproject/build:build` image.

```
$ bazel-bin/stratum/hal/bin/bmv2/stratum_bmv2
stratum_bmv2: external/com_github_p4lang_PI/src/pi.c:160: pi_init: Assertion `abi_version == PI_ABI_VERSION && "PI ABI version mismatch"' failed.
*** Aborted at 1627441551 (unix time) try "date -d @1627441551" if you are using GNU date ***
PC: @                0x0 (unknown)
*** SIGABRT (@0x1f500000109) received by PID 265 (TID 0x7f6abf56bf00) from PID 265; stack trace: ***
    @     0x7f6abdcb80e0 (unknown)
    @     0x7f6abd723fff gsignal
    @     0x7f6abd72542a abort
    @     0x7f6abd71ce67 (unknown)
    @     0x7f6abd71cf12 __assert_fail
    @     0x55721ba8837b pi_init
    @     0x55721b9d953d pi::fe::proto::DeviceMgrImp::init()
    @     0x55721b9ca91c pi::fe::proto::DeviceMgr::init()
    @     0x55721b2ba6fc stratum::hal::bmv2::Main()
    @     0x55721b2bb342 main
    @     0x7f6abd7112e1 __libc_start_main
    @     0x55721b2b98ea _start
    @                0x0 (unknown)
Aborted
```
