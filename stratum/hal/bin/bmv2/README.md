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

## Building the `stratum_bmv2` package

To build the Debian package, start the development docker container and run
Bazel:

```bash
setup_dev_env.sh  # You're inside the Docker container now
bazel build //stratum/hal/bin/bmv2:stratum_bmv2_deb
```

## Running the `stratum_bmv2` binary

As of now the `stratum_bmv2` binary can be run from Bazel or directly, if
installed as from a Debian package:

```bash
# Or just `stratum_bmv2`
./bazel-bin/stratum/hal/bin/bmv2/stratum_bmv2 \
    --chassis_config_file=/etc/stratum/chassis_config.pb.txt \
    --bmv2_log_level=debug
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
cp stratum/hal/bin/bmv2/update_config.py /tmp/ && \
[sudo] docker run -v /tmp:/tmp -w /tmp p4lang/pi ./update_config.py \
    --grpc-addr <YOUR_HOST_IP_ADDRESS>:9559 --json <prog>.json --p4info <prog>.proto.txt
```

You can use the loopback program under `stratum/pipelines/loopback/p4c-out/bmv2`
if you do not have your own P4 program.

## Notes

### P4Runtime canonical byte strings

P4Runtime defines a [canonical byte string representation](https://s3-us-west-2.amazonaws.com/p4runtime/docs/master/P4Runtime-Spec.html#sec-bytestrings)
for binary data in proto messages such as TableEntries and PacketIn/Outs. In
short, it requires that the binary strings must not contain redundant bytes,
 i.e., `\x00\xab` vs `\xab`.
 Stratum-bmv2 does not yet support canonical byte strings although support is planned for the near future. If using a p4runtime client that defaults to canonical bytestrings usage (e.g. `p4lang/p4runtime-sh`) make sure the legacy byte-padded format is used instead. For `p4lang/p4runtime-sh` this is:

 ```python
 P4Runtime sh >>> global_options["canonical_bytestrings"] = False
 ```

 if using the shell or:

 ```python
from p4runtime_sh.global_options import global_options
global_options["canonical_bytestrings"] = False
 ```

if using python directly.
