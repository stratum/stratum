<!--
Copyright 2018 Barefoot Networks, Inc.
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# Testing Stratum on a Barefoot Tofino based switch

## Using p4runtime-shell

[p4runtime-shell](https://github.com/p4lang/p4runtime-shell) is an interactive
Python shell for P4Runtime. While it can also be used to set the P4 forwarding
pipeline and issue P4Runtime `Write` RPCs, it especially comes in handy when you
want to read the forwarding state of the switch.

To start a shell session, you can use (requires Docker):
```
./p4runtime-sh-docker --grpc-addr <Stratum IP>:9559 --device-id 1 --election-id 0,1
```

Refer to the [p4runtime-shell](https://github.com/p4lang/p4runtime-shell)
documentation for more information.

## Testing gNMI

See [gNMI CLI](/stratum/tools/gnmi/README.md)

