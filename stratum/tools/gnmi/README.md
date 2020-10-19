<!--
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

gNMI tool
----

### Usage

```
usage: gnmi-cli [--help] [Options] {get,set,cap,del,sub-onchange,sub-sample} path

Basic gNMI CLI

positional arguments:
  {get,set,cap,del,sub-onchange,sub-sample}         gNMI command
  path                                              gNMI path

optional arguments:
  --help            show this help message and exit
  --grpc_addr GRPC_ADDR    gNMI server address
  --bool_val BOOL_VAL      [SetRequest only] Set boolean value
  --int_val INT_VAL        [SetRequest only] Set int value (64-bit)
  --uint_val UINT_VAL      [SetRequest only] Set uint value (64-bit)
  --string_val STRING_VAL  [SetRequest only] Set string value
  --float_val FLOAT_VAL    [SetRequest only] Set float value
  --interval INTERVAL      [Sample subscribe only] Sample subscribe poll interval in ms
  --replace                [SetRequest only] Use replace instead of update
  --get-type               [GetRequest only] Use specific data type for get request (ALL,CONFIG,STATE,OPERATIONAL)
  --ca-cert                CA certificate
  --client-cert            gRPC Client certificate
  --client-key             gRPC Client key
```

### Examples

```
# To get port index
bazel run //stratum/tools/gnmi:gnmi-cli -- get /interfaces/interface[name=1/1/1]/state/ifindex

# To set port health indicator
bazel run //stratum/tools/gnmi:gnmi-cli -- set /interfaces/interface[name=1/1/1]/config/health-indicator --string-val GOOD

# To subscribe one sample of port operation status per second
bazel run //stratum/tools/gnmi:gnmi-cli -- sub-sample /interfaces/interface[name=1/1/1]/state/oper-status --interval 1000

# To push chassis config
bazel run //stratum/tools/gnmi:gnmi-cli -- --replace --bytes_val_file [chassis config file] set /
```
