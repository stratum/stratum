<!--
Copyright 2018 Google LLC
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

![stratum logo](stratum/docs/images/stratum-logo.png)

# Stratum - Enabling the era of next-generation SDN

Copyright 2018 Google LLC <br/>
Copyright 2018-present Open Networking Foundation

Stratum is an open source silicon-independent switch operating system for software defined networks. It is building an open, minimal production-ready distribution for white box switches. Stratum exposes a set of next-generation SDN interfaces including P4Runtime and OpenConfig, enabling interchangeability of forwarding devices and programmability of forwarding behaviors. Current support includes Barefoot Tofino and Broadcom Tomahawk devices, as well as the bmv2 software switch.

Build status (master): [![CircleCI](https://circleci.com/gh/stratum/stratum/tree/master.svg?style=svg)](https://circleci.com/gh/stratum/stratum/tree/master)
[![codecov](https://codecov.io/gh/stratum/stratum/branch/master/graph/badge.svg)](https://codecov.io/gh/stratum/stratum)

# Documentation

## Running Stratum

[Broadcom](stratum/hal/bin/bcm/standalone/README.md)

[Tofino](stratum/hal/bin/barefoot/README.md)

[bmv2](stratum/hal/bin/bmv2/README.md)

[dummy](stratum/hal/bin/dummy/README.md)

[NP4 Intel](stratum/hal/bin/np4intel/docker/README.md)

[Stratum inside Docker](TODO)

## Other Topics

[p4c-fpm](stratum/p4c_backends/README.md)

[PHAL](stratum/docs/phal.md)

[PHAL Development Tutorial](stratum/docs/phal_development_tutorial.md)

[PHAL DB Configurators](stratum/docs/configurators.md)

[gNMI](stratum/docs/gnmi/README.md)

[cdlang](stratum/testing/cdlang/README.md)

[testvectors](stratum/public/proto/tv.proto)

[SSL/TLS support](stratum/lib/security/README.md)

[TAI](stratum/docs/tai/README.md)

# Component Overview

![img](stratum/docs/images/stratum_architecture.png)

[P4Runtime](https://p4.org/p4-runtime) provides a flexible mechanism for
configuring the forwarding pipeline on a network switch.

[gNMI](https://github.com/openconfig/reference/tree/master/rpc/gnmi) is a
framework for network device management that uses gRPC as the transport
mechanism.

[SDKLT](https://github.com/Broadcom-Network-Switching-Software/SDKLT) is used
to program fixed-pipeline switches using the Tomahawk chip from Broadcom.

# Supported Devices

## Tofino

- Delta AG9064v1

- Edgecore Wedge 100BF 32x

- Edgecore Wedge 100BF 65x

- Inventec D5254

- Stordis BF6064X

- Stordis BF2556X

## Broadcom

- Edgecore AS7712-32X

- Dell Z9100

- Inventec D7032

- QCT QuantaMesh T7032-IX1

## Software Switches

- bmv2

- dummy

# Source code

This repository contains source code for a reference implementation of
the P4Runtime and gNMI services, that serves as the hardware abstraction layer
for a network switch. It has been successfully prototyped at Google, running on
production hardware on a data center network subsystem.

# Development environment

We provide a script to create a Docker development environment for Stratum.

To start the container, run the following in your `stratum` directory:

    ./setup_dev_env.sh

The script will build a Docker image using Dockerfile.dev and run a bash session
in it. This directory will be mounted in the Docker image and you will be able
to run git, edit code, and build Stratum / run tests using Bazel.

For more information, you can see a complete list of arguments by running:
`setup_dev_env.sh -h`

If you are using Docker on Linux, make sure that you can use Docker as a
non-root user, otherwise you will not be able to run setup_dev_env.sh:

    sudo usermod -aG docker $USER

