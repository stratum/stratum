<!--
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

gNMI tool
----

### Usage

```bash
bazel run //stratum/tools/gnmi:gnmi_cli -- --helpshort
```

### Examples

```
# To get port index (which is used when referring to a port in P4Runtime)
bazel run //stratum/tools/gnmi:gnmi_cli -- get /interfaces/interface[name=1/1/1]/state/ifindex

# To set port health indicator
bazel run //stratum/tools/gnmi:gnmi_cli -- set /interfaces/interface[name=1/1/1]/config/health-indicator --string-val GOOD

# To subscribe one sample of port operation status per second
bazel run //stratum/tools/gnmi:gnmi_cli -- sub-sample /interfaces/interface[name=1/1/1]/state/oper-status --interval 1000

# To push chassis config
bazel run //stratum/tools/gnmi:gnmi_cli -- --replace --bytes_val_file [chassis config file] set /
```
