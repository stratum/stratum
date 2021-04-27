<!--
Copyright 2021-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# Stratum_tna P4 pipeline

Stratum_tna is an example pipeline for Tofino-enabled switches. It is used for
testing and demonstration purposes.

## Building

- Set `SDE_INSTALL` env variable
- `bazel build //stratum/pipelines/stratum-tna:stratum_tna`

## Testing

- `cd stratum/pipelines/stratum-tna/; bazel build //stratum/pipelines/stratum-tna:stratum_tna; prevd`
- `bazel run //stratum/pipelines/stratum-tna:stratum_tna_test -- --grpc_addr=<switch_address> --p4_info_file=$(pwd)/stratum/pipelines/stratum-tna/build/p4info.pb.txt --p4_pipeline_config_file=$(pwd)/stratum/pipelines/stratum-tna/build/device_config.pb.bin`
