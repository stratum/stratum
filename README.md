[Stratum - A P4Runtime based reference framework](https://github.com/opennetworkinglab/stratum.git)

Copyright 2018 Google LLC

# Documentation

# Overview

[P4Runtime](https://p4.org/p4-runtime) provides a flexible mechanism for
configuring the forwarding pipeline on a network switch.

[gNMI](https://github.com/openconfig/reference/tree/master/rpc/gnmi) is a
framework for network device management that uses gRPC as the transport
mechanism.

# Source code

This repository contains source code for a reference implementation of
the P4Runtime and gNMI services, that serves as the hardware abstraction layer
for a network switch. It has been successfully prototyped at Google, running on
production hardware on a data center network subsytem.

# Development environment

We provide a script to create a Docker development environment for Stratum. Run
`setup_dev_env.sh -h` for more information. Typical usage looks like this:

    ./setup_dev_env.sh --pull --mount-ssh --git-name <name> --git-email <email>

The script will build a Docker image using Dockerfile.dev and run a bash session
in it. This directory will be mounted in the Docker image and you will be able
to run git, edit code, and build Stratum / run tests using Bazel.
