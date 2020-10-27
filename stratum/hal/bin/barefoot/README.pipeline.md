<!--
Copyright 2018 Barefoot Networks, Inc.
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# P4Runtime p4_device_config formats

Stratum supports a few different device configuration formats for pushing the P4
pipeline over P4Runtime for Barefoot Tofino devices, including
the older binary packing used by [PI](https://github.com/p4lang/PI)
and a newer more flexible protobuf based format ([bf.proto](stratum/hal/lib/barefoot/bf.proto)).

*Note: The older PI format does not work with `stratum_bfrt`.*

## Stratum BfPipelineConfig format and the BfPipelineBuilder

You can use the device config builder to generate the protobuf based format:

```bash
bazel run //stratum/hal/bin/barefoot:bf_pipeline_builder -- \
    -p4c_conf_file=</path/to/bf-p4c/compiler/output.conf> \
    -bf_pipeline_config_binary_file=$PWD/device_config.pb.bin
```

The tool is also available as a [Docker image](https://hub.docker.com/repository/docker/stratumproject/stratum-bf-pipeline-builder):

```bash
cd </path/to/bf-p4c/compiler/output>
docker run --rm -v $PWD:$PWD stratumproject/stratum-bf-pipeline-builder:latest \
    -p4c_conf_file=./<output.conf> \
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

The format accepts a tar archive of the `bf-p4c` compiler output:

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
