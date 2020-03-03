<!--
Copyright 2019 NoviFlow, Inc.
Copyright 2019-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# Stratum Pipeline Examples

P4 Pipelines used for simple demonstrations of the `Stratum` software using `bmv2`.

Before proceeding with the instructions below, be sure to build the `stratum_bmv2` software switch using [these instructions](../hal/bin/bmv2/README.md).

In the instructions below, _stratum_dir_ and  _bmv2_install_dir_ represent the directories where `Stratum` and `bmv2` are installed.  Directory _bmv2_dir_ represents the directory where the `bmv2` repository was cloned. See [these instructions](../hal/bin/bmv2/README.md).

## Environment variables

Environment variables used throughout these instructions.

Variables used to build `stratum_bmv2` and described in [these instructions](../hal/bin/bmv2/README.md):
```
    $ export BMV2_INSTALL=<bmv2_install_dir>
    $ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$BMV2_INSTALL/lib
```
Variable required by test environment:
```
    $ export STRATUM_ROOT=<stratum_dir>
```
## Building the pipelines

The P4 code associated to a particular pipeline is located in directory `<stratum_dir>/stratum/pipelines/<pipeline_name>`.

1. Open a terminal.

2. Define the variables specified in [Environment variables](#environment-variables).

3. Use the command below to build a pipeline:
```
    $ cd <stratum_dir>
    $ bazel build //stratum/pipelines/<pipeline_name>:p4_<pipeline_name>
```
For instance, to build the `loopback` pipeline, use the following command:
```
    $ bazel build //stratum/pipelines/loopback:p4_loopback
```
__Note__: Building the pipelines requires P4 compiler [p4c](https://github.com/p4lang/p4c).  The first time P4 code is built, Bazel retrieves the [p4c](https://github.com/p4lang/p4c) source files and builds the compiler executables before building the pipeline.  The overall build process of that first pipeline takes some time to complete.  Subsequent pipeline builds will not require rebuilding [p4c](https://github.com/p4lang/p4c) and will be much faster.

## Run PTF tests with stratum_bmv2

Each pipeline includes a [Packet Test Framework](https://github.com/p4lang/ptf) test that can be executed on `stratum_bmv2`.  For a given pipeline, the PTF tests are located in the `./ptf` subdirectory.

1. In a terminal, define the variables specified in [Environment variables](#environment-variables).

2. Configure the Virtual Ethernet interfaces:
```
    $ sudo <bmv2_dir>/tools/veth_setup.sh
```
3. Execute the PTF tests using the commands below:
```
    $ cd <stratum_dir>
    $ bazel run //stratum/pipelines/<pipeline_name>:p4_<pipeline_name>_test
```
For instance, to run the PTF test for the `loopback` pipeline, use the following command:
```
    $ bazel run //stratum/pipelines/loopback:p4_loopback_test
```
This starts the `stratum_bmv2` binary and executes the tests defined in `<stratum_dir>/pipelines/loopback/ptf`.

__Note__: Similar to what happens the first time a pipeline is built, the first time a test is launched, dependencies are built before the test actually executes.  This increases the time the first test takes to complete.  Subsequent test executions will be faster.

## Run tests with stratum_bmv2 using Scapy

Tests can be executed outside of the [Packet Test Framework](https://github.com/p4lang/ptf) environment using Scapy.  To achieve this, we provide a method to configure pipelines in `stratum_bmv2` without running any tests.

The Stratum software includes a version of Scapy that can be used to send packets to the switch as shown below.  However, if [Scapy](https://scapy.readthedocs.io/en/latest/installation.html) is already installed on your system, you can use that instance to communicate with `stratum_bmv2`.

Manual testing provides an interresting level of visibility as `stratum_bmv2` logs are output to the terminal.

### Start stratum_bmv2

1. Open a new terminal and switch to the root user.

2. Define the following envionment variable:
```
    # export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:<bmv2_install_dir>/lib
```

3. Configure the Virtual Ethernet interfaces:
```
    # <bmv2_dir>/tools/veth_setup.sh
```
4. Start `stratum_bmv2`:
```
    # cd <stratum_dir>
    # ./bazel-bin/stratum/hal/bin/bmv2/stratum_bmv2 --device_id=1 --forwarding_pipeline_configs_file=/tmp/stratum-bmv2/config.txt --persistent_config_dir=/tmp/stratum-bmv2 --cpu_port=64 --external-stratum-urls=0.0.0.0:28000 0@veth1 1@veth3 2@veth5 3@veth7 4@veth9 5@veth11 6@veth13 7@veth15 --bmv2-log-level=trace
```
At this point the switch is started and ready to receive a pipeline.

### Configure the pipeline in stratum_bmv2:

1. In a different terminal, define the variables specified in [Environment variables](#environment-variables).

2. To configure a pipeline in `stratum_bmv2`, issue the following commands:
```
    $ cd <stratum_dir>
    $ bazel run //stratum/pipelines/<pipeline_name>:p4_<pipeline_name>_pipeline
```
For instance, use the following command to configure the `loopback` pipeline.
```
    $ bazel run //stratum/pipelines/loopback:p4_loopback_pipeline
```
__Note__: This particular pipeline does not require Match-Action table configuration, so as soon as `stratum_bmv2` has been successfully configured with this pipeline, it is ready to receive packets.

### Use Scapy to send packets to the switch

The commands below can be executed in the previous terminal or in a new terminal.

1. Start Scapy:

To use the version of Scapy that is included with Stratum, use the following command:
```
    $ bazel run //stratum/pipelines/ptf:scapy
```
If you'd rather use the version of Scapy installed on your system:
```
    $ sudo scapy
```
2. From the Scapy prompt, send a packet to `stratum_bmv2`:
```
    >>> pkt=Ether()/IP()/UDP()/'Packet payload'
    >>> sendp(pkt, iface='veth3')
    .
    Sent 1 packets.
    >>>
```
Packet processing in the pipeline can be observed in the logs that appear in the `stratum_bmv2` terminal.

3. Exit Scapy:
```
    >>> quit()
    $
```
