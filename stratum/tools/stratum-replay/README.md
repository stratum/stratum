<!--
Copyright 2020-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

P4Runtime write request replay tool
====

This tool replay P4Runtime write requests to a Stratum device with a given
Stratum P4Runtime write request log.

# Usage:

`stratum-replay [options] [p4runtime write log file]`

# Options:

```
-device_id: The device ID (default: 1)
-election_id: Election ID (high,low) for abstraction update (default: "0,1")
-grpc_addr: Stratum gRPC address (default: "127.0.0.1:9339")
-p4info: The P4Info file (default: "p4info.pb.txt")
-pipeline_cfg: The pipeline config file (default: "pipeline.pb.bin")
-ca_cert: CA certificate(optional), will use insecure credential if empty (default: "")
-client_cert: Client certificate (optional) (default: "")
-client_key: Client key (optional) (default: "")
```
