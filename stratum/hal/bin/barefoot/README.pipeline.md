<!--
Copyright 2018 Barefoot Networks, Inc.
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# P4Runtime p4_device_config formats

Stratum-bfrt uses a protobuf based format ([bf.proto](/stratum/hal/lib/barefoot/bf.proto))
to push a pipeline over P4Runtime for Barefoot Tofino devices. Use the
`BfPipelineBuilder` tool to generate it.

For legacy reasons, Stratum-bf supports both the older binary packing used by
[PI](https://github.com/p4lang/PI) and the new protobuf based format also
supported in Stratum-bfrt.

## Stratum BfPipelineConfig format and the BfPipelineBuilder

You can use the device config builder to generate the protobuf based format
directly from the `bf-p4c` compiler output:

```bash
bazel run //stratum/hal/bin/barefoot:bf_pipeline_builder -- \
    -p4c_conf_file=</path/to/bf-p4c/compiler/output.conf> \
    -bf_pipeline_config_binary_file=$PWD/device_config.pb.bin
```

The tool is also available as part of the Stratum-tools [Docker image](https://hub.docker.com/repository/docker/stratumproject/stratum-tools):

```bash
cd </path/to/bf-p4c/compiler/output>
docker run --rm -v $PWD:$PWD -w $PWD stratumproject/stratum-tools:latest \
    bf_pipeline_builder \
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
