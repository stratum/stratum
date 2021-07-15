<!--
Copyright 2018 Barefoot Networks, Inc.
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# Stratum on Barefoot Tofino based switches

Stratum is supported on Barefoot Tofino based switches. There are
several documents to help get you understand how to build, run, and test
these targets.

There are two different Stratum targets for Tofino:

`stratum_bfrt` is a newer target based on Barefoot's BfRt C++ API. This target
is newer, actively used and tested, and gives users access to additional Tofino
features. It is the recommended target for most users.

`stratum_bf` is based on the [PI](https://github.com/p4lang/PI) C++ API and is
the legacy target.

The easiest way to get started is with the pre-built Docker images. See
the "Running" guide below for more details.

## Guides

**[Building Stratum for Barefoot Tofino based switches](./README.build.md)**
provides instructions for users that want to build their own Stratum images,
including those who want to customize the BF SDE, Linux kernel, or Stratum
code.

**[Running Stratum on a Barefoot Tofino based switch](./README.run.md)**
provides instructions for users who want to get started with a prebuilt
image or one that they have built themselves.

**[P4Runtime p4_device_config formats](./README.pipeline.md)**
explains the supported formats in Stratum for Barefoot Tofino based switches.

**[Testing Stratum on a Barefoot Tofino based switch](./README.test.md)**
provides some examples of how to interact with Stratum running on a
Barefoot Tofino based switch.
