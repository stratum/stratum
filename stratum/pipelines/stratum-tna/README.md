<!--
Copyright 2021-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# Stratum_tna P4 pipeline

Stratum_tna is an example pipeline for Tofino-enabled switches. It is used for
testing and demonstration purposes.

## Building

- Set `SDE_INSTALL` env variable
- `bazel run //stratum/pipelines/stratum-tna:build`
- `bazel run //stratum/hal/bin/barefoot:bf_pipeline_builder -- -p4c_conf_file=bazel-bin/stratum/pipelines/stratum-tna/stratum_tna.conf -bf_pipeline_config_binary_file=bazel-bin/stratum/pipelines/stratum-tna/build_out/device_config.pb.bin`

## Testing

- `bazel run //stratum/pipelines/stratum-tna:stratum_tna_test -- --grpc_addr=<switch_address> --p4_info_file=$(pwd)/bazel-bin/stratum/pipelines/stratum-tna/build_out/p4info.pb.txt --p4_pipeline_config_file=$(pwd)/bazel-bin/stratum/pipelines/stratum-tna/build_out/device_config.pb.bin`
