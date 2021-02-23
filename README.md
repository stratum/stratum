<!--
Copyright 2018 Google LLC
Copyright 2018-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

<p align="center">
  <img alt="Stratum Logo" src="https://raw.githubusercontent.com/stratum/stratum/main/stratum/docs/images/stratum-logo.png">
</p>

# Stratum - Enabling the era of next-generation SDN

Stratum is an open source silicon-independent switch operating system for software defined networks. It is building an open, minimal production-ready distribution for white box switches. Stratum exposes a set of next-generation SDN interfaces including P4Runtime and OpenConfig, enabling interchangeability of forwarding devices and programmability of forwarding behaviors. Current support includes Barefoot Tofino and Broadcom Tomahawk devices, as well as the bmv2 software switch.

Build status (main): [![CircleCI](https://circleci.com/gh/stratum/stratum/tree/main.svg?style=svg)](https://circleci.com/gh/stratum/stratum/tree/main)
[![codecov](https://codecov.io/gh/stratum/stratum/branch/main/graph/badge.svg)](https://codecov.io/gh/stratum/stratum)

# Documentation

## Getting Started with SDN / P4 / Stratum

If you're completly new to the world of P4 and are looking for step-by-step instructions with batteries included, the [NG-SDN](https://github.com/opennetworkinglab/ngsdn-tutorial) tutorial is a good starting point. For material more focused on P4 as a language, have a look at the [p4lang](https://github.com/p4lang/tutorials) tutorials. Finally, there is the "Software-Defined Networks: A Systems Approach" book ([HTML](https://sdn.systemsapproach.org/)) for comprehensive coverage of SDN. Also see the chapter about [Stratum](https://sdn.systemsapproach.org/stratum.html#thin-switch-os) in it.

## Table of Contents
### Development

- [Code of Conduct](/CODE_OF_CONDUCT.md)
- [Contribution Guidelines](/CONTRIBUTING.md)
- [IDE Setup](https://github.com/stratum/stratum/wiki/IDE-setup-for-development)
- [Release Plan](stratum/stratum/docs/release_plan.md)
- [PHAL Development Tutorial](/stratum/docs/phal_development_tutorial.md)
- [Stratum Configurators Design Document](/stratum/docs/configurators.md)
- [Stratum PHAL Overview (Physical Hardware Access Layer)](/stratum/docs/phal.md)

### Platforms

- [Tofino](/stratum/hal/bin/barefoot/README.md)
  - [Building Stratum for Barefoot Tofino based switches](/stratum/hal/bin/barefoot/README.build.md)
  - [Running Stratum on a Barefoot Tofino based switch](/stratum/hal/bin/barefoot/README.run.md)
  - [Testing Stratum on a Barefoot Tofino based switch](/stratum/hal/bin/barefoot/README.test.md)
  - [P4Runtime p4_device_config formats](/stratum/hal/bin/barefoot/README.pipeline.md)
- [Broadcom](/stratum/hal/bin/bcm/standalone/README.md)
  - [How to Add a Dataplane Feature to Stratum (BCM)](/stratum/docs/extenting_stratum_bcm.md)
- [Intel NP4](/stratum/hal/bin/np4intel/docker/README.md)
- [bmv2](/stratum/hal/bin/bmv2/README.md)
- [Dummy Switch](/stratum/hal/bin/dummy/README.md)

### Tools

- [gNMI CLI Tool](/stratum/tools/gnmi/README.md)
- [Tofino Pipeline Builder](/stratum/hal/bin/barefoot/README.pipeline.md#stratum-bfpipelineconfig-format-and-the-bfpipelinebuilder)
- [Stratum-Enabled Mininet](/tools/mininet/README.md)
- [P4Runtime write request replay tool](/stratum/tools/stratum-replay/README.md)
- [ChassisConfig Migrator](/stratum/hal/config/chassis_config_migrator.cc)
- [PHAL CLI Tool](/stratum/hal/lib/phal/phal_cli.cc)
- [ONLP CLI Tool](/stratum/hal/lib/phal/onlp/onlp_cli.cc)
- [ONLP PHAL CLI Tool](/stratum/hal/lib/phal/onlp/onlp_phal_cli.cc)

### Other Documentation

- [Installation guide: ONOS + Stratum + Barefoot Tofino switch](/stratum/docs/setup_guide_barefoot_tofino_onos.md)
- [Trellis + Stratum example](/tools/mininet/examples/trellis/README.md)
- [Continuous Certification Program (CCP)](https://wiki.opennetworking.org/display/COM/Stratum+Continuous+Certification+Program)
- [Community Channels](https://github.com/stratum/stratum/wiki/Discuss)
- [Talks](https://github.com/stratum/stratum/wiki/Talks)
- [Test Vectors](https://github.com/stratum/testvectors/blob/master/docs/testvectors_overview.md)
- [Stratum example pipelines](/stratum/pipelines/README.md)
  - [Main FPM pipeline](/stratum/pipelines/main/README.md)
- [Supported gNMI paths](/stratum/docs/gnmi/supported-paths.md)
- [Config Monitoring Service](/stratum/docs/gnmi/README.md)
- [Transponder Abstraction Interface (TAI)](/stratum/docs/tai/README.md)
- [P4C backend](/stratum/p4c_backends/README.md)
- [CDLang gNMI Compliance Definition Language](/stratum/testing/cdlang/g3doc/cdlang.md)
- [gNMI/OpenConfig Contract Definition Language tools](/stratum/testing/cdlang/README.md)
- [gNMI Test Scenarios](/stratum/testing/scenarios/README.md)
- [SSL/TLS Support and Security](/stratum/lib/security/README.md)

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

- APS Networks BF2556X-1T
- APS Networks BF6064X-T
- Delta AG9064v1
- Edgecore Wedge100BF-32QS
- Edgecore Wedge100BF-32X
- Edgecore Wedge100BF-65X
- Inventec D5254
- Inventec D5264Q28B
- Inventec D10056
- Inventec D10064
- Netberg Aurora 610
- Netberg Aurora 710
- Netberg Aurora 750

## Broadcom

- Tomahawk
    - Edgecore AS7712-32X
    - Edgecore AS7716-24XC Cassini
    - Dell Z9100
    - Inventec D7032
    - QCT QuantaMesh T7032-IX1
- Tomahawk+
    - Edgecore AS7716-24SC Cassini
    - Inventec D7054
- Tomahawk2
- Tomahawk3
- Trident2
    - Edgecore AS6712
    - Inventec D6254

## Smart NIC

- Intel PAC N3000

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
